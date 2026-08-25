import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import test from "node:test";
import { createG0GateReport } from "../dist/ci/GateReport.js";

const sourceCommit = "a".repeat(40);
const artifact = "b".repeat(64);
const taskLineage = Array.from({ length: 16 }, (_, index) => ({
  taskId: `GT-G0-${String(index).padStart(2, "0")}`,
  status: "Pass",
  evidencePath: `verification/evidence/g0/gt-g0-${String(index).padStart(2, "0")}.json`,
  evidenceSha256: artifact,
}));

const base = () => createG0GateReport({
  sourceCommit,
  branch: "main",
  schemaVersion: "axiom-gate-report-v1",
  generatorVersion: "0.1.0",
  corpus: { schemaSha256: artifact, corpusSha256: artifact, runnerVersion: "0.1.0", runtimeVersion: "0.1.0" },
  taskLineage,
  platforms: [{ subject: "web", platformFamily: "WEB", profileId: "web-reference-v0-1", reality: "HOSTED", evidencePath: "verification/evidence/g0/gt-g0-15/evidence-index.json", environment: { runnerName: "hosted" } }],
  hosted: { nightlyDecision: "PASS", releaseDecision: "BLOCKED_AUTHORITY", reproducibility: "PASS" },
  artifacts: [{ path: "artifact.json", bytes: 7, sha256: createHash("sha256").update("fixture").digest("hex") }],
});

test("G0 report is deterministic and records E1-E4 plus lineage", () => {
  const report = base();
  assert.equal(report.format, "axiom-gate-report-v1");
  assert.equal(report.gate, "G0");
  assert.equal(report.status, "BLOCKED");
  assert.deepEqual(report.checks.map(({ level }) => level), ["E1", "E2", "E3", "E4"]);
  assert.equal(report.checks.find(({ level }) => level === "E4").status, "BLOCKED");
  assert.equal(report.lineage.length, 16);
  assert.equal(report.platforms[0].subject, "web");
  assert.equal(report.promotion.allowed, false);
  assert.ok(report.issues.some((issue) => issue.startsWith("PHYSICAL_RELEASE_AUTHORITY_BLOCKED")));
});

test("package-bound physical authority passes without a current-commit rebuild", () => {
  const platforms = [
    ["android", "ANDROID", "android-instrumentation-reference-v0-1"],
    ["ios", "APPLE", "ios-rn-objcxx-reference-v0-1"],
    ["ipados", "APPLE", "ipados-rn-objcxx-reference-v0-1"],
    ["web", "WEB", "web-reference-v0-1"],
    ["windows", "WINDOWS", "windows-native-reference-v0-1"],
  ].map(([subject, platformFamily, profileId]) => ({
    subject, platformFamily, profileId, reality: "PHYSICAL",
    evidencePath: `docs/quality/evidence/${subject}.md`,
    environment: { packageIdentity: `installed-${subject}-package`, fixtureDigest: "47826449b895ac4f4a57b4f386379775", artifactSha256: artifact },
  }));
  const report = createG0GateReport({
    sourceCommit, branch: "main", schemaVersion: "axiom-gate-report-v1", generatorVersion: "0.1.0",
    corpus: { schemaSha256: artifact, corpusSha256: artifact, runnerVersion: "0.1.0", runtimeVersion: "0.1.0" },
    taskLineage, platforms,
    hosted: { nightlyDecision: "PASS", releaseDecision: "BLOCKED_AUTHORITY", reproducibility: "PASS" },
    artifacts: [{ path: "artifact.json", bytes: 7, sha256: createHash("sha256").update("fixture").digest("hex") }],
  });
  assert.equal(report.checks.find(({ level }) => level === "E4").status, "PASS");
  assert.equal(report.status, "PASS");
  assert.equal(report.promotion.allowed, true);
});

test("G0 report rejects incomplete task lineage", () => {
  assert.throws(() => createG0GateReport({ ...base(), taskLineage: taskLineage.slice(1) }), /missing required task GT-G0-00/);
});

test("G0 report rejects non-commit-bound source identity", () => {
  assert.throws(() => createG0GateReport({ ...base(), sourceCommit: "WORKTREE" }), /sourceCommit/);
});
