import assert from "node:assert/strict";
import test from "node:test";

import {
  assertMaterializationPath,
  validateCiRunRecord,
} from "../../../tools/generate_g1_04_c8_evidence.mjs";

const PACKAGE_REF = "5ef414e3dc5b8f4cc42327543b3b1b978ff9a0cc";
const TASK_ANCHOR = "34c8db4f247849c5850e16226b0e556f57497053";
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
