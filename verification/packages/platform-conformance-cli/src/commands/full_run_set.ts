import { writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { createFullRunSet, type FullCadence } from "@axiom/platform-harness-runner";
import { ExitCode } from "../exit_codes.js";

const valueAfter = (args: string[], flag: string): string | null => {
  const index = args.indexOf(flag);
  return index >= 0 && args[index + 1] ? args[index + 1] : null;
};

export async function fullRunSet(args: string[]): Promise<number> {
  const cadence = valueAfter(args, "--cadence")?.toUpperCase() as FullCadence | undefined;
  const sourceCommit = valueAfter(args, "--source-commit");
  const schemaSha256 = valueAfter(args, "--schema-sha256");
  const corpusSha256 = valueAfter(args, "--corpus-sha256");
  const runnerVersion = valueAfter(args, "--runner-version");
  const runtimeVersion = valueAfter(args, "--runtime-version");
  const repeat = Number(valueAfter(args, "--repeat"));
  const seed = Number(valueAfter(args, "--seed"));
  const output = valueAfter(args, "--output");
  if (!cadence || !sourceCommit || !schemaSha256 || !corpusSha256 || !runnerVersion || !runtimeVersion || !output || !["NIGHTLY", "RELEASE"].includes(cadence)) return ExitCode.INVALID_ARGUMENTS;
  try {
    const manifest = createFullRunSet({ cadence, sourceCommit, schemaSha256, corpusSha256, runnerVersion, runtimeVersion, repeat, seed });
    await writeFile(resolve(process.cwd(), output), `${JSON.stringify(manifest, null, 2)}\n`);
    return ExitCode.SUCCESS;
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    return ExitCode.INVALID_ARGUMENTS;
  }
}
