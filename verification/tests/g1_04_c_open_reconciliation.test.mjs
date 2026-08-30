import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { validateValue } from "../tools/validate_schemas.mjs";
import { assertExpectedPolicyStatus, CLOSED_CURRENT_POLICIES } from "../tools/g1_04_c_contract.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const expectedSchema = JSON.parse(await readFile(resolve(root, "schemas/semantic/g1-04-c-expected.schema.json"), "utf8"));
const baseExpected = {
  format: "axiom-g1-04-c-expected-v1",
  formatVersion: 1,
  provenance: "AUTHORITY_MANUAL",
  caseId: "CASE-POLICY",
  authorityRuleRefs: ["notion:current-authority#closed-policy"],
  mutationExpected: false,
  disposition: "PLAN_READY",
  terminalPhase: "PREPARE"
};

test("BLOCKED_OPEN is not a valid expected disposition", () => {
  assert.throws(() => validateValue(expectedSchema, {...baseExpected, disposition:"BLOCKED_OPEN"}), /enum mismatch/);
});

test("closed Connector and geometry policies reject openPolicy true", async () => {
  for (const key of ["connector-target-delete", "geometry-point-like-elements-per-operation-aggregate"]) {
    assert.throws(() => assertExpectedPolicyStatus(key, {...baseExpected, openPolicy:true}), /closed policy cannot be OPEN/);
  }
});

test("closed policies allow absent or false openPolicy", async () => {
  for (const key of ["connector-target-delete", "geometry-point-like-elements-per-operation-aggregate"]) {
    assert.doesNotThrow(() => assertExpectedPolicyStatus(key, baseExpected));
    assert.doesNotThrow(() => assertExpectedPolicyStatus(key, {...baseExpected, openPolicy:false}));
  }
});

test("unknown policies are not silently promoted to CLOSED", async () => {
  assert.equal(CLOSED_CURRENT_POLICIES.has("future-unknown-policy"), false);
  assert.doesNotThrow(() => assertExpectedPolicyStatus("future-unknown-policy", {...baseExpected, openPolicy:true}));
});
