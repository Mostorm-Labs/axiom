import { createHash } from "node:crypto";
import { existsSync } from "node:fs";
import { readFile, realpath, stat, writeFile } from "node:fs/promises";
import { dirname, isAbsolute, relative, resolve } from "node:path";
import { createG0GateReport, type G0GateReportInput } from "@axiom/platform-harness-runner";
import { ExitCode } from "../exit_codes.js";

const valueAfter = (args: string[], flag: string): string | null => {
  const index = args.indexOf(flag);
  return index >= 0 && args[index + 1] ? args[index + 1] : null;
};

const sha256 = /^[0-9a-f]{64}$/u;

function repositoryRoot(): string {
  if (process.env.AXIOM_REPOSITORY_ROOT) return resolve(process.env.AXIOM_REPOSITORY_ROOT);
  let current = resolve(process.cwd());
  while (true) {
    try {
      // The workspace manifest is deliberately used instead of .git so this also works in worktrees.
      requireWorkspaceMarker(current);
      return current;
    } catch {
      const parent = dirname(current);
      if (parent === current) return resolve(process.cwd());
      current = parent;
    }
  }
}

function requireWorkspaceMarker(root: string): void {
  // This synchronous existence check keeps root discovery independent of the input files.
  // The actual file is opened asynchronously by verifyBoundFiles.
  const marker = resolve(root, "verification/workspace.json");
  if (!existsSync(marker)) throw new Error("not a repository root");
}

function safeRelativePath(value: unknown, label: string): string {
  if (typeof value !== "string" || value.length === 0 || value.includes("\\") || value.includes("\0") || isAbsolute(value)) {
    throw new Error(`${label}: unsafe relative path`);
  }
  const parts = value.split("/");
  if (parts.some((part) => part === "" || part === "." || part === "..") || /^[A-Za-z]:$/u.test(parts[0] ?? "")) throw new Error(`${label}: unsafe relative path`);
  return value;
}

async function readBoundFile(root: string, value: unknown, label: string): Promise<Buffer> {
  const path = safeRelativePath(value, label);
  const rootReal = await realpath(root);
  const candidate = resolve(rootReal, path);
  const actual = await realpath(candidate).catch(() => { throw new Error(`${label}: file is missing`); });
  const escaped = relative(rootReal, actual);
  if (!escaped || escaped === "" || escaped.startsWith("..") || isAbsolute(escaped)) throw new Error(`${label}: path escapes repository root`);
  const details = await stat(actual);
  if (!details.isFile()) throw new Error(`${label}: path is not a regular file`);
  return readFile(actual);
}

async function verifyBoundFiles(root: string, lineage: unknown, artifacts: unknown): Promise<void> {
  if (!lineage || typeof lineage !== "object" || !Array.isArray((lineage as { tasks?: unknown }).tasks)) throw new Error("lineage.tasks must be an array");
  for (const task of (lineage as { tasks: Array<{ taskId?: unknown; evidencePath?: unknown; evidenceSha256?: unknown }> }).tasks) {
    if (!sha256.test(String(task.evidenceSha256 ?? ""))) throw new Error(`${String(task.taskId)}: invalid evidence hash`);
    const bytes = await readBoundFile(root, task.evidencePath, `${String(task.taskId)} evidencePath`);
    const digest = createHash("sha256").update(bytes).digest("hex");
    if (digest !== task.evidenceSha256) throw new Error(`${String(task.taskId)}: evidence hash mismatch`);
  }
  const platforms = (lineage as { platforms?: Array<{ subject?: unknown; evidencePath?: unknown }> }).platforms;
  if (!Array.isArray(platforms) || platforms.length === 0) throw new Error("lineage.platforms must be a non-empty array");
  for (const platform of platforms) await readBoundFile(root, platform.evidencePath, `${String(platform.subject)} platform evidencePath`);
  if (!Array.isArray(artifacts)) throw new Error("artifacts must be an array");
  for (const artifact of artifacts as Array<{ path?: unknown; bytes?: unknown; sha256?: unknown }>) {
    if (!sha256.test(String(artifact.sha256 ?? ""))) throw new Error(`${String(artifact.path)}: invalid artifact hash`);
    const bytes = await readBoundFile(root, artifact.path, `artifact ${String(artifact.path)}`);
    if (bytes.byteLength !== artifact.bytes) throw new Error(`${String(artifact.path)}: artifact byte count mismatch`);
    const digest = createHash("sha256").update(bytes).digest("hex");
    if (digest !== artifact.sha256) throw new Error(`${String(artifact.path)}: artifact hash mismatch`);
  }
}

export async function gateReport(args: string[]): Promise<number> {
  const sourceCommit = valueAfter(args, "--source-commit");
  const branch = valueAfter(args, "--branch");
  const lineagePath = valueAfter(args, "--lineage");
  const hostedPath = valueAfter(args, "--hosted");
  const artifactsPath = valueAfter(args, "--artifacts");
  const output = valueAfter(args, "--output");
  const root = valueAfter(args, "--repository-root") ?? repositoryRoot();
  if (!sourceCommit || !branch || !lineagePath || !hostedPath || !artifactsPath || !output) return ExitCode.INVALID_ARGUMENTS;
  try {
    const [lineage, hosted, artifacts] = await Promise.all([
      readFile(resolve(process.cwd(), lineagePath), "utf8").then((value) => JSON.parse(value)),
      readFile(resolve(process.cwd(), hostedPath), "utf8").then((value) => JSON.parse(value)),
      readFile(resolve(process.cwd(), artifactsPath), "utf8").then((value) => JSON.parse(value)),
    ]);
    await verifyBoundFiles(root, lineage, artifacts);
    const input: G0GateReportInput = {
      sourceCommit, branch, schemaVersion: "axiom-gate-report-v1", generatorVersion: "0.1.0",
      corpus: lineage.corpus, taskLineage: lineage.tasks, platforms: lineage.platforms, hosted, artifacts,
    };
    const report = createG0GateReport(input);
    await writeFile(resolve(process.cwd(), output), `${JSON.stringify(report, null, 2)}\n`);
    return ExitCode.SUCCESS;
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    return ExitCode.INVALID_EVIDENCE;
  }
}
