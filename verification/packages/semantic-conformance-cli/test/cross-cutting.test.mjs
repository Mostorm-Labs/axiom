import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const packageRoot = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(packageRoot, "../../../..");
const c3Root = join(repoRoot, "verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C");
const r2Root = join(repoRoot, "verification/evidence/gates/G1/1763d57e7554ec690634326b998971f0decaae28/GT-G1-04-C");
const cases = JSON.parse(readFileSync(join(repoRoot, "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json"), "utf8"));
const expected = JSON.parse(readFileSync(join(repoRoot, "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json"), "utf8"));
const coreCorpusEvidence = JSON.parse(readFileSync(join(c3Root, "C-CORE-CORPUS.json"), "utf8"));
const noMutationEvidence = JSON.parse(readFileSync(join(c3Root, "C-NO-MUTATION.json"), "utf8"));
const planProjectionEvidence = JSON.parse(readFileSync(join(c3Root, "C-PLAN-PROJECTION.json"), "utf8"));
const p36RepairEvidence = JSON.parse(readFileSync(join(c3Root, "P36-C6-UPSTREAM-REPAIR.json"), "utf8"));
const p36OverflowLineageEvidence = JSON.parse(readFileSync(join(r2Root, "P36-R2-OVERFLOW-LINEAGE.json"), "utf8"));

const { runCrossCutting } = await import("../dist/cross-cutting.js");

function baseInput() {
  return {
    cases: structuredClone(cases),
    expected: structuredClone(expected),
    coreCorpusEvidence: structuredClone(coreCorpusEvidence),
    noMutationEvidence: structuredClone(noMutationEvidence),
    planProjectionEvidence: structuredClone(planProjectionEvidence),
    p36RepairEvidence: structuredClone(p36RepairEvidence),
    p36OverflowLineageEvidence: structuredClone(p36OverflowLineageEvidence),
  };
}

test("C6 accepts the repaired cross-cutting dependency basis", () => {
  const run = runCrossCutting(baseInput());
  assert.equal(run.idempotency.status, "PASS");
  assert.equal(run.noMutation.acceptedCases, 90);
  assert.equal(run.noMutation.observationCount, 180);
  assert.equal(run.noMutation.beforeAfterEqual, 180);
  assert.equal(run.noMutation.observerMutationCalls, 0);
  assert.equal(run.planProjection.status, "PASS");
  assert.equal(run.openReconciliation.status, "PASS");
});

test("C6 requires the corrected C3 lineage overlay", () => {
  const input = baseInput();
  input.p36OverflowLineageEvidence.C3Lineage.materializedRef = "906327beb9a268c339accd6d3ca6a7038e54ad68";
  assert.throws(() => runCrossCutting(input), /corrected C3 lineage/i);
});

test("C6 requires all 90 accepted cases and 90 expected records", () => {
  const input = baseInput();
  input.cases.pop();
  assert.throws(() => runCrossCutting(input), /90 accepted cases/i);
});

test("C6 rejects non-manual expected truth or mutation claims", () => {
  const input = baseInput();
  input.expected[0].provenance = "IMPLEMENTATION_OBSERVATION";
  assert.throws(() => runCrossCutting(input), /AUTHORITY_MANUAL/i);
  const second = baseInput();
  second.expected[0].mutationExpected = true;
  assert.throws(() => runCrossCutting(second), /mutationExpected/i);
});

test("C6 rejects accepted OPEN expected records", () => {
  const input = baseInput();
  input.expected[0].openPolicy = true;
  assert.throws(() => runCrossCutting(input), /openPolicy/i);
});

test("C6 proves equivalent replay and collision terminate at IDEMPOTENCY", () => {
  const run = runCrossCutting(baseInput());
  assert.deepEqual(run.idempotency.cases, [
    { caseId: "C1-IDEMPOTENT-EQUIVALENT", disposition: "ALREADY_APPLIED", terminalPhase: "IDEMPOTENCY", reference: "PASS", indexed: "PASS" },
    { caseId: "C1-RESTORE-OPID-BEFORE-EXISTENCE", disposition: "ALREADY_APPLIED", terminalPhase: "IDEMPOTENCY", reference: "PASS", indexed: "PASS" },
    { caseId: "C1-ID-COLLISION", disposition: "REJECTED", terminalPhase: "IDEMPOTENCY", reference: "PASS", indexed: "PASS" },
  ]);
  const input = baseInput();
  input.expected.find((record) => record.caseId === "C1-ID-COLLISION").terminalPhase = "STATEFUL_VALIDATE";
  assert.throws(() => runCrossCutting(input), /IDEMPOTENCY/i);
});

