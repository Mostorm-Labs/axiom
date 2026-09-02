import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { join } from "node:path";
import { runCoreCorpus, validateCoreCoverage } from "../dist/core-corpus.js";

const repoRoot = fileURLToPath(new URL("../../../../", import.meta.url));
const verificationRoot = join(repoRoot, "verification");
const corpusRoot = join(verificationRoot, "corpus/semantic/v1/g1-04-c");
const c3EvidencePath = join(
  verificationRoot,
  "evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/C-PLAN-PROJECTION.json",
);

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function acceptedInput() {
  return {
    cases: readJson(join(corpusRoot, "authoring/cases.json")),
    expected: readJson(join(corpusRoot, "authoring/expected.json")),
    suite: readJson(join(corpusRoot, "suites/core.json")),
    observations: readJson(c3EvidencePath).observationRecords,
  };
}

function coverage(input = acceptedInput()) {
  return validateCoreCoverage(input);
}

function assertCoverageBaseline(report) {
  assert.equal(report.operationFamilies.length, 15);
  assert.deepEqual(report.missingMandatoryFamilies, []);
  assert.deepEqual(report.wrongFamilyCaseIds, []);
  assert.deepEqual(report.unselectedCaseIds, []);
  assert.deepEqual(report.duplicateSuiteCaseIds, []);
}

test("accepted C5 inventory covers 90 cases, 15 operation families, and all mandatory anchors", () => {
  const input = acceptedInput();
  const report = coverage(input);
  assert.equal(input.cases.length, 90);
  assert.equal(input.expected.length, 90);
  assert.equal(input.suite.caseIds.length, 90);
  assert.equal(input.observations.length, 180);
  assertCoverageBaseline(report);
});

test("missing, extra, and duplicate suite IDs are reported", () => {
  const input = acceptedInput();
  assert.ok(coverage({ ...input, suite: { ...input.suite, caseIds: input.suite.caseIds.slice(1) } }).unselectedCaseIds.includes(input.suite.caseIds[0]));
  assert.ok(coverage({ ...input, suite: { ...input.suite, caseIds: [...input.suite.caseIds, "C1-EXTRA"] } }).unselectedCaseIds.includes("C1-EXTRA"));
  assert.ok(coverage({ ...input, suite: { ...input.suite, caseIds: [input.suite.caseIds[0], input.suite.caseIds[0], ...input.suite.caseIds.slice(1)] } }).duplicateSuiteCaseIds.includes(input.suite.caseIds[0]));
});

test("missing and duplicate expected records are reported", () => {
  const input = acceptedInput();
  assert.ok(coverage({ ...input, expected: input.expected.slice(1) }).unselectedCaseIds.includes(input.expected[0].caseId));
  assert.ok(coverage({ ...input, expected: [input.expected[0], ...input.expected] }).duplicateSuiteCaseIds.includes(input.expected[0].caseId));
});

test("wrong operation-family mapping and missing mandatory anchor are reported", () => {
  const input = acceptedInput();
  const wrongCases = input.cases.map((record) => record.id === "C1-INSERT-VALID" ? { ...record, operationFamily: "DeleteObjects" } : record);
  const wrongReport = coverage({ ...input, cases: wrongCases });
  assert.ok(wrongReport.wrongFamilyCaseIds.includes("C1-INSERT-VALID"));

  const missingAnchorSuite = { ...input.suite, caseIds: input.suite.caseIds.filter((id) => id !== "C1-INSERT-VALID") };
  const missingReport = coverage({ ...input, suite: missingAnchorSuite });
  assert.ok(missingReport.missingMandatoryFamilies.length > 0);
});

test("Reference and Indexed observations must form exactly one pair per selected case", () => {
  const input = acceptedInput();
  const first = input.suite.caseIds[0];
  const withoutReference = input.observations.filter((observation) => !(observation.caseId === first && observation.provider === "reference"));
  const withoutIndexed = input.observations.filter((observation) => !(observation.caseId === first && observation.provider === "indexed"));
  const duplicateProvider = [...input.observations, input.observations.find((observation) => observation.caseId === first && observation.provider === "reference")];
  assert.throws(() => runCoreCorpus({ ...input, observations: withoutReference, c3Evidence: { format: "axiom-gt-g1-04-c-plan-projection-v2", formatVersion: 2, factsOnly: true, acceptedCases: 90, observationCount: 180, noMutationObservations: 180, observationRecords: withoutReference }, c3SourceRef: "c26c38feb192a7e584fa60a5ffbedf44f4b6e97a" }), /observation|provider|coverage/i);
  assert.throws(() => runCoreCorpus({ ...input, observations: withoutIndexed, c3Evidence: { format: "axiom-gt-g1-04-c-plan-projection-v2", formatVersion: 2, factsOnly: true, acceptedCases: 90, observationCount: 180, noMutationObservations: 180, observationRecords: withoutIndexed }, c3SourceRef: "c26c38feb192a7e584fa60a5ffbedf44f4b6e97a" }), /observation|provider|coverage/i);
  assert.throws(() => runCoreCorpus({ ...input, observations: duplicateProvider, c3Evidence: { format: "axiom-gt-g1-04-c-plan-projection-v2", formatVersion: 2, factsOnly: true, acceptedCases: 90, observationCount: 181, noMutationObservations: 180, observationRecords: duplicateProvider }, c3SourceRef: "c26c38feb192a7e584fa60a5ffbedf44f4b6e97a" }), /duplicate|observation|provider|coverage/i);
});

