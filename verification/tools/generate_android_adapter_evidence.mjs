#!/usr/bin/env node

import { createHash } from "node:crypto";
import { copyFile, mkdir, readFile, readdir, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { validatePlatformSeed } from "./validate_platform_scenarios.mjs";
import { AndroidReferenceAdapter, ANDROID_PROFILE } from "../packages/platform-harness-android/dist/index.js";

const verificationRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const defaultOutputRoot = resolve(verificationRoot, "evidence/g0/gt-g0-12");

const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const writeJson = async (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");

export async function generateAndroidAdapterEvidence({
  outputRoot = defaultOutputRoot,
  sourceCommit = process.env.AXIOM_EVIDENCE_SOURCE_COMMIT ?? "UNBOUND",
  physicalExecution = process.env.AXIOM_ANDROID_PHYSICAL_EXECUTION === "true",
  instrumentationResultPath = process.env.AXIOM_ANDROID_INSTRUMENTATION_RESULT,
} = {}) {
  await mkdir(join(outputRoot, "observations"), { recursive: true });
  await mkdir(join(outputRoot, "results"), { recursive: true });
  const corpus = await validatePlatformSeed({ suiteFile: join(verificationRoot, "platform/v1/suites/platform-seed-v0.1.json") });
  const applicable = corpus.scenarios.filter((scenario) => scenario.targets.some((target) => target.platformFamily === "ANDROID"));
  const notApplicable = corpus.scenarios.filter((scenario) => !scenario.targets.some((target) => target.platformFamily === "ANDROID"));
  if (physicalExecution && !instrumentationResultPath) throw new Error("AXIOM_ANDROID_INSTRUMENTATION_RESULT is required for physical Evidence");
  let instrumentation = null;
  if (instrumentationResultPath) {
    instrumentation = JSON.parse(await readFile(resolve(instrumentationResultPath), "utf8"));
    if (instrumentation.format !== "axiom-android-instrumentation-result-v1" || instrumentation.status !== "HARNESS_STARTED") {
      throw new Error("invalid Android instrumentation result");
    }
    if (physicalExecution && instrumentation.device?.emulator !== false) {
      throw new Error("physical Android Evidence cannot use an emulator result");
    }
  }

  for (const scenario of applicable) {
    const adapter = new AndroidReferenceAdapter();
    const observation = adapter.execute(scenario);
    observation.execution = {
      ...observation.execution,
      physicalExecution,
      instrumentationResult: instrumentation ? "instrumentation-result.json" : null,
    };
    observation.diagnostics = instrumentation
      ? [physicalExecution ? "ANDROID_PHYSICAL_INSTRUMENTATION_CAPTURED" : "ANDROID_EMULATOR_INSTRUMENTATION_CAPTURED"]
      : ["ANDROID_INSTRUMENTATION_PENDING"];
    await writeJson(join(outputRoot, "observations", `${scenario.id}.json`), observation);
    await writeJson(join(outputRoot, "results", `${scenario.id}.json`), {
      format: "axiom-platform-conformance-result-v1",
      formatVersion: 1,
      scenarioId: scenario.id,
      requirementStatus: scenario.requirementStatus,
      result: "OBSERVED_AGREEMENT_OPEN",
      participants: [{ profileId: ANDROID_PROFILE.profileId }],
      checks: [{ kind: "OBSERVATION_CAPTURED", status: "OBSERVED" }],
      openObservations: [{ kind: "COMPARATOR_DEFERRED", reason: "shared runner owns expected comparison" }],
      divergence: null,
      diagnostics: instrumentation
        ? [physicalExecution ? "PHYSICAL_INSTRUMENTATION_RESULT_ATTACHED" : "EMULATOR_INSTRUMENTATION_RESULT_ATTACHED"]
        : ["LOGICAL_REFERENCE_ONLY"],
    });
  }

  if (instrumentationResultPath) {
    await copyFile(resolve(instrumentationResultPath), join(outputRoot, "instrumentation-result.json"));
  }
  await writeJson(join(outputRoot, "profile.json"), ANDROID_PROFILE);
  await writeJson(join(outputRoot, "applicability.json"), {
    format: "axiom-platform-android-applicability-v1",
    adapter: "android-instrumentation",
    platformFamily: "ANDROID",
    notApplicable: notApplicable.map((scenario) => ({
      scenarioId: scenario.id,
      reason: scenario.expected.applicability?.ANDROID ?? "NOT_APPLICABLE_BY_CONTRACT",
    })),
  });
  await writeJson(join(outputRoot, "summary.json"), {
    format: "axiom-platform-android-run-summary-v1",
    sourceCommit,
    implementationState: sourceCommit === "UNBOUND" || sourceCommit === "WORKTREE" ? sourceCommit : "COMMITTED",
    adapter: "android-instrumentation",
    suite: "platform-seed-v0.1",
    corpusDigest: corpus.digest,
    applicableCount: applicable.length,
    notApplicableCount: notApplicable.length,
    resultCount: corpus.scenarios.length,
    physicalExecution,
    emulator: instrumentation?.device?.emulator ?? null,
    executionKind: instrumentation ? "android-instrumentation" : "logical-reference",
    observationPolicy: "facts-only; comparison-owned-by-shared-runner",
  });

  const files = [];
  async function collect(dir, relativeRoot = "") {
    for (const entry of (await readdir(dir, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
      const path = join(dir, entry.name);
      const relative = relativeRoot ? `${relativeRoot}/${entry.name}` : entry.name;
      if (entry.isDirectory()) await collect(path, relative);
      else if (entry.isFile() && entry.name !== "manifest.json") files.push({ path: relative, sha256: sha256(await readFile(path)) });
    }
  }
  await collect(outputRoot);
  await writeJson(join(outputRoot, "manifest.json"), {
    format: "axiom-platform-android-evidence-manifest-v1",
    formatVersion: 1,
    sourceCommit,
    files,
  });
  return { applicableCount: applicable.length, notApplicableCount: notApplicable.length, files };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  try {
    const result = await generateAndroidAdapterEvidence();
    console.log(`Android adapter evidence: ${result.applicableCount} applicable, ${result.notApplicableCount} N/A`);
  } catch (error) {
    console.error(`Android adapter evidence failed: ${error.message}`);
    process.exitCode = 1;
  }
}
