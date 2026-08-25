import { readFile, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { comparePlatformReleaseDecisions, type PlatformReleaseDecision } from "@axiom/platform-harness-runner";
import { ExitCode } from "../exit_codes.js";

const valueAfter = (args: string[], flag: string): string | null => {
  const index = args.indexOf(flag);
  return index >= 0 && args[index + 1] ? args[index + 1] : null;
};

export async function compareFull(args: string[]): Promise<number> {
  const leftPath = valueAfter(args, "--left");
  const rightPath = valueAfter(args, "--right");
  const outputPath = valueAfter(args, "--output");
  if (!leftPath || !rightPath || !outputPath) return ExitCode.INVALID_ARGUMENTS;
  try {
    const left = JSON.parse(await readFile(resolve(process.cwd(), leftPath), "utf8")) as PlatformReleaseDecision;
    const right = JSON.parse(await readFile(resolve(process.cwd(), rightPath), "utf8")) as PlatformReleaseDecision;
    const comparison = comparePlatformReleaseDecisions(left, right);
    await writeFile(resolve(process.cwd(), outputPath), `${JSON.stringify(comparison, null, 2)}\n`);
    return comparison.status === "PASS" ? ExitCode.SUCCESS : ExitCode.INVALID_EVIDENCE;
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    return ExitCode.INVALID_EVIDENCE;
  }
}
