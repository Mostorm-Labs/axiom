import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { validateValue } from "../tools/validate_schemas.mjs";
import { assertExpectedPolicyStatus } from "../tools/g1_04_c_contract.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const P20 = "3cc4c57a-590c-81ae-ab73-d75501c47169";
const OPERATIONS = ["InsertObjects", "DeleteObjects", "RestoreObjects", "SetPlacements", "SetTransforms", "PatchProperties", "SetObjectSize", "SetVectorPathGeometry", "SetImageContent", "AddStroke", "SplitStrokes", "AddEraseMasks", "RemoveEraseMasks", "EditRichText", "SetConnectorContent"];

async function json(relativePath) {
  return JSON.parse(await readFile(resolve(root, relativePath), "utf8"));
}
async function schema(name) { return json(`schemas/semantic/g1-04-c-${name}.schema.json`); }
function hasUuid(ref) { return /[0-9a-f]{8}-?[0-9a-f]{4}-?[0-9a-f]{4}-?[0-9a-f]{4}-?[0-9a-f]{12}/i.test(ref); }
function hasPage(ref, page) { return ref.includes(page) || ref.includes(page.replaceAll("-", "")); }
function assertRefs(record) {
  assert(record.authorityRuleRefs.every((ref) => hasUuid(ref)));
  assert(record.authorityRuleRefs.some((ref) => hasPage(ref, P20)), `${record.caseId ?? record.id} missing direct P20 ref`);
  assert(record.authorityRuleRefs.some((ref) => hasUuid(ref) && !hasPage(ref, P20)), `${record.caseId ?? record.id} missing semantic authority ref`);
  assert(record.authorityRuleRefs.every((ref) => !/(runtime\/semantic|IMPLEMENTATION_OBSERVATION|generated\/|evidence|source_ref|materialized_ref)/i.test(ref)));
}

test("C1 authoring root is complete and authority-bound", async () => {
  const cases = await json("corpus/semantic/v1/g1-04-c/authoring/cases.json");
  const expected = await json("corpus/semantic/v1/g1-04-c/authoring/expected.json");
  const suite = await json("corpus/semantic/v1/g1-04-c/suites/core.json");
  assert(Array.isArray(cases) && cases.length > 0);
  assert(Array.isArray(expected) && expected.length > 0);
  const caseSchema = await schema("case");
  const expectedSchema = await schema("expected");
  for (const record of cases) { validateValue(caseSchema, record); assertRefs(record); assert.equal(record.inputRef, `generated/inputs/${record.id}.json`); assert.equal(record.expectedRef, `authoring/expected.json#${record.id}`); }
  for (const record of expected) { validateValue(expectedSchema, record); assertRefs(record); assert.equal(record.mutationExpected, false); }
  const ids = cases.map((r) => r.id); const expectedIds = expected.map((r) => r.caseId);
  assert.equal(new Set(ids).size, ids.length); assert.equal(new Set(expectedIds).size, expectedIds.length);
  assert.deepEqual([...expectedIds].sort(), [...ids].sort());
  for (const record of cases) assert.equal(expected.filter((e) => e.caseId === record.id).length, 1);
  for (const op of OPERATIONS) assert(cases.some((r) => r.operationFamily === op && r.blocking === true && expected.find((e) => e.caseId === r.id && e.disposition === "PLAN_READY" && e.terminalPhase === "PREPARE")), `missing positive ${op}`);
  for (const key of ["connector-target-delete", "geometry-point-like-elements-per-operation-aggregate"]) for (const record of expected) assert.doesNotThrow(() => assertExpectedPolicyStatus(key, record));
  assert.equal(suite.format, "axiom-g1-04-c-core-suite-v1"); assert.equal(suite.formatVersion, 1); assert.equal(suite.suiteId, "GT-G1-04-C-CORE");
  assert.deepEqual(suite.caseIds, [...new Set(suite.caseIds)].sort());
  assert.deepEqual([...suite.caseIds].sort(), ids.filter((id) => cases.find((r) => r.id === id).blocking).sort());
});
