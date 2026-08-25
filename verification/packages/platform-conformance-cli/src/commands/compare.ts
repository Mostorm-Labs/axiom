import { mkdir, readFile, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { comparePlatformObservation, type PlatformObservation, type PlatformScenario } from "@axiom/platform-harness-runner";
import { ExitCode } from "../exit_codes.js";

type Seed = { scenarios: PlatformScenario[] };
const valueAfter = (args: string[], flag: string): string | null => {
  const index = args.indexOf(flag);
  return index >= 0 && args[index + 1] ? args[index + 1] : null;
};

export async function compare(root: string, args: string[]): Promise<number> {
  if (valueAfter(args, "--suite") !== "platform-seed-v0.1") return ExitCode.INVALID_ARGUMENTS;
  const observationsPath = valueAfter(args, "--observations");
  const outputPath = valueAfter(args, "--output");
  if (!observationsPath || !outputPath) return ExitCode.INVALID_ARGUMENTS;
  try {
    // @ts-expect-error the corpus validator is a tooling module, not a product API.
    const validator = await import("../../../../tools/validate_platform_scenarios.mjs") as { validatePlatformSeed: (options: { suiteFile: string }) => Promise<Seed> };
    const seed = await validator.validatePlatformSeed({ suiteFile: join(root, "platform/v1/suites/platform-seed-v0.1.json") });
    const output = resolve(process.cwd(), outputPath);
    await mkdir(output, { recursive: true });
    let blockingFailure = false;
    for (const scenario of seed.scenarios) {
      let observation: PlatformObservation;
      try {
        observation = JSON.parse(await readFile(resolve(process.cwd(), observationsPath, `${scenario.id}.json`), "utf8")) as PlatformObservation;
      } catch {
        continue;
      }
      const result = comparePlatformObservation(scenario, observation);
      await writeFile(join(output, `${scenario.id}.json`), `${JSON.stringify(result, null, 2)}\n`);
      if (!["PASS", "OBSERVED_AGREEMENT_OPEN", "OBSERVED_DIVERGENCE_OPEN"].includes(result.result)) blockingFailure = true;
    }
    return blockingFailure ? ExitCode.RUNNER_EXPECTATION_MISMATCH : ExitCode.SUCCESS;
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    return ExitCode.INVALID_EVIDENCE;
  }
}
