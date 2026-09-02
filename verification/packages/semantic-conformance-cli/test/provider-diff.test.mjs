import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { summarizeProviderDiff } from "../dist/provider-diff.js";

const repoRoot = fileURLToPath(new URL("../../../../", import.meta.url));
const coreCorpusPath = `${repoRoot}/verification/evidence/gates/G1/492d2f914f078a6e4ac8b567e07f7ec813c10107/GT-G1-04-C/C-CORE-CORPUS.json`;
const noMutationPath = `${repoRoot}/verification/evidence/gates/G1/492d2f914f078a6e4ac8b567e07f7ec813c10107/GT-G1-04-C/C-NO-MUTATION.json`;

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function acceptedInput() {
  return {
    coreCorpusEvidence: JSON.parse(readFileSync(coreCorpusPath, "utf8")),
    noMutationEvidence: JSON.parse(readFileSync(noMutationPath, "utf8")),
  };
}

test("accepted P36 basis reports passing manual golden and provider parity", () => {
  const summary = summarizeProviderDiff(acceptedInput());
  assert.equal(summary.status, "PASS");
  assert.equal(summary.caseCount, 90);
  assert.equal(summary.providerPairCount, 90);
  assert.equal(summary.providerAgreement, "90/90");
  assert.equal(summary.divergenceCount, 0);
  assert.deepEqual(summary.divergenceCaseIds, []);
  assert.equal(summary.goldenPassCount, 90);
  assert.equal(summary.goldenFailCount, 0);
  assert.equal(summary.observationOnlyCount, 0);
  assert.equal(summary.manualGoldenCorrectness, "PASS");
  assert.deepEqual(summary.goldenMismatchCaseIds, []);
});

test("a provider divergence fails provider parity and records the failing result inventory", () => {
  const input = acceptedInput();
  const mismatch = input.coreCorpusEvidence.results[0];
  mismatch.status = "FAIL";
  mismatch.diagnostics = ["PROVIDER_DIVERGENCE"];
  input.coreCorpusEvidence.resultStatusCounts = { PASS: 89, FAIL: 1, OBSERVATION_ONLY: 0 };
  const summary = summarizeProviderDiff(input);
  assert.equal(summary.status, "FAIL");
  assert.equal(summary.manualGoldenCorrectness, "FAIL");
  assert.equal(summary.goldenPassCount, 89);
  assert.equal(summary.goldenFailCount, 1);
  assert.equal(summary.divergenceCount, 1);
  assert.deepEqual(summary.divergenceCaseIds, [mismatch.caseId]);
});

test("a synthetic golden mismatch is counted independently from provider divergence", () => {
  const input = acceptedInput();
  const mismatch = input.coreCorpusEvidence.results[0];
  mismatch.status = "FAIL";
  mismatch.diagnostics = ["REFERENCE_GOLDEN_MISMATCH"];
  input.coreCorpusEvidence.resultStatusCounts = { PASS: 89, FAIL: 1, OBSERVATION_ONLY: 0 };
  const summary = summarizeProviderDiff(input);
  assert.equal(summary.status, "PASS");
  assert.equal(summary.manualGoldenCorrectness, "FAIL");
  assert.equal(summary.goldenPassCount, 89);
  assert.equal(summary.goldenFailCount, 1);
  assert.deepEqual(summary.goldenMismatchCaseIds, [mismatch.caseId]);
});

test("provider differential fails closed on an altered corpus inventory or missing provider pair", () => {
  const badInventory = acceptedInput();
  badInventory.coreCorpusEvidence.selectedObservationCount = 179;
  assert.throws(() => summarizeProviderDiff(badInventory), /inventory/i);

  const missingPair = acceptedInput();
  missingPair.coreCorpusEvidence.results[0].observationRefs.pop();
  assert.throws(() => summarizeProviderDiff(missingPair), /provider pair/i);

  const duplicateProvider = acceptedInput();
  const [reference] = duplicateProvider.coreCorpusEvidence.results[0].observationRefs;
  duplicateProvider.coreCorpusEvidence.results[0].observationRefs = [reference, `${reference}-second`];
  assert.throws(() => summarizeProviderDiff(duplicateProvider), /provider pair/i);
});

test("provider differential rejects OPEN truth, invalid provenance, expected-truth writes, and unexpected diagnostics", () => {
  for (const mutate of [
    (input) => { input.coreCorpusEvidence.acceptedOpenPolicyCount = 1; },
    (input) => { input.coreCorpusEvidence.acceptedExpectedAllAuthorityManual = false; },
    (input) => { input.coreCorpusEvidence.expectedTruthWrites = 1; },
    (input) => { input.coreCorpusEvidence.results[0].diagnostics = ["UNEXPECTED_DIAGNOSTIC"]; },
  ]) {
    const input = acceptedInput();
    mutate(input);
    assert.throws(() => summarizeProviderDiff(input));
  }
});

test("golden mismatch inventory is deterministic and independently sorted", () => {
  const input = acceptedInput();
  const ids = input.coreCorpusEvidence.results.slice(0, 3).map((result) => result.caseId);
  for (const [index, result] of input.coreCorpusEvidence.results.slice(0, 3).entries()) {
    result.status = "FAIL";
    result.diagnostics = ["REFERENCE_GOLDEN_MISMATCH"];
    ids[index] = result.caseId;
  }
  input.coreCorpusEvidence.resultStatusCounts = { PASS: 87, FAIL: 3, OBSERVATION_ONLY: 0 };
  const first = summarizeProviderDiff(input);
  const reordered = clone(input);
  reordered.coreCorpusEvidence.results.reverse();
  const second = summarizeProviderDiff(reordered);
  assert.deepEqual(second.goldenMismatchCaseIds, first.goldenMismatchCaseIds);
  assert.deepEqual(second.goldenMismatchCaseIds, [...ids].sort());
});

test("provider differential rejects invalid no-mutation aggregate", () => {
  const input = acceptedInput();
  input.noMutationEvidence.beforeAfterEqual = "179/180";
  assert.throws(() => summarizeProviderDiff(input), /no-mutation/i);
});

test("provider differential requires the accepted P36 no-mutation aggregate", () => {
  const input = acceptedInput();
  input.noMutationEvidence.acceptedCases = 89;
  assert.throws(() => summarizeProviderDiff(input));
});
