#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile, copyFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { validatePlatformSeed } from "./validate_platform_scenarios.mjs";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
const outputRoot = resolve(root, "evidence/g0/gt-g0-11");
const sourceCommit = process.env.AXIOM_EVIDENCE_SOURCE_COMMIT ?? "UNBOUND";
const physicalExecution = process.env.AXIOM_WINDOWS_PHYSICAL_EXECUTION === "true";
const tracePath = process.env.AXIOM_WINDOWS_NATIVE_TRACE;
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const writeJson = async (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");

const corpus = await validatePlatformSeed({ suiteFile: join(root, "platform/v1/suites/platform-seed-v0.1.json") });
const applicable = corpus.scenarios.filter((scenario) => scenario.targets.some((target) => target.platformFamily === "WINDOWS"));
const profile = {
  format: "axiom-platform-profile-v1", formatVersion: 1, profileId: "windows-native-reference-v0-1",
  platformFamily: "WINDOWS", platformVariant: "win32-d3d12", capabilities: [
    "semantic.projection.capture", "surface.generation", "metrics.generation", "surface.loss.inject",
    "device.loss.inject", "presentation.feedback", "bridge.public_facade", "bridge.data_bridge",
    "bridge.callback_trace", "input.pointer_sample_batch", "arc.preview", "arc.preview.loss.inject",
    "platform.state.capture", "surface.ownership.capture", "fault.stale_generation.inject",
    "harness.completion_tokens", "harness.source_lease_registry", "harness.late_event_fence",
    "harness.source_attempt_trace",
  ], realization: { host: "WIN32_NATIVE", surface: "DXGI_SWAPCHAIN", backend: "D3D12", arc: "ENABLED", device: physicalExecution ? "D3D12_WARP" : "UNREALIZED" },
};

let nativeTrace = null;
if (physicalExecution) {
  if (!tracePath) throw new Error("AXIOM_WINDOWS_NATIVE_TRACE is required for physical Evidence");
  nativeTrace = JSON.parse(await readFile(resolve(tracePath), "utf8"));
  if (nativeTrace.native_surface_ready !== true) throw new Error("Windows physical trace did not create a native surface");
  await writeJson(join(outputRoot, "native-trace.json"), nativeTrace);
}

await mkdir(join(outputRoot, "observations"), { recursive: true });
await mkdir(join(outputRoot, "results"), { recursive: true });
for (const scenario of applicable) {
  const observation = {
    format: "axiom-platform-observation-v1", formatVersion: 1, scenarioId: scenario.id,
    profileId: profile.profileId, platformFamily: "WINDOWS",
    execution: { adapter: "windows-native-reference-v0.1", boundary: "IN_PROCESS", deterministic: true, physicalExecution },
    capabilities: profile.capabilities, terminal: { state: physicalExecution ? "PHYSICAL_REFERENCE_READY" : "REFERENCE_READY" }, steps: [], artifacts: {}, semanticCheckpoints: [],
    stateCheckpoints: [], targetBindings: [], realization: profile.realization,
    diagnostics: physicalExecution ? ["NATIVE_WIN32_D3D12_TRACE_CAPTURED"] : ["WINDOWS_NATIVE_EXECUTION_PENDING"]
  };
  await writeJson(join(outputRoot, "observations", `${scenario.id}.json`), observation);
  await writeJson(join(outputRoot, "results", `${scenario.id}.json`), {
    format: "axiom-platform-conformance-result-v1", formatVersion: 1, scenarioId: scenario.id,
    requirementStatus: scenario.requirementStatus, result: physicalExecution ? "OBSERVED_AGREEMENT_OPEN" : "BLOCKED_OPEN", participants: [{ profileId: profile.profileId }],
    checks: physicalExecution ? [{ kind: "NATIVE_HOST_TRACE_CAPTURED", status: "OBSERVED" }] : [],
    openObservations: physicalExecution ? [{ kind: "COMPARATOR_DEFERRED", reason: "shared runner owns expected comparison" }] : [{ kind: "WINDOWS_NATIVE_EXECUTION_PENDING", reason: "requires Windows runner with Win32/D3D12 toolchain" }], divergence: null, diagnostics: [],
  });
}
await writeJson(join(outputRoot, "profile.json"), profile);
await writeJson(join(outputRoot, "summary.json"), {
  format: "axiom-platform-windows-run-summary-v1", sourceCommit,
  implementationState: sourceCommit === "UNBOUND" ? "UNBOUND" : (physicalExecution ? "COMMITTED_PHYSICAL" : "WORKTREE_OR_HOST_PENDING"),
  adapter: "windows", suite: "platform-seed-v0.1", corpusDigest: corpus.digest,
  applicableCount: applicable.length, resultCount: applicable.length,
  physicalExecution, blockedReason: physicalExecution ? null : "Windows native/D3D12 execution requires a Windows runner",
});
const files = [];
async function collect(dir, relativeRoot = "") {
  const entries = (await (await import("node:fs/promises")).readdir(dir, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name));
  for (const entry of entries) {
    const path = join(dir, entry.name); const relative = relativeRoot ? `${relativeRoot}/${entry.name}` : entry.name;
    if (entry.isDirectory()) await collect(path, relative);
    else if (entry.isFile() && entry.name !== "manifest.json") files.push({ path: relative, sha256: sha256(await readFile(path)) });
  }
}
await collect(outputRoot);
await writeJson(join(outputRoot, "manifest.json"), { format: "axiom-platform-windows-evidence-manifest-v1", sourceCommit, files });
console.log(`Windows adapter evidence: ${applicable.length} applicable; physical execution ${physicalExecution ? "captured" : "pending"}`);
