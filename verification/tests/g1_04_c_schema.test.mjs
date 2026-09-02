import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { validateValue } from "../tools/validate_schemas.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
async function schema(name) {
  const path = resolve(root, `schemas/semantic/g1-04-c-${name}.schema.json`);
  let text;
  try { text = await readFile(path, "utf8"); }
  catch { assert.fail(`required C0 schema does not exist: ${name}`); }
  return JSON.parse(text);
}

const valid = {
  case: {format:"axiom-g1-04-c-case-v1",formatVersion:1,provenance:"AUTHORITY_MANUAL",id:"CASE-1",operationFamily:"CreateObject",authorityRuleRefs:["authority:semantic"],inputRef:"inputs/CASE-1.json",expectedRef:"expected/CASE-1.json",blocking:true},
  expected: {format:"axiom-g1-04-c-expected-v1",formatVersion:1,provenance:"AUTHORITY_MANUAL",caseId:"CASE-1",authorityRuleRefs:["authority:semantic"],mutationExpected:false,disposition:"PLAN_READY",terminalPhase:"PREPARE"},
  observation: {format:"axiom-g1-04-c-observation-v1",formatVersion:1,provenance:"IMPLEMENTATION_OBSERVATION",caseId:"CASE-1",provider:"reference",observedDisposition:"PLAN_READY",observedTerminalPhase:"PREPARE",beforeProjection:{},afterProjection:{}},
  result: {format:"axiom-g1-04-c-result-v1",formatVersion:1,provenance:"CONFORMANCE_RESULT",caseId:"CASE-1",status:"PASS",expectedRef:"expected/CASE-1.json",observationRefs:["observations/CASE-1-reference.json"]},
  gate: {format:"axiom-g1-04-c-gate-v1",formatVersion:1,provenance:"GATE_EVIDENCE",gateId:"GT-G1-04-C",status:"PASS",resultRefs:["results/CASE-1.json"]},
};

test("each C0 trust-domain record has a strict valid schema", async () => {
  for (const name of Object.keys(valid)) { const s = await schema(name); assert.doesNotThrow(() => validateValue(s, valid[name])); }
});

test("wrong formatVersion and provenance are rejected", async () => {
  const s = await schema("case");
  assert.throws(() => validateValue(s, {...valid.case, formatVersion:2}), /const mismatch/);
  assert.throws(() => validateValue(s, {...valid.case, provenance:"IMPLEMENTATION_OBSERVATION"}), /const mismatch/);
});

test("unknown top-level fields are rejected", async () => {
  const s = await schema("gate");
  assert.throws(() => validateValue(s, {...valid.gate, unexpected:true}), /unknown unexpected/);
});

test("observation result and gate cannot masquerade as authority-manual input", async () => {
  const expected = await schema("expected");
  const caseSchema = await schema("case");
  assert.throws(() => validateValue(expected, valid.observation));
  assert.throws(() => validateValue(caseSchema, valid.result));
  assert.throws(() => validateValue(expected, valid.gate));
});
