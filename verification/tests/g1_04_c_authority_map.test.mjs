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
const validCase={format:"axiom-g1-04-c-case-v1",formatVersion:1,provenance:"AUTHORITY_MANUAL",id:"CASE-1",operationFamily:"CreateObject",authorityRuleRefs:["notion:current-authority#rule"],inputRef:"inputs/CASE-1.json",expectedRef:"expected/CASE-1.json",blocking:true};
const validExpected={format:"axiom-g1-04-c-expected-v1",formatVersion:1,provenance:"AUTHORITY_MANUAL",caseId:"CASE-1",authorityRuleRefs:["notion:current-authority#rule"],mutationExpected:false,disposition:"PLAN_READY",terminalPhase:"PREPARE"};
const observation={format:"axiom-g1-04-c-observation-v1",formatVersion:1,provenance:"IMPLEMENTATION_OBSERVATION",caseId:"CASE-1",provider:"reference",observedDisposition:"PLAN_READY",observedTerminalPhase:"PREPARE",beforeProjection:{},afterProjection:{}};
const result={format:"axiom-g1-04-c-result-v1",formatVersion:1,provenance:"CONFORMANCE_RESULT",caseId:"CASE-1",status:"PASS",expectedRef:"expected/CASE-1.json",observationRefs:["obs.json"]};
const gate={format:"axiom-g1-04-c-gate-v1",formatVersion:1,provenance:"GATE_EVIDENCE",gateId:"GT-G1-04-C",status:"PASS",resultRefs:["result.json"]};

test("manual case and expected require non-empty authority refs", async () => {
  const c=await schema("case"), e=await schema("expected");
  assert.doesNotThrow(()=>validateValue(c,validCase));
  for (const v of [{...validCase,authorityRuleRefs:[]},(()=>{const x={...validCase};delete x.authorityRuleRefs;return x;})(),({...validCase,authorityRuleRefs:[]})]) assert.throws(()=>validateValue(c,v));
  for (const v of [{...validExpected,authorityRuleRefs:[]},(()=>{const x={...validExpected};delete x.authorityRuleRefs;return x;})()]) assert.throws(()=>validateValue(e,v));
});

test("C expected truth is no-mutation and authority-manual only", async () => {
  const e=await schema("expected");
  assert.throws(()=>validateValue(e,{...validExpected,mutationExpected:true}),/const mismatch/);
  assert.throws(()=>validateValue(e,{...validExpected,provenance:"DERIVED_GENERATED"}),/const mismatch/);
  assert.throws(()=>validateValue(e,{...validExpected,provenance:"IMPLEMENTATION_OBSERVATION"}),/const mismatch/);
});

test("non-authority records cannot validate as manual case or expected", async () => {
  const c=await schema("case"), e=await schema("expected");
  for (const v of [observation,result,gate]) {
    assert.throws(()=>validateValue(c,v));
    assert.throws(()=>validateValue(e,v));
  }
});
