import assert from "node:assert/strict";
import test from "node:test";
import {
  aggregatePlatformRelease,
  comparePlatformReleaseDecisions,
  createFullRunSet,
  createPlatformEvidenceIndex,
} from "../dist/index.js";

const sha = (value) => value.repeat(64);
const sourceCommit = "a".repeat(40);
const groups = ["PG-01", "PG-02", "PG-03", "PG-04", "PG-05", "PG-06"];

function runSet(cadence = "NIGHTLY", repeat = 1) {
  return createFullRunSet({
    cadence,
    sourceCommit,
    schemaSha256: sha("b"),
    corpusSha256: sha("c"),
    runnerVersion: "0.1.0",
    runtimeVersion: "0.1.0",
    repeat,
    seed: 17,
  });
}

function record(subject, overrides = {}) {
  const profile = {
    web: ["WEB", "web-reference-v0-1"],
    windows: ["WINDOWS", "windows-native-reference-v0-1"],
    android: ["ANDROID", "android-instrumentation-reference-v0-1"],
    ios: ["APPLE", "ios-rn-objcxx-reference-v0-1"],
    ipados: ["APPLE", "ipados-rn-objcxx-reference-v0-1"],
  }[subject];
  const prerequisite = ["schema", "protocol", "semantic"].includes(subject);
  return {
    format: "axiom-platform-evidence-record-v1",
    formatVersion: 1,
    subject,
    category: prerequisite ? "PREREQUISITE" : "PROFILE",
    platformFamily: prerequisite ? null : profile[0],
    profileId: prerequisite ? null : profile[1],
    sourceCommit,
    corpusSha256: sha("c"),
    runnerVersion: "0.1.0",
    runtimeVersion: "0.1.0",
    reality: prerequisite ? "NOT_APPLICABLE" : "HOSTED",
    status: "PASS",
    evidenceSha256: sha("d"),
    pgStatuses: prerequisite ? [] : groups.map((group) => ({ group, status: "PASS" })),
    diagnostics: [],
    environment: { runnerName: "ignored-diagnostic" },
    ...overrides,
  };
}

const completeRecords = () => ["schema", "protocol", "semantic", "web", "windows", "android", "ios", "ipados"].map((subject) => record(subject));

test("full run-set fixes four families and keeps iOS/iPadOS as five independent profiles", () => {
  const manifest = runSet();
  assert.equal(manifest.authority, "G0_WIRING_ONLY");
  assert.deepEqual(manifest.requiredProfiles.map(({ profileKey }) => profileKey), ["web", "windows", "android", "ios", "ipados"]);
  assert.deepEqual([...new Set(manifest.requiredProfiles.map(({ platformFamily }) => platformFamily))], ["WEB", "WINDOWS", "ANDROID", "APPLE"]);
  assert.equal(manifest.requiredProfiles.find(({ profileKey }) => profileKey === "ios").profileId, "ios-rn-objcxx-reference-v0-1");
  assert.equal(manifest.requiredProfiles.find(({ profileKey }) => profileKey === "ipados").profileId, "ipados-rn-objcxx-reference-v0-1");
  assert.equal(manifest.requiredProfiles.every(({ requiredReality }) => requiredReality === "HOSTED"), true);
  assert.equal(runSet("RELEASE").requiredProfiles.every(({ requiredReality }) => requiredReality === "PHYSICAL"), true);
});

test("missing iPadOS is invalid evidence and iPhone does not substitute for it", () => {
  const manifest = runSet();
  const index = createPlatformEvidenceIndex(manifest, completeRecords().filter(({ subject }) => subject !== "ipados"));
  const decision = aggregatePlatformRelease(manifest, index);
  assert.deepEqual(index.missingSubjects, ["ipados"]);
  assert.equal(decision.decision, "INVALID_EVIDENCE");
  assert.ok(decision.blockingReasons.includes("MISSING_REQUIRED_SUBJECT:ipados"));
});

test("release hosted evidence is blocked instead of being presented as physical PASS", () => {
  const manifest = runSet("RELEASE");
  const index = createPlatformEvidenceIndex(manifest, completeRecords());
  const decision = aggregatePlatformRelease(manifest, index);
  assert.equal(index.validationIssues.length, 0);
  assert.equal(decision.decision, "BLOCKED_AUTHORITY");
  assert.deepEqual(decision.blockingReasons, [
    "REQUIRED_REALITY_MISSING:web:PHYSICAL",
    "REQUIRED_REALITY_MISSING:windows:PHYSICAL",
    "REQUIRED_REALITY_MISSING:android:PHYSICAL",
    "REQUIRED_REALITY_MISSING:ios:PHYSICAL",
    "REQUIRED_REALITY_MISSING:ipados:PHYSICAL",
  ]);
});

test("decision precedence is invalid, authority blocked, correctness fail, observations, pass", () => {
  const manifest = runSet();
  const decide = (records) => aggregatePlatformRelease(manifest, createPlatformEvidenceIndex(manifest, records)).decision;
  assert.equal(decide(completeRecords().map((value) => value.subject === "web" ? { ...value, evidenceSha256: "bad" } : value)), "INVALID_EVIDENCE");
  assert.equal(decide(completeRecords().map((value) => value.subject === "web" ? { ...value, status: "BLOCKED_AUTHORITY" } : value)), "BLOCKED_AUTHORITY");
  assert.equal(decide(completeRecords().map((value) => value.subject === "web" ? { ...value, status: "FAIL" } : value)), "FAIL");
  assert.equal(decide(completeRecords().map((value) => value.subject === "web" ? { ...value, status: "PASS_WITH_OBSERVATIONS" } : value)), "PASS_WITH_OBSERVATIONS");
  assert.equal(decide(completeRecords()), "PASS");
});

test("same identity compares deterministically while environment diagnostics are excluded", () => {
  const manifest = runSet();
  const left = aggregatePlatformRelease(manifest, createPlatformEvidenceIndex(manifest, completeRecords()));
  const changedEnvironment = completeRecords().map((value) => ({ ...value, environment: { runnerName: "another-runner", elapsedMs: 1000 } }));
  const right = aggregatePlatformRelease(manifest, createPlatformEvidenceIndex(manifest, changedEnvironment));
  const first = comparePlatformReleaseDecisions(left, right);
  const second = comparePlatformReleaseDecisions(left, right);
  assert.deepEqual(first, second);
  assert.equal(first.comparable, true);
  assert.equal(first.status, "PASS");
  assert.equal(first.blockingConclusionMatch, true);
  assert.deepEqual(first.correctnessDifferences, []);
});

test("different source identity is rejected as invalid reproducibility evidence", () => {
  const manifest = runSet();
  const left = aggregatePlatformRelease(manifest, createPlatformEvidenceIndex(manifest, completeRecords()));
  const otherManifest = { ...manifest, sourceCommit: "e".repeat(40) };
  const otherRecords = completeRecords().map((value) => ({ ...value, sourceCommit: otherManifest.sourceCommit }));
  const right = aggregatePlatformRelease(otherManifest, createPlatformEvidenceIndex(otherManifest, otherRecords));
  const comparison = comparePlatformReleaseDecisions(left, right);
  assert.equal(comparison.comparable, false);
  assert.equal(comparison.status, "INVALID_EVIDENCE");
  assert.deepEqual(comparison.correctnessDifferences, ["IDENTITY_MISMATCH:sourceCommit"]);
});
