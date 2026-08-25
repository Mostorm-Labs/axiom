import { readdir, readFile, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { aggregatePlatformRelease, createPlatformEvidenceIndex, type FullRunSetManifest, type PlatformEvidenceRecord } from "@axiom/platform-harness-runner";
import { ExitCode } from "../exit_codes.js";

const valueAfter = (args: string[], flag: string): string | null => {
  const index = args.indexOf(flag);
  return index >= 0 && args[index + 1] ? args[index + 1] : null;
};

export async function aggregateFull(args: string[]): Promise<number> {
  const runSetPath = valueAfter(args, "--run-set");
  const recordsPath = valueAfter(args, "--records");
  const outputPath = valueAfter(args, "--output");
  if (!runSetPath || !recordsPath || !outputPath) return ExitCode.INVALID_ARGUMENTS;
  try {
    const runSet = JSON.parse(await readFile(resolve(process.cwd(), runSetPath), "utf8")) as FullRunSetManifest;
    const directory = resolve(process.cwd(), recordsPath);
    const names = (await readdir(directory)).filter((name) => name.endsWith(".json")).sort();
    const records = await Promise.all(names.map(async (name) => JSON.parse(await readFile(resolve(directory, name), "utf8")) as PlatformEvidenceRecord));
    const index = createPlatformEvidenceIndex(runSet, records);
    const decision = aggregatePlatformRelease(runSet, index);
    await writeFile(resolve(process.cwd(), outputPath), `${JSON.stringify(decision, null, 2)}\n`);
    if (decision.decision === "PASS" || decision.decision === "PASS_WITH_OBSERVATIONS") return ExitCode.SUCCESS;
    if (decision.decision === "INVALID_EVIDENCE") return ExitCode.INVALID_EVIDENCE;
    return ExitCode.RUNNER_EXPECTATION_MISMATCH;
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    return ExitCode.INVALID_EVIDENCE;
  }
}
