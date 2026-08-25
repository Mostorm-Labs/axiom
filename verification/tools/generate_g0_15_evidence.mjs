#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdir, readFile, readdir, rm, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import {
  aggregatePlatformRelease,
  comparePlatformReleaseDecisions,
  createFullRunSet,
  createPlatformEvidenceIndex,
} from "../packages/platform-harness-runner/dist/index.js";

const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const output = resolve(process.env.AXIOM_G0_15_OUTPUT ?? join(verificationRoot, "evidence/g0/gt-g0-15"));
const sourceLabel = process.env.AXIOM_EVIDENCE_SOURCE_COMMIT ?? "WORKTREE";
const sourceCommit = /^[0-9a-f]{40}$/u.test(sourceLabel) ? sourceLabel : "a".repeat(40);
const sha = (value) => createHash("sha256").update(value).digest("hex");
const writeJson = (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`);
const profileDefinitions = [
  ["web", "WEB", "web-reference-v0-1"],
  ["windows", "WINDOWS", "windows-native-reference-v0-1"],
  ["android", "ANDROID", "android-instrumentation-reference-v0-1"],
  ["ios", "APPLE", "ios-rn-objcxx-reference-v0-1"],
  ["ipados", "APPLE", "ipados-rn-objcxx-reference-v0-1"],
];
const groups = ["PG-01", "PG-02", "PG-03", "PG-04", "PG-05", "PG-06"];

await rm(output, { recursive: true, force: true });
await mkdir(join(output, "deliberate-failures"), { recursive: true });
const runSet = createFullRunSet({
  cadence: "NIGHTLY", sourceCommit, schemaSha256: sha("schema"), corpusSha256: sha("corpus"),
  runnerVersion: "0.1.0", runtimeVersion: "0.1.0", repeat: 2, seed: 17,
});
await writeJson(join(output, "run-set.json"), runSet);

const record = (subject, overrides = {}) => {
  const profile = profileDefinitions.find(([key]) => key === subject);
  const isPrerequisite = ["schema", "protocol", "semantic"].includes(subject);
  return {
    format: "axiom-platform-evidence-record-v1", formatVersion: 1, subject,
    category: isPrerequisite ? "PREREQUISITE" : "PROFILE",
    platformFamily: profile?.[1] ?? null, profileId: profile?.[2] ?? null,
    sourceCommit, corpusSha256: runSet.corpusSha256, runnerVersion: "0.1.0", runtimeVersion: "0.1.0",
    reality: isPrerequisite ? "NOT_APPLICABLE" : "HOSTED", status: "PASS", evidenceSha256: sha(`evidence:${subject}`),
    pgStatuses: isPrerequisite ? [] : groups.map((group) => ({ group, status: "PASS" })), diagnostics: [], environment: { runnerName: "worktree" },
    ...overrides,
  };
};
const records = ["schema", "protocol", "semantic", ...profileDefinitions.map(([key]) => key)].map((subject) => record(subject));
const index = createPlatformEvidenceIndex(runSet, records);
await writeJson(join(output, "evidence-index.json"), index);
const decision = aggregatePlatformRelease(runSet, index);
await writeJson(join(output, "platform-release-decision.json"), decision);
await writeJson(join(output, "reproducibility-comparison.json"), comparePlatformReleaseDecisions(decision, aggregatePlatformRelease(runSet, createPlatformEvidenceIndex(runSet, records.map((value) => ({ ...value, environment: { runnerName: "other" } }))))));

const failureCases = {
  "missing-ipados": records.filter(({ subject }) => subject !== "ipados"),
  "invalid-hash": records.map((value) => value.subject === "web" ? { ...value, evidenceSha256: "invalid" } : value),
  "authority-blocked": records.map((value) => value.subject === "web" ? { ...value, reality: "HOSTED" } : value),
  "correctness-fail": records.map((value) => value.subject === "web" ? { ...value, status: "FAIL" } : value),
  "observations": records.map((value) => value.subject === "web" ? { ...value, status: "PASS_WITH_OBSERVATIONS" } : value),
};
for (const [name, values] of Object.entries(failureCases)) {
  const failureIndex = createPlatformEvidenceIndex(runSet, values);
  await writeJson(join(output, "deliberate-failures", `${name}.json`), aggregatePlatformRelease(runSet, failureIndex));
}

await writeJson(join(output, "summary.json"), {
  format: "axiom-gt-g0-15-evidence-summary-v1", formatVersion: 1, taskId: "GT-G0-15", sourceCommit: sourceLabel,
  status: sourceLabel === "WORKTREE" ? "WORKTREE_VALIDATED" : "COMMIT_BOUND_VALIDATED",
  authority: "G0_WIRING_ONLY", cadence: "NIGHTLY", profileCount: 5, platformFamilyCount: 4,
  pgGroups: groups, hostedValidation: "PENDING", physicalReleaseValidation: "BLOCKED_AUTHORITY",
  limitations: ["NOT_COMMIT_BOUND", "HOSTED_NIGHTLY_PENDING", "PHYSICAL_RELEASE_EVIDENCE_REQUIRED", "NOT_G0_GATE_REPORT", "NOT_G3_GATE_DECISION"],
});

const files = [];
async function collect(directory, prefix = "") {
  for (const entry of (await readdir(directory, { withFileTypes: true })).sort((left, right) => left.name.localeCompare(right.name))) {
    if (entry.name === "manifest.json") continue;
    const path = join(directory, entry.name);
    const relative = prefix ? `${prefix}/${entry.name}` : entry.name;
    if (entry.isDirectory()) await collect(path, relative);
    else files.push({ path: relative, sha256: sha(await readFile(path)) });
  }
}
await collect(output);
await writeJson(join(output, "manifest.json"), { format: "axiom-gt-g0-15-evidence-manifest-v1", sourceCommit: sourceLabel, files });
console.log(`GT-G0-15 evidence: ${files.length} files; ${sourceLabel}`);
