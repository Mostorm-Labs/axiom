import { createHash } from "node:crypto";
import { readFile, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { createG3GateReport, type G3GateReportInput } from "@axiom/platform-harness-runner";
import { ExitCode } from "../exit_codes.js";

const valueAfter = (args: string[], flag: string): string | null => { const i = args.indexOf(flag); return i >= 0 && args[i + 1] ? args[i + 1] : null; };
const digest = (value: string): string => createHash("sha256").update(value).digest("hex");

export async function g3GateReport(args: string[]): Promise<number> {
  const sourceCommit = valueAfter(args, "--source-commit"); const branch = valueAfter(args, "--branch"); const lineagePath = valueAfter(args, "--lineage"); const checksPath = valueAfter(args, "--checks"); const platformsPath = valueAfter(args, "--platforms"); const output = valueAfter(args, "--output");
  if (!sourceCommit || !branch || !lineagePath || !checksPath || !platformsPath || !output) return ExitCode.INVALID_ARGUMENTS;
  try {
    const [lineageRaw, checksRaw, platformsRaw] = await Promise.all([readFile(resolve(process.cwd(), lineagePath), "utf8"), readFile(resolve(process.cwd(), checksPath), "utf8"), readFile(resolve(process.cwd(), platformsPath), "utf8")]);
    const lineage = JSON.parse(lineageRaw); const checks = JSON.parse(checksRaw); const platforms = JSON.parse(platformsRaw);
    const input: G3GateReportInput = { sourceCommit, branch, generatorVersion: "0.1.0", wpLineage: lineage, checks, platforms, inputSha256: { lineage: digest(lineageRaw), checks: digest(checksRaw), platforms: digest(platformsRaw) } };
    const report = createG3GateReport(input); await writeFile(resolve(process.cwd(), output), `${JSON.stringify(report, null, 2)}\n`); return ExitCode.SUCCESS;
  } catch (error) { console.error(error instanceof Error ? error.message : String(error)); return ExitCode.INVALID_EVIDENCE; }
}
