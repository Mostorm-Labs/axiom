import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { validateValue } from "../tools/validate_schemas.mjs";
import { assertExpectedPolicyStatus } from "../tools/g1_04_c_contract.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const P20 = "3cc4c57a-590c-81ae-ab73-d75501c47169";
const TERMINAL_PHASE_AUTHORITY = "3cd4c57a-590c-8165-973f-ee31d93f1116";
const OPERATIONS = ["InsertObjects", "DeleteObjects", "RestoreObjects", "SetPlacements", "SetTransforms", "PatchProperties", "SetObjectSize", "SetVectorPathGeometry", "SetImageContent", "AddStroke", "SplitStrokes", "AddEraseMasks", "RemoveEraseMasks", "EditRichText", "SetConnectorContent"];

async function json(relativePath) {
  return JSON.parse(await readFile(resolve(root, relativePath), "utf8"));
}
async function schema(name) { return json(`schemas/semantic/g1-04-c-${name}.schema.json`); }
function hasUuid(ref) { return /[0-9a-f]{8}-?[0-9a-f]{4}-?[0-9a-f]{4}-?[0-9a-f]{4}-?[0-9a-f]{12}/i.test(ref); }
function hasPage(ref, page) { return ref.includes(page) || ref.includes(page.replaceAll("-", "")); }
function p20Requirements(record) {
  return record.authorityRuleRefs
    .filter((ref) => hasPage(ref, P20))
    .flatMap((ref) => ref.match(/#C-V0[1-6]\b/g) ?? []);
}
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
  for (const record of cases) assert(p20Requirements(record).length > 0, `${record.id} missing applicable P20 requirement ref`);
  for (const record of expected) { validateValue(expectedSchema, record); assertRefs(record); assert.equal(record.mutationExpected, false); assert(p20Requirements(record).length > 0, `${record.caseId} missing applicable P20 requirement ref`); assert(p20Requirements(record).includes("#C-V01"), `${record.caseId} missing P20:C-V01 authority-derived expected truth binding`); }
  for (const record of expected) assert(record.authorityRuleRefs.some((ref) => hasPage(ref, TERMINAL_PHASE_AUTHORITY)), `${record.caseId} missing terminal-phase Authority ref`);
  const ids = cases.map((r) => r.id); const expectedIds = expected.map((r) => r.caseId);
  assert.equal(new Set(ids).size, ids.length); assert.equal(new Set(expectedIds).size, expectedIds.length);
  assert.deepEqual([...expectedIds].sort(), [...ids].sort());
  for (const record of cases) assert.equal(expected.filter((e) => e.caseId === record.id).length, 1);
  const operationIdCollision = expected.find((record) => record.caseId === "C1-ID-COLLISION");
  assert.deepEqual(
    {disposition: operationIdCollision?.disposition, terminalPhase: operationIdCollision?.terminalPhase},
    {disposition: "REJECTED", terminalPhase: "IDEMPOTENCY"},
    "different-payload OperationId collision must reject during idempotency before stateful validation",
  );
  const caseById = new Map(cases.map((record) => [record.id, record]));
  const expectedById = new Map(expected.map((record) => [record.caseId, record]));
  assert(p20Requirements(caseById.get("C1-IDEMPOTENT-EQUIVALENT")).includes("#C-V04"));
  assert(p20Requirements(expectedById.get("C1-IDEMPOTENT-EQUIVALENT")).includes("#C-V04"));
  for (const id of ["C1-ID-COLLISION"]) {
    assert(p20Requirements(caseById.get(id)).includes("#C-V04"));
    assert(p20Requirements(caseById.get(id)).includes("#C-V03"));
    assert(p20Requirements(expectedById.get(id)).includes("#C-V04"));
    assert(p20Requirements(expectedById.get(id)).includes("#C-V03"));
  }
  for (const id of ["C1-INSERT-STAGED-PARENT", "C1-INSERT-STAGED-CONNECTOR", "C1-RESTORE-STAGED-PARENT-CHILD", "C1-RESTORE-STAGED-CONNECTOR"]) {
    assert(p20Requirements(caseById.get(id)).includes("#C-V05"), `${id} must bind staged-state P20 requirement`);
    assert(p20Requirements(expectedById.get(id)).includes("#C-V05"), `${id} expected must bind staged-state P20 requirement`);
  }
  assert(p20Requirements(caseById.get("C1-RESTORE-ELIGIBLE")).includes("#C-V02"));
  assert(!p20Requirements(caseById.get("C1-RESTORE-ELIGIBLE")).includes("#C-V04"));
  for (const id of ["C1-GEOMETRY-LIMIT", "C1-GEOMETRY-OVERFLOW"]) {
    const refs = p20Requirements(caseById.get(id));
    assert(refs.includes("#C-V03"), `${id} must bind rejection requirement C-V03`);
    assert(!(refs.length === 1 && refs.includes("#C-V05")), `${id} must not be governed solely by staged-state C-V05`);
  }
  for (const id of ["C1-GEOMETRY-BOUNDARY", "C1-GEOMETRY-N-1", "C1-GEOMETRY-N"]) {
    assert(p20Requirements(caseById.get(id)).includes("#C-V02"), `${id} is a positive conformance case`);
    assert(!p20Requirements(caseById.get(id)).includes("#C-V05"), `${id} is not a staged-state case`);
  }
  for (const id of ["C1-DELETE-CASCADE", "C1-DELETE-VALID", "C1-DELETE-SUBTREE", "C1-SPLIT-PLAN"]) {
    assert(p20Requirements(caseById.get(id)).includes("#C-V02"), `${id} must bind positive conformance`);
    assert(p20Requirements(caseById.get(id)).includes("#C-V06"), `${id} must bind PreparedApplyPlan closure`);
  }
  const guards = {
    "C1-INSERT-STAGED-PARENT": ["PLAN_READY", "PREPARE"],
    "C1-INSERT-STAGED-CONNECTOR": ["PLAN_READY", "PREPARE"],
    "C1-RESTORE-STAGED-PARENT-CHILD": ["PLAN_READY", "PREPARE"],
    "C1-RESTORE-STAGED-CONNECTOR": ["PLAN_READY", "PREPARE"],
    "C1-RESTORE-NO-TOMBSTONE": ["PLAN_READY", "PREPARE"],
    "C1-TRANSFORM-NEGATIVE-ZERO": ["PLAN_READY", "PREPARE"],
    "C1-GEOMETRY-N-1": ["PLAN_READY", "PREPARE"],
    "C1-GEOMETRY-N": ["PLAN_READY", "PREPARE"],
  };
  for (const [id, [disposition, terminalPhase]] of Object.entries(guards)) {
    const outcome = expected.find((record) => record.caseId === id);
    assert.deepEqual({disposition: outcome?.disposition, terminalPhase: outcome?.terminalPhase}, {disposition, terminalPhase}, id);
  }
  assert.equal(expected.find((record) => record.caseId === "C1-GEOMETRY-LIMIT")?.semanticErrorCategory, "GEOMETRY_LIMIT_EXCEEDED");
  assert.equal(expected.find((record) => record.caseId === "C1-GEOMETRY-LIMIT")?.terminalPhase, "STATELESS_VALIDATE");
  assert.equal(expected.find((record) => record.caseId === "C1-GEOMETRY-OVERFLOW")?.semanticErrorCategory, "INTEGER_OVERFLOW");
  assert.equal(expected.find((record) => record.caseId === "C1-GEOMETRY-OVERFLOW")?.terminalPhase, "STATELESS_VALIDATE");
  for (const op of OPERATIONS) assert(cases.some((r) => r.operationFamily === op && r.blocking === true && expected.find((e) => e.caseId === r.id && e.disposition === "PLAN_READY" && e.terminalPhase === "PREPARE")), `missing positive ${op}`);
  for (const key of ["connector-target-delete", "geometry-point-like-elements-per-operation-aggregate"]) for (const record of expected) assert.doesNotThrow(() => assertExpectedPolicyStatus(key, record));
  assert.equal(suite.format, "axiom-g1-04-c-core-suite-v1"); assert.equal(suite.formatVersion, 1); assert.equal(suite.suiteId, "GT-G1-04-C-CORE");
  assert.deepEqual(suite.caseIds, [...new Set(suite.caseIds)].sort());
  assert.deepEqual([...suite.caseIds].sort(), ids.filter((id) => cases.find((r) => r.id === id).blocking).sort());
});
