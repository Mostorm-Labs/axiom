import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { summarizeProviderDiff } from "../dist/provider-diff.js";

const repoRoot = fileURLToPath(new URL("../../../../", import.meta.url));
const c5Path = `${repoRoot}/verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C/C-CORE-CORPUS.json`;
const c6Path = `${repoRoot}/verification/evidence/gates/G1/2bd2a2fa6502163d471995147daa683cefd7cf8f/GT-G1-04-C/C-NO-MUTATION.json`;

const expectedGoldenMismatches = [
  "C1-CONNECTOR-VALID",
  "C1-ERASE-ADD-VALID",
  "C1-ERASE-REMOVE-VALID",
  "C1-ERASE-REMOVE-WHOLE-REJECT",
  "C1-GEOMETRY-OVERFLOW",
  "C1-GEOMETRY-WRONG-KIND",
  "C1-IMAGE-CONTENTMODE",
  "C1-IMAGE-INTRINSIC",
  "C1-IMAGE-LOCAL-SIZE",
  "C1-IMAGE-RUNTIME-RESOURCE-NONSEMANTIC",
  "C1-IMAGE-SOURCE-RECT",
  "C1-IMAGE-VALID",
  "C1-INSERT-STAGED-PARENT",
  "C1-PATCH-PRESENCE-DEFAULT",
  "C1-PLACEMENT-ORDERKEY",
  "C1-RESTORE-LOCAL-REPLAY-REMOTE",
  "C1-RESTORE-SAME-PAYLOAD-NEW-OPID",
  "C1-RESTORE-STAGED-PARENT-CHILD",
  "C1-RICHTEXT-INVALID-STEP",
  "C1-RICHTEXT-STABLE-REFS",
  "C1-RICHTEXT-UTF8-STYLE",
  "C1-RICHTEXT-VALID",
  "C1-SIZE-HARD-LIMIT",
  "C1-SPLIT-PLAN",
  "C1-SPLIT-REPLACEMENT-COLLISION",
  "C1-SPLIT-SOURCE-MISSING",
  "C1-STROKE-EXISTING-ID",
  "C1-STROKE-NEW-ID",
  "C1-STROKE-VALID",
];

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function acceptedInput() {
  return {
    coreCorpusEvidence: JSON.parse(readFileSync(c5Path, "utf8")),
    c6NoMutationEvidence: JSON.parse(readFileSync(c6Path, "utf8")),
  };
}

test("provider parity remains PASS while the 29 manual-golden mismatches remain FAIL", () => {
  const summary = summarizeProviderDiff(acceptedInput());
  assert.equal(summary.status, "PASS");
  assert.equal(summary.caseCount, 90);
  assert.equal(summary.providerPairCount, 90);
  assert.equal(summary.providerAgreement, "90/90");
  assert.equal(summary.divergenceCount, 0);
  assert.deepEqual(summary.divergenceCaseIds, []);
  assert.equal(summary.goldenPassCount, 61);
  assert.equal(summary.goldenFailCount, 29);
  assert.equal(summary.observationOnlyCount, 0);
  assert.equal(summary.manualGoldenCorrectness, "FAIL");
  assert.deepEqual(summary.goldenMismatchCaseIds, expectedGoldenMismatches);
});

test("a provider divergence fails only provider parity and cannot repair a golden mismatch", () => {
  const input = acceptedInput();
  const mismatch = input.coreCorpusEvidence.results.find((result) => result.status === "FAIL");
  mismatch.diagnostics.push("PROVIDER_DIVERGENCE");
  const summary = summarizeProviderDiff(input);
  assert.equal(summary.status, "FAIL");
  assert.equal(summary.manualGoldenCorrectness, "FAIL");
  assert.equal(summary.divergenceCount, 1);
  assert.deepEqual(summary.divergenceCaseIds, [mismatch.caseId]);
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
  const first = summarizeProviderDiff(acceptedInput());
  const reordered = acceptedInput();
  reordered.coreCorpusEvidence.results.reverse();
  const second = summarizeProviderDiff(reordered);
  assert.deepEqual(second.goldenMismatchCaseIds, first.goldenMismatchCaseIds);
  assert.deepEqual(second.goldenMismatchCaseIds, expectedGoldenMismatches);
});

test("provider differential requires the accepted C6 no-mutation aggregate", () => {
  const input = acceptedInput();
  input.c6NoMutationEvidence.providerAgreement = "89/90";
  assert.throws(() => summarizeProviderDiff(input));
});
