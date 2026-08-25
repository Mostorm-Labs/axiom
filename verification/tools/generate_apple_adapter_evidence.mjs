#!/usr/bin/env node

import { createHash } from "node:crypto";
import { copyFile, mkdir, readFile, readdir, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { validatePlatformSeed } from "./validate_platform_scenarios.mjs";
import { IOS_PROFILE, IPADOS_PROFILE, AppleReferenceAdapter } from "../packages/platform-harness-apple/dist/index.js";

const verificationRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const defaultOutputRoot = resolve(verificationRoot, "evidence/g0/gt-g0-13");
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const writeJson = async (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");

function validatePhysicalReport(report, label) {
  if (!report || !["ios", "ipados"].includes(report.platform)) throw new Error(`${label}: platform must be ios or ipados`);
  if (report.backend !== "ganesh-metal") throw new Error(`${label}: unexpected backend`);
  if (report.digest !== "47826449b895ac4f4a57b4f386379775") throw new Error(`${label}: digest mismatch`);
  if (report.lifecycle !== 100 || report.smoke_seconds !== 60) throw new Error(`${label}: lifecycle/smoke gate mismatch`);
  if (typeof report.max_frame_ms !== "number" || report.max_frame_ms > 100) throw new Error(`${label}: frame gate failed`);
  if (report.memory_analysis?.passed !== true) throw new Error(`${label}: memory gate failed`);
  if (report.physical_environment?.reality !== "physical" || report.physical_environment?.platform !== "iOS") {
    throw new Error(`${label}: report is not a physical Apple mobile execution`);
  }
  if (typeof report.visual?.matching_ratio !== "number" || report.visual.matching_ratio < 0.999) {
    throw new Error(`${label}: visual ratio gate failed`);
  }
  return report;
}

export async function generateAppleAdapterEvidence({
  outputRoot = defaultOutputRoot,
  sourceCommit = process.env.AXIOM_EVIDENCE_SOURCE_COMMIT ?? "UNBOUND",
  physicalExecution = process.env.AXIOM_APPLE_PHYSICAL_EXECUTION === "true",
  iosPhysicalReportPath = process.env.AXIOM_APPLE_IOS_PHYSICAL_REPORT,
  ipadosPhysicalReportPath = process.env.AXIOM_APPLE_IPADOS_PHYSICAL_REPORT,
} = {}) {
  if (physicalExecution && !iosPhysicalReportPath && !ipadosPhysicalReportPath) throw new Error("at least one Apple physical report is required for physical Evidence");
  const iosPhysicalReport = iosPhysicalReportPath
    ? validatePhysicalReport(JSON.parse(await readFile(resolve(iosPhysicalReportPath), "utf8")), "iPhone physical report")
    : null;
  if (iosPhysicalReport && iosPhysicalReport.platform !== "ios") throw new Error("iPhone physical report must have platform ios");
  const ipadosPhysicalReport = ipadosPhysicalReportPath
    ? validatePhysicalReport(JSON.parse(await readFile(resolve(ipadosPhysicalReportPath), "utf8")), "iPadOS physical report")
    : null;
  if (ipadosPhysicalReport && ipadosPhysicalReport.platform !== "ipados") throw new Error("iPadOS physical report must have platform ipados");
  const corpus = await validatePlatformSeed({ suiteFile: join(verificationRoot, "platform/v1/suites/platform-seed-v0.1.json") });
  const applicable = corpus.scenarios.filter((scenario) => scenario.targets.some((target) => target.platformFamily === "APPLE"));
  const notApplicable = corpus.scenarios.filter((scenario) => !scenario.targets.some((target) => target.platformFamily === "APPLE"));
  const targets = [
    { key: "ios", profile: IOS_PROFILE },
    { key: "ipados", profile: IPADOS_PROFILE },
  ];
  for (const { key, profile } of targets) {
    const root = join(outputRoot, key);
    await mkdir(join(root, "observations"), { recursive: true });
    await mkdir(join(root, "results"), { recursive: true });
    const adapter = new AppleReferenceAdapter(profile);
    const physicalReport = key === "ios" ? iosPhysicalReport : ipadosPhysicalReport;
    const physicalReportPath = key === "ios" ? iosPhysicalReportPath : ipadosPhysicalReportPath;
    const physicalReportName = key === "ios" ? "ios-physical-report.json" : "ipados-physical-report.json";
    if (physicalReport && physicalReportPath) await copyFile(resolve(physicalReportPath), join(root, physicalReportName));
    for (const scenario of applicable) {
      const observation = adapter.execute(scenario);
      observation.execution = { ...observation.execution, physicalExecution: Boolean(physicalReport), deviceEvidence: physicalReport ? `${key}/${physicalReportName}` : null };
      observation.diagnostics = physicalReport ? ["APPLE_PHYSICAL_EVIDENCE_ATTACHED"] : ["APPLE_DEVICE_EXECUTION_PENDING"];
      await writeJson(join(root, "observations", `${scenario.id}.json`), observation);
      await writeJson(join(root, "results", `${scenario.id}.json`), {
        format: "axiom-platform-conformance-result-v1", formatVersion: 1, scenarioId: scenario.id,
        requirementStatus: scenario.requirementStatus, result: "OBSERVED_AGREEMENT_OPEN",
        participants: [{ profileId: profile.profileId }], checks: [{ kind: "OBSERVATION_CAPTURED", status: "OBSERVED" }],
        openObservations: [{ kind: "COMPARATOR_DEFERRED", reason: "shared runner owns expected comparison" }],
        divergence: null, diagnostics: physicalReport ? ["PHYSICAL_DEVICE_REPORT_ATTACHED"] : ["DEVICE_REPORT_PENDING"],
      });
    }
    await writeJson(join(root, "profile.json"), profile);
    await writeJson(join(root, "summary.json"), {
      format: "axiom-platform-apple-run-summary-v1", sourceCommit, adapter: "apple-xctest-reference",
      deviceClass: profile.realization.deviceClass, profileId: profile.profileId, suite: "platform-seed-v0.1",
      corpusDigest: corpus.digest, applicableCount: applicable.length, notApplicableCount: notApplicable.length,
      resultCount: corpus.scenarios.length, physicalExecution: Boolean(physicalReport),
      executionKind: physicalReport ? "physical-device" : "logical-reference",
      physicalStatus: physicalReport ? "PASS" : "PENDING_NO_DEVICE",
      physicalEvidence: physicalReport ? `${key}/${physicalReportName}` : null,
      observationPolicy: "facts-only; comparison-owned-by-shared-runner",
    });
  }
  await writeJson(join(outputRoot, "applicability.json"), {
    format: "axiom-platform-apple-applicability-v1", adapter: "apple-xctest-reference", platformFamily: "APPLE",
    notApplicable: notApplicable.map((scenario) => ({ scenarioId: scenario.id, reason: scenario.expected.applicability?.APPLE ?? "NOT_APPLICABLE_BY_CONTRACT" })),
  });
  await writeJson(join(outputRoot, "macos-core-summary.json"), {
    format: "axiom-platform-macos-core-conformance-v1", productGate: false, status: "HOST_LOGICAL_CONTRACT_ONLY",
    note: "macOS native product shell is deferred; this target only checks the shared Apple-independent contract.",
  });
  const files = [];
  async function collect(dir, relativeRoot = "") {
    for (const entry of (await readdir(dir, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
      const path = join(dir, entry.name); const relative = relativeRoot ? `${relativeRoot}/${entry.name}` : entry.name;
      if (entry.isDirectory()) await collect(path, relative);
      else if (entry.isFile() && entry.name !== "manifest.json") files.push({ path: relative, sha256: sha256(await readFile(path)) });
    }
  }
  await collect(outputRoot);
  await writeJson(join(outputRoot, "manifest.json"), { format: "axiom-platform-apple-evidence-manifest-v1", formatVersion: 1, sourceCommit, targets: ["ios", "ipados"], files });
  return { applicableCount: applicable.length, targetCount: targets.length, files };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  try {
    const result = await generateAppleAdapterEvidence();
    console.log(`Apple adapter evidence: ${result.targetCount} targets × ${result.applicableCount} scenarios`);
  } catch (error) {
    console.error(`Apple adapter evidence failed: ${error.message}`);
    process.exitCode = 1;
  }
}
