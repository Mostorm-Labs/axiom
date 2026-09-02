import assert from "node:assert/strict";
import { execFileSync, spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import {
  assertMaterializationPath,
  validateCiRunRecord,
} from "../../../tools/generate_g1_04_c8_evidence.mjs";

const PACKAGE_REF = "5ef414e3dc5b8f4cc42327543b3b1b978ff9a0cc";
const TASK_ANCHOR = "34c8db4f247849c5850e16226b0e556f57497053";
const REPOSITORY_ROOT = fileURLToPath(new URL("../../../../", import.meta.url));
const GENERATOR = fileURLToPath(new URL("../../../tools/generate_g1_04_c8_evidence.mjs", import.meta.url));
const INHERITED_OBSERVATION_REF = "9b73be589ae070bc602b8989f83d89745a54774e";
const INHERITED_OBSERVATION_PATH = "verification/evidence/gates/G1/492d2f914f078a6e4ac8b567e07f7ec813c10107/GT-G1-04-C/C-PLAN-PROJECTION.json";
const SOURCE_REF = "a".repeat(40);
const BRANCH_REF = "refs/heads/codex/gt-g1-04-c8-exact-source-ci";
const REQUIRED_EVIDENCE_FILES = [
  "C-CORE-CORPUS.json",
  "C-IDEMPOTENCY.json",
  "C-NO-MUTATION.json",
  "C-PLAN-PROJECTION.json",
  "C-OPEN-RECONCILIATION.json",
  "C-PROVIDER-DIFF.json",
  "C-GATE.json",
  "C-CI-RUN.json",
];

function ciRun(overrides = {}) {
  return {
    format: "axiom-g1-04-c-ci-run-v1",
    formatVersion: 1,
    gate: "GT-G1-04-C",
    slice: "C8",
    packageRef: PACKAGE_REF,
    taskAnchor: { revision: TASK_ANCHOR, relation: "ancestor" },
    sourceRef: SOURCE_REF,
    repository: "Mostorm-Labs/axiom",
    workflowName: "GT-G1-04-C Exact Source",
    workflowPath: ".github/workflows/g1-04-c-exact-source.yml",
    workflowRef: `${BRANCH_REF}/.github/workflows/g1-04-c-exact-source.yml@${SOURCE_REF}`,
    event: "push",
    ref: BRANCH_REF,
    runId: "123456789",
    runAttempt: "1",
    hostedRunUrl: "https://github.com/Mostorm-Labs/axiom/actions/runs/123456789",
    headSha: SOURCE_REF,
    checkoutSha: SOURCE_REF,
    artifactName: `gt-g1-04-c8-${SOURCE_REF}`,
    requiredEvidenceFiles: REQUIRED_EVIDENCE_FILES,
    verificationResults: {
      semanticCli: "PASS",
      fixtureReproducibility: "PASS",
      fullCTest: "PASS",
      nativeObservation: "PASS",
      independentEvaluation: "PASS",
    },
    expectedTruthWrites: 0,
    providerOutputUsedAsExpected: false,
    productionSemanticDelta: 0,
    authorityDelta: 0,
    verdict: "PASS",
    ...overrides,
  };
}

function validate(record) {
  return validateCiRunRecord(record, {
    packageRef: PACKAGE_REF,
    taskAnchor: TASK_ANCHOR,
    sourceRef: SOURCE_REF,
    branchRef: BRANCH_REF,
  });
}

test("C8 accepts an exact-source authoritative push record", () => {
  assert.deepEqual(validate(ciRun()), ciRun());
});

test("C8 rejects a source, run head, or checkout identity mismatch", () => {
  assert.throws(() => validate(ciRun({ headSha: "b".repeat(40) })), /headSha|sourceRef|identity/i);
  assert.throws(() => validate(ciRun({ checkoutSha: "c".repeat(40) })), /checkoutSha|sourceRef|identity/i);
  assert.throws(() => validateCiRunRecord(ciRun(), {
    packageRef: PACKAGE_REF,
    taskAnchor: TASK_ANCHOR,
    sourceRef: "d".repeat(40),
    branchRef: BRANCH_REF,
  }), /sourceRef|foreign|identity/i);
});

test("C8 rejects non-authoritative event, branch, package, or task anchor", () => {
  assert.throws(() => validate(ciRun({ event: "workflow_dispatch" })), /event|push/i);
  assert.throws(() => validate(ciRun({ ref: "refs/heads/main" })), /ref|branch/i);
  assert.throws(() => validate(ciRun({ packageRef: "e".repeat(40) })), /packageRef|package/i);
  assert.throws(() => validate(ciRun({ taskAnchor: { revision: "f".repeat(40), relation: "ancestor" } })), /taskAnchor|anchor/i);
});

test("C8 rejects incomplete durable evidence and local-only CI identity", () => {
  assert.throws(() => validate(ciRun({ requiredEvidenceFiles: REQUIRED_EVIDENCE_FILES.slice(0, -1) })), /evidence/i);
  assert.throws(() => validate(ciRun({ hostedRunUrl: "file:///tmp/local-run" })), /hostedRunUrl|hosted|GitHub/i);
  assert.throws(() => validate(ciRun({ runId: "" })), /runId/i);
});

test("C8 rejects expected-truth, provider, semantic, or Authority contamination", () => {
  assert.throws(() => validate(ciRun({ expectedTruthWrites: 1 })), /expectedTruthWrites/i);
  assert.throws(() => validate(ciRun({ providerOutputUsedAsExpected: true })), /providerOutputUsedAsExpected/i);
  assert.throws(() => validate(ciRun({ productionSemanticDelta: 1 })), /productionSemanticDelta/i);
  assert.throws(() => validate(ciRun({ authorityDelta: 1 })), /authorityDelta/i);
});

test("C8 permits only evidence paths directly under the source-bound materialization root", () => {
  const root = `verification/evidence/gates/G1/${SOURCE_REF}/GT-G1-04-C`;
  assert.equal(assertMaterializationPath(root, "C-CI-RUN.json"), `${root}/C-CI-RUN.json`);
  assert.throws(() => assertMaterializationPath(root, "../C-CI-RUN.json"), /escape|path/i);
  assert.throws(() => assertMaterializationPath(root, "nested/C-CI-RUN.json"), /exactly one|path/i);
  assert.throws(() => assertMaterializationPath(root, "C-OTHER.json"), /required|evidence/i);
});

test("C8 generator consumes the native observation envelope and emits exactly eight artifacts", () => {
  const sourceRef = execFileSync("git", ["rev-parse", "HEAD"], { cwd: REPOSITORY_ROOT, encoding: "utf8" }).trim();
  const temp = mkdtempSync(join(tmpdir(), "g1-04-c8-e2e-"));
  const observation = join(temp, "observation.json");
  const output = join(temp, "evidence");
  const inherited = execFileSync("git", ["show", `${INHERITED_OBSERVATION_REF}:${INHERITED_OBSERVATION_PATH}`], { cwd: REPOSITORY_ROOT, encoding: "utf8" });
  writeFileSync(observation, inherited);
  const result = spawnSync(process.execPath, [
    GENERATOR,
    "--package-ref", PACKAGE_REF,
    "--task-anchor", TASK_ANCHOR,
    "--source-ref", sourceRef,
    "--observation", observation,
    "--output-dir", output,
    "--ci-run-id", "123456789",
    "--ci-run-attempt", "1",
    "--ci-event", "push",
    "--ci-ref", BRANCH_REF,
    "--ci-head-sha", sourceRef,
    "--checkout-sha", sourceRef,
    "--workflow-ref", `${BRANCH_REF}/.github/workflows/g1-04-c-exact-source.yml@${sourceRef}`,
  ], { cwd: REPOSITORY_ROOT, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.deepEqual(readdirSync(output).sort(), REQUIRED_EVIDENCE_FILES.slice().sort());
  const core = JSON.parse(readFileSync(join(output, "C-CORE-CORPUS.json"), "utf8"));
  assert.equal(core.sourceRef, sourceRef);
  assert.equal(core.selectedCaseCount, 90);
  assert.deepEqual(core.resultStatusCounts, { PASS: 90, FAIL: 0, OBSERVATION_ONLY: 0 });
});