test("C6 requires both providers for every accepted observation", () => {
  const input = baseInput();
  input.planProjectionEvidence.observationRecords.pop();
  assert.throws(() => runCrossCutting(input), /180.*observations|provider pair/i);
});

test("C6 aggregates no-mutation facts by all occurring dispositions", () => {
  const run = runCrossCutting(baseInput());
  assert.deepEqual(run.noMutation.byDisposition, {
    PLAN_READY: { cases: 36, observations: 72, unchanged: 72 },
    REJECTED: { cases: 52, observations: 104, unchanged: 104 },
    ALREADY_APPLIED: { cases: 2, observations: 4, unchanged: 4 },
  });
  const input = baseInput();
  input.noMutationEvidence.records[0].afterProjection = { objects: [{ id: "mutation" }] };
  assert.throws(() => runCrossCutting(input), /before.*after|mutation/i);
});

test("C6 requires zero observer mutation calls", () => {
  const input = baseInput();
  input.noMutationEvidence.observerMutationCalls = 1;
  assert.throws(() => runCrossCutting(input), /observer mutation/i);
});

test("C6 proves the exact manual Connector cascade projection", () => {
  const run = runCrossCutting(baseInput());
  assert.equal(run.planProjection.cases.length, 1);
  assert.equal(run.planProjection.cases[0].caseId, "C1-DELETE-CASCADE");
  assert.equal(run.planProjection.cases[0].reference, "PASS");
  assert.equal(run.planProjection.cases[0].indexed, "PASS");
  const input = baseInput();
  const record = input.planProjectionEvidence.observationRecords.find((item) => item.caseId === "C1-DELETE-CASCADE" && item.provider === "reference");
  record.observedPlanProjection.deletes = ["bed0e2f48ca23c0992a9a9ae8f8bf109"];
  assert.throws(() => runCrossCutting(input), /logical plan|cascade|projection/i);
});

test("C6 consumes repaired geometry thresholds and exact overflow proof", () => {
  const run = runCrossCutting(baseInput());
  assert.deepEqual(run.openReconciliation.geometry, {
    thresholds: {
      "C1-GEOMETRY-N-1": { units: 1999999, observed: "PLAN_READY" },
      "C1-GEOMETRY-N": { units: 2000000, observed: "PLAN_READY" },
      "C1-GEOMETRY-LIMIT": { units: 2000001, observed: "GEOMETRY_LIMIT_EXCEEDED" },
    },
    checkedAddition: "INTEGER_OVERFLOW",
    checkedDabMultiplication: "INTEGER_OVERFLOW",
    checkedEraseMultiplication: "INTEGER_OVERFLOW",
    historicalOverflowCaseUsedAsArithmeticOracle: false,
  });
  const input = baseInput();
  input.p36OverflowLineageEvidence.arithmeticOverflow.checkedAddition.observed = "GEOMETRY_LIMIT_EXCEEDED";
  assert.throws(() => runCrossCutting(input), /checked addition/i);
});

test("C6 proves Restore no-tombstone and hierarchy cross-operation anchors", () => {
  const run = runCrossCutting(baseInput());
  assert.equal(run.openReconciliation.restoreNoTombstone, "PASS");
  assert.equal(run.openReconciliation.hierarchy.status, "PASS");
  assert.deepEqual(run.openReconciliation.closedGroups, [
    "connector-target-delete",
    "geometry-point-like-elements-per-operation-aggregate",
    "restore-no-tombstone",
    "hierarchy-parent-capability",
    "sticky-direct-richtext-cardinality",
  ]);
  const input = baseInput();
  input.p36RepairEvidence.sticky.cardinality = "corrupt";
  assert.throws(() => runCrossCutting(input), /Sticky|cardinality/i);
});

test("C6 does not accept provider agreement as a substitute for manual truth", () => {
  const input = baseInput();
  input.coreCorpusEvidence.providerOutputUsedAsExpected = true;
  assert.throws(() => runCrossCutting(input), /provider output/i);
  const second = baseInput();
  const expectedRecord = second.expected.find((record) => record.caseId === "C1-ID-COLLISION");
  expectedRecord.terminalPhase = "STATEFUL_VALIDATE";
  assert.throws(() => runCrossCutting(second), /IDEMPOTENCY/i);
});
