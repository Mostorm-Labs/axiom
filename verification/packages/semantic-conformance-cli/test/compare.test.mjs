import test from "node:test";
import assert from "node:assert/strict";
import { compareObservationToExpected, compareProviders, deepEqualJson } from "../dist/compare.js";

const expected = { disposition: "PLAN_READY", terminalPhase: "PREPARE" };
const observation = { observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", observedErrorCategory: "EXTRA", observedPlanProjection: { extra: true } };

test("exact expected match has no diagnostics", () => assert.deepEqual(compareObservationToExpected(expected, observation), []));
test("disposition mismatch is detected", () => assert.deepEqual(compareObservationToExpected(expected, { ...observation, observedDisposition: "REJECTED" }), ["DISPOSITION_MISMATCH"]));
test("terminal phase mismatch is detected", () => assert.deepEqual(compareObservationToExpected(expected, { ...observation, observedTerminalPhase: "NORMALIZE" }), ["TERMINAL_PHASE_MISMATCH"]));
test("asserted semantic error category is compared", () => assert.deepEqual(compareObservationToExpected({ ...expected, semanticErrorCategory: "A" }, { ...observation, observedErrorCategory: "B" }), ["SEMANTIC_ERROR_CATEGORY_MISMATCH"]));
test("unasserted semantic error category is ignored", () => assert.deepEqual(compareObservationToExpected(expected, { ...observation, observedErrorCategory: "B" }), []));
test("asserted logical projection is compared", () => assert.deepEqual(compareObservationToExpected({ ...expected, logicalPlanProjection: { a: 1 } }, { ...observation, observedPlanProjection: { a: 2 } }), ["LOGICAL_PLAN_PROJECTION_MISMATCH"]));
test("unasserted logical projection is ignored", () => assert.deepEqual(compareObservationToExpected(expected, { ...observation, observedPlanProjection: { implementationOnly: true } }), []));
test("object key order is structural", () => assert.equal(deepEqualJson({ a: 1, b: 2 }, { b: 2, a: 1 }), true));
test("array order remains significant and provider parity uses authority fields", () => {
  assert.equal(deepEqualJson([1, 2], [2, 1]), false);
  assert.deepEqual(compareProviders({ ...expected, logicalPlanProjection: [1, 2] }, { ...observation, observedPlanProjection: [1, 2] }, { ...observation, observedPlanProjection: [2, 1] }), ["PROVIDER_DIVERGENCE"]);
});
