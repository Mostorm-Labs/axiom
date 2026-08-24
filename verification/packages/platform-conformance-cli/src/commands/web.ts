import { mkdir, readFile, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { WebReferenceAdapter, WEB_PROFILE } from "@axiom/platform-harness-web";
import { ExitCode } from "../exit_codes.js";

type Scenario = { id: string; requirementStatus: string; targets: Array<{ platformFamily: string }>; expected: Record<string, unknown>; steps: Array<Record<string, unknown>> };
type Seed = { scenarios: Scenario[]; digest: string };
async function loadSeed(root: string): Promise<Seed> {
  // The checked-in scenario validator is intentionally a tooling-only .mjs module.
  // @ts-expect-error no product type boundary is created for this verification script.
  const module = await import("../../../../tools/validate_platform_scenarios.mjs") as { validatePlatformSeed: (options: { suiteFile: string }) => Promise<Seed> };
  return module.validatePlatformSeed({ suiteFile: join(root, "platform/v1/suites/platform-seed-v0.1.json") });
}

const usage = "profile --adapter web | run --suite platform-seed-v0.1 --adapter web --output PATH";
const valueAfter = (args: string[], flag: string): string | null => {
  const index = args.indexOf(flag);
  return index >= 0 && args[index + 1] ? args[index + 1] : null;
};

export function profile(args: string[]): number {
  if (args.length !== 2 || args[0] !== "--adapter") return ExitCode.INVALID_ARGUMENTS;
  if (args[1] === "web") process.stdout.write(`${JSON.stringify(WEB_PROFILE)}\n`);
  else if (args[1] === "windows") process.stdout.write(`${JSON.stringify({
    format: "axiom-platform-profile-v1", formatVersion: 1, profileId: "windows-native-reference-v0-1",
    platformFamily: "WINDOWS", platformVariant: "win32-d3d12", capabilities: [
      "semantic.projection.capture", "surface.generation", "metrics.generation", "surface.loss.inject",
      "device.loss.inject", "presentation.feedback", "bridge.public_facade", "bridge.data_bridge",
      "bridge.callback_trace", "input.pointer_sample_batch", "arc.preview", "arc.preview.loss.inject",
      "platform.state.capture", "surface.ownership.capture", "fault.stale_generation.inject",
      "harness.completion_tokens", "harness.source_lease_registry", "harness.late_event_fence",
      "harness.source_attempt_trace",
    ], realization: { host: "WIN32_NATIVE", surface: "DXGI_SWAPCHAIN", backend: "D3D12", arc: "ENABLED" },
  })}\n`);
  else return ExitCode.INVALID_ARGUMENTS;
  return ExitCode.SUCCESS;
}

export async function runWeb(root: string, args: string[]): Promise<number> {
  if (args.length < 6 || valueAfter(args, "--suite") !== "platform-seed-v0.1" || valueAfter(args, "--adapter") !== "web") return ExitCode.INVALID_ARGUMENTS;
  const output = valueAfter(args, "--output");
  if (!output) return ExitCode.INVALID_ARGUMENTS;
  const out = resolve(process.cwd(), output);
  const corpusRoot = resolve(root, "platform/v1");
  if (out === corpusRoot || out.startsWith(`${corpusRoot}/`)) return ExitCode.INVALID_ARGUMENTS;
  const corpus = await loadSeed(root);
  const adapter = new WebReferenceAdapter();
  const applicable = corpus.scenarios.filter((scenario) => scenario.targets.some((target) => target.platformFamily === "WEB"));
  const notApplicable = corpus.scenarios.filter((scenario) => !scenario.targets.some((target) => target.platformFamily === "WEB"));
  await mkdir(join(out, "observations"), { recursive: true });
  for (const scenario of applicable) {
    const observation = adapter.execute(scenario as never);
    await writeFile(join(out, "observations", `${scenario.id}.json`), `${JSON.stringify(observation, null, 2)}\n`);
    await mkdir(join(out, "results"), { recursive: true });
    await writeFile(join(out, "results", `${scenario.id}.json`), `${JSON.stringify({ format: "axiom-platform-conformance-result-v1", formatVersion: 1, scenarioId: scenario.id, requirementStatus: scenario.requirementStatus, result: "OBSERVED_AGREEMENT_OPEN", participants: [{ profileId: WEB_PROFILE.profileId }], checks: [{ kind: "OBSERVATION_CAPTURED", status: "OBSERVED" }], openObservations: [{ kind: "COMPARATOR_DEFERRED", reason: "shared runner owns expected comparison" }], divergence: null, diagnostics: [] }, null, 2)}\n`);
  }
  await writeFile(join(out, "profile.json"), `${JSON.stringify(WEB_PROFILE, null, 2)}\n`);
  await writeFile(join(out, "applicability.json"), `${JSON.stringify({ format: "axiom-platform-web-applicability-v1", adapter: "web", notApplicable: notApplicable.map((scenario) => ({ scenarioId: scenario.id, reason: (scenario.expected.applicability as { WEB?: string } | undefined)?.WEB ?? "NOT_APPLICABLE_BY_CONTRACT" })) }, null, 2)}\n`);
  await writeFile(join(out, "summary.json"), `${JSON.stringify({ format: "axiom-platform-web-run-summary-v1", adapter: "web", suite: "platform-seed-v0.1", corpusDigest: corpus.digest, applicableCount: applicable.length, notApplicableCount: notApplicable.length, resultCount: corpus.scenarios.length, observationPolicy: "facts-only; comparison-owned-by-shared-runner" }, null, 2)}\n`);
  return ExitCode.SUCCESS;
}