test("non-manual expected provenance and OPEN records fail closed", () => {
  const input = acceptedInput();
  const expected = input.expected.map((record, index) => index === 0 ? { ...record, provenance: "IMPLEMENTATION_OBSERVATION" } : record);
  assert.throws(() => runCoreCorpus({ ...input, expected, c3Evidence: readJson(c3EvidencePath), c3SourceRef: "c26c38feb192a7e584fa60a5ffbedf44f4b6e97a" }), /provenance|AUTHORITY_MANUAL|coverage/i);

  const openExpected = input.expected.map((record, index) => index === 0 ? { ...record, openPolicy: true } : record);
  assert.throws(() => runCoreCorpus({ ...input, expected: openExpected, c3Evidence: readJson(c3EvidencePath), c3SourceRef: "c26c38feb192a7e584fa60a5ffbedf44f4b6e97a" }), /OPEN|openPolicy|authority/i);
});

function runInput() {
  const input = acceptedInput();
  return {
    ...input,
    c3Evidence: readJson(c3EvidencePath),
    c3SourceRef: "c26c38feb192a7e584fa60a5ffbedf44f4b6e97a",
  };
}

test("accepted core corpus runs all 90 cases in suite order with stable C3 refs", () => {
  const input = runInput();
  const before = structuredClone(input);
  const run = runCoreCorpus(input);
  assert.equal(run.suiteId, "GT-G1-04-C-CORE");
  assert.equal(run.selectedCaseCount, 90);
  assert.equal(run.selectedExpectedCount, 90);
  assert.equal(run.selectedObservationCount, 180);
  assert.equal(run.operationFamilyCount, 15);
  assert.equal(run.results.length, 90);
  assert.deepEqual(run.results.map((result) => result.caseId), input.suite.caseIds);
  const first = input.suite.caseIds[0];
  assert.deepEqual(run.results[0].observationRefs, [
    `g1-04-c://c3/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/${first}/reference`,
    `g1-04-c://c3/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/${first}/indexed`,
  ]);
  assert.deepEqual(input, before);
});

test("semantic mismatches remain visible as ConformanceResult FAIL without mutating expected truth", () => {
  const input = runInput();
  const target = "C1-INSERT-VALID";
  input.c3Evidence.observationRecords = input.c3Evidence.observationRecords.map((observation) => observation.caseId === target && observation.provider === "reference" ? { ...observation, observedDisposition: "REJECTED" } : observation);
  const expectedBefore = structuredClone(input.expected);
  const run = runCoreCorpus(input);
  const result = run.results.find((entry) => entry.caseId === target);
  assert.equal(result.status, "FAIL");
  assert.ok(result.diagnostics?.includes("REFERENCE_GOLDEN_MISMATCH"));
  assert.deepEqual(input.expected, expectedBefore);
});

test("provider divergence and no-mutation failures remain visible", () => {
  const divergence = runInput();
  const target = "C1-INSERT-VALID";
  divergence.c3Evidence.observationRecords = divergence.c3Evidence.observationRecords.map((observation) => observation.caseId === target && observation.provider === "indexed" ? { ...observation, observedDisposition: "REJECTED" } : observation);
  const divergenceResult = runCoreCorpus(divergence).results.find((entry) => entry.caseId === target);
  assert.equal(divergenceResult.status, "FAIL");
  assert.ok(divergenceResult.diagnostics?.includes("PROVIDER_DIVERGENCE"));

  const mutation = runInput();
  mutation.c3Evidence.observationRecords = mutation.c3Evidence.observationRecords.map((observation) => observation.caseId === target && observation.provider === "reference" ? { ...observation, afterProjection: { changed: true } } : observation);
  const mutationResult = runCoreCorpus(mutation).results.find((entry) => entry.caseId === target);
  assert.equal(mutationResult.status, "FAIL");
  assert.ok(mutationResult.diagnostics?.includes("REFERENCE_MUTATION"));
});
