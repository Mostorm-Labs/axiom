import { createHash } from "node:crypto";
import { mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { isAbsolute, join, relative, resolve, sep } from "node:path";
import { spawnSync } from "node:child_process";
import { ExitCode } from "../exit_codes.js";
import { validate } from "./validate.js";

const suiteId = "protocol-seed-v0.1";
const boundaryNames = {
  "in-process": "IN_PROCESS",
  "serialized-loopback": "OUT_OF_PROCESS",
} as const;
type Boundary = (typeof boundaryNames)[keyof typeof boundaryNames];

interface ProtocolMetaResult {
  format: string;
  formatVersion: number;
  vectorId: string;
  boundaryMode: Boundary;
  status: "PASS" | "FAIL" | "REJECTED";
  checks: unknown[];
  firstDivergence: object | null;
}
interface ProtocolSuite { id: string; vectorRefs: string[]; }
interface ParsedArguments { suite: string; boundaries: Boundary[]; output: string; }

function treeHash(path: string): string {
  const hash = createHash("sha256");
  const visit = (current: string, prefix: string): void => {
    for (const name of readdirSync(current).sort()) {
      const full = join(current, name);
      const itemPath = join(prefix, name);
      const stat = statSync(full);
      if (stat.isDirectory()) visit(full, itemPath);
      else {
        hash.update(itemPath);
        hash.update("\0");
        hash.update(readFileSync(full));
      }
    }
  };
  visit(path, "");
  return hash.digest("hex");
}

function parseArguments(workspaceRoot: string, args: string[]): ParsedArguments | null {
  let suite = "";
  let output = resolve(workspaceRoot, "evidence/g0/gt-g0-07/protocol-seed");
  const boundaries: Boundary[] = [];
  for (let index = 0; index < args.length; index += 1) {
    const argument = args[index];
    if (argument === "--suite") {
      suite = args[++index] ?? "";
    } else if (argument === "--boundary") {
      const value = args[++index] as keyof typeof boundaryNames | undefined;
      if (!value || !boundaryNames[value]) return null;
      boundaries.push(boundaryNames[value]);
    } else if (argument === "--output") {
      const value = args[++index];
      if (!value) return null;
      output = isAbsolute(value) ? resolve(value) : resolve(process.cwd(), value);
    } else return null;
  }
  if (suite !== suiteId || boundaries.length === 0) return null;
  return { suite, boundaries: [...new Set(boundaries)], output };
}

function isWithin(parent: string, candidate: string): boolean {
  const path = relative(parent, candidate);
  return path === "" || (!path.startsWith(`..${sep}`) && path !== "..");
}
function readJson(path: string): unknown { return JSON.parse(readFileSync(path, "utf8")); }

function isProtocolMetaResult(value: unknown, vectorId: string, boundary: Boundary): value is ProtocolMetaResult {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const result = value as Record<string, unknown>;
  const expectedKeys = ["boundaryMode", "checks", "firstDivergence", "format", "formatVersion", "status", "vectorId"];
  if (Object.keys(result).sort().join("\0") !== expectedKeys.sort().join("\0")) return false;
  return result.format === "axiom-platform-protocol-meta-result-v1"
    && result.formatVersion === 1
    && result.vectorId === vectorId
    && result.boundaryMode === boundary
    && ["PASS", "FAIL", "REJECTED"].includes(String(result.status))
    && Array.isArray(result.checks)
    && (result.firstDivergence === null || (typeof result.firstDivergence === "object" && !Array.isArray(result.firstDivergence)));
}

function verifyEvidence(workspaceRoot: string, output: string, boundaries: Boundary[]): { count: number; sha256: string; hasFailure: boolean } | null {
  try {
    const suite = readJson(resolve(workspaceRoot, `platform/protocol/v1/suites/${suiteId}.json`)) as ProtocolSuite;
    if (suite.id !== suiteId || !Array.isArray(suite.vectorRefs)) return null;
    const results: ProtocolMetaResult[] = [];
    const resultRoot = resolve(output, "protocol-meta-results");
    if (JSON.stringify(readdirSync(resultRoot).sort()) !== JSON.stringify([...boundaries].sort())) return null;
    for (const ref of suite.vectorRefs) {
      const vector = readJson(resolve(workspaceRoot, ref.replace(/^verification\//, ""))) as { id?: unknown };
      if (typeof vector.id !== "string") return null;
      for (const boundary of boundaries) {
        const result = readJson(resolve(output, "protocol-meta-results", boundary, `${vector.id}.json`));
        if (!isProtocolMetaResult(result, vector.id, boundary)) return null;
        results.push(result);
      }
    }
    const expectedFiles = suite.vectorRefs.map((ref) => `${ref.split("/").at(-2)}.json`).sort();
    for (const boundary of boundaries) {
      if (JSON.stringify(readdirSync(resolve(resultRoot, boundary)).sort()) !== JSON.stringify(expectedFiles)) return null;
    }
    const lines = `${results.map((result) => JSON.stringify(result)).join("\n")}\n`;
    const sha256 = createHash("sha256").update(lines).digest("hex");
    const integrity = readJson(resolve(output, "corpus-integrity.json")) as Record<string, unknown>;
    const expectedResults = results.map((result) => ({ vectorId: result.vectorId, boundaryMode: result.boundaryMode, status: result.status }));
    if (integrity.format !== "axiom-platform-protocol-corpus-integrity-v1"
      || integrity.suiteId !== suiteId
      || integrity.resultCount !== results.length
      || integrity.sha256 !== sha256
      || JSON.stringify(integrity.results) !== JSON.stringify(expectedResults)) return null;
    return { count: results.length, sha256, hasFailure: results.some((result) => result.status === "FAIL") };
  } catch { return null; }
}

function writeSummary(output: string, value: Record<string, unknown>): void {
  writeFileSync(resolve(output, "summary.json"), `${JSON.stringify({ format: "axiom-platform-conformance-cli-summary-v1", command: "protocol", ...value })}\n`);
}

export function protocol(workspaceRoot: string, args: string[]): number {
  const parsed = parseArguments(workspaceRoot, args);
  if (!parsed) return ExitCode.INVALID_ARGUMENTS;
  const corpus = resolve(workspaceRoot, "platform/protocol/v1");
  if (isWithin(corpus, parsed.output)) {
    console.error("evidence output must not be inside the versioned protocol corpus");
    return ExitCode.INVALID_ARGUMENTS;
  }
  if (validate(workspaceRoot) !== ExitCode.SUCCESS) return ExitCode.INVALID_SCHEMA_OR_CORPUS;

  let before: string;
  try {
    before = treeHash(corpus);
    mkdirSync(parsed.output, { recursive: true });
  } catch (error) {
    console.error(error);
    return ExitCode.INVALID_EVIDENCE;
  }
  const result = spawnSync(process.execPath, ["tools/run_protocol_vectors.mjs"], {
    cwd: workspaceRoot,
    encoding: "utf8",
    env: { ...process.env, AXIOM_PROTOCOL_EVIDENCE_ROOT: parsed.output, AXIOM_PROTOCOL_BOUNDARIES: parsed.boundaries.join(",") },
  });
  writeFileSync(resolve(parsed.output, "protocol.log"), `${result.stdout ?? ""}${result.stderr ?? ""}`);

  let after = "unavailable";
  try { after = treeHash(corpus); } catch { /* classified below */ }
  const evidence = verifyEvidence(workspaceRoot, parsed.output, parsed.boundaries);
  if (before !== after || !evidence) {
    writeSummary(parsed.output, { suite: parsed.suite, boundaries: parsed.boundaries, status: "INVALID_EVIDENCE", corpusHashBefore: before, corpusHashAfter: after });
    return ExitCode.INVALID_EVIDENCE;
  }
  const mismatch = result.status !== 0 || evidence.hasFailure;
  writeSummary(parsed.output, {
    suite: parsed.suite, boundaries: parsed.boundaries,
    status: mismatch ? "RUNNER_EXPECTATION_MISMATCH" : "PASS",
    resultCount: evidence.count, corpusHashBefore: before, corpusHashAfter: after, integritySha256: evidence.sha256,
  });
  return mismatch ? ExitCode.RUNNER_EXPECTATION_MISMATCH : ExitCode.SUCCESS;
}
