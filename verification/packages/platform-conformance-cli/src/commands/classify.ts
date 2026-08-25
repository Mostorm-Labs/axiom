import { readFile, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { classifyChanges } from "@axiom/platform-harness-runner";
import { ExitCode } from "../exit_codes.js";

const valueAfter = (args: string[], flag: string): string | null => {
  const index = args.indexOf(flag);
  return index >= 0 && args[index + 1] ? args[index + 1] : null;
};

export async function classify(args: string[]): Promise<number> {
  const changedPathFile = valueAfter(args, "--changed-paths");
  const outputPath = valueAfter(args, "--output");
  if (!changedPathFile || !outputPath) return ExitCode.INVALID_ARGUMENTS;
  try {
    const paths = (await readFile(resolve(process.cwd(), changedPathFile), "utf8"))
      .split(/\r?\n/u).map((path) => path.trim()).filter(Boolean);
    await writeFile(resolve(process.cwd(), outputPath), `${JSON.stringify(classifyChanges(paths), null, 2)}\n`);
    return ExitCode.SUCCESS;
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    return ExitCode.INVALID_EVIDENCE;
  }
}
