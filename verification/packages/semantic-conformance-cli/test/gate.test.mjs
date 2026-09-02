import test from "node:test";
import assert from "node:assert/strict";
import { evaluateC7Gate } from "../dist/gate.js";

const ids = [
  "authority-provenance",
  "mandatory-corpus",
  "manual-golden-correctness",
  "no-mutation",
  "provider-differential",
  "fixture-reproducibility",
  "open-reconciliation",
];

function gateInput(overrides = {}) {
  return {
    conditions: ids.map((id) => ({ id, status: overrides[id] ?? "PASS", evidenceRefs: [`g1-04-c://${id}`] })),
  };
}

test("all seven passing conditions produce a passing C Gate", () => {
  const summary = evaluateC7Gate(gateInput());
  assert.equal(summary.status, "PASS");
  assert.deepEqual(summary.failedConditions, []);
  assert.deepEqual(summary.conditions.map((condition) => condition.id), ids);
});

test("manual-golden correctness fails the C Gate even when provider parity passes", () => {
  const summary = evaluateC7Gate(gateInput({ "manual-golden-correctness": "FAIL" }));
  assert.equal(summary.status, "FAIL");
  assert.deepEqual(summary.failedConditions, ["manual-golden-correctness"]);
  assert.equal(summary.conditions.find((condition) => condition.id === "provider-differential").status, "PASS");
});

test("each non-golden blocking condition fails the C Gate independently", () => {
  for (const id of ["provider-differential", "no-mutation", "fixture-reproducibility", "open-reconciliation"]) {
    const summary = evaluateC7Gate(gateInput({ [id]: "FAIL" }));
    assert.equal(summary.status, "FAIL");
    assert.deepEqual(summary.failedConditions, [id]);
  }
});

test("missing or duplicate conditions fail closed", () => {
  const missing = gateInput();
  missing.conditions.pop();
  assert.throws(() => evaluateC7Gate(missing), /conditions/i);

  const duplicate = gateInput();
  duplicate.conditions[6] = { ...duplicate.conditions[0] };
  assert.throws(() => evaluateC7Gate(duplicate), /duplicate/i);
});

test("conditions require a valid status and at least one unique durable evidence ref", () => {
  const badStatus = gateInput();
  badStatus.conditions[0].status = "WAIVED";
  assert.throws(() => evaluateC7Gate(badStatus), /status/i);

  const badRefs = gateInput();
  badRefs.conditions[0].evidenceRefs = ["same", "same"];
  assert.throws(() => evaluateC7Gate(badRefs), /evidence refs/i);
});
