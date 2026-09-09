import test from "node:test";
import assert from "node:assert/strict";
import {
  EXECUTION_REF, PACKAGE_MATERIALIZATION_REF, PACKAGE_REF, REQUIRED, SOURCE_PATHS,
  TASK_ANCHOR, validateFacts,
} from "../../../tools/generate_g1_08_evidence.mjs";

const source = "a".repeat(40);
const parent = "b".repeat(40);
const materialized = "c".repeat(40);
const xml = "<?xml version=\"1.0\"?><testsuites tests=\"1\" failures=\"0\"><testsuite tests=\"1\" failures=\"0\"><testcase name=\"ok\"/></testsuite></testsuites>\n";
const suite = {status:"PASS",total:1,passed:1,skipped:0,failed:0,skipIds:[]};
const facts = (overrides = {}) => ({
  format:"axiom-gt-g1-08-facts-v1", taskId:"GT-G1-08", packageRef:PACKAGE_REF,
  packageMaterializationRef:PACKAGE_MATERIALIZATION_REF, repository:"Mostorm-Labs/axiom",
  taskAnchor:TASK_ANCHOR, actualStartingRevision:TASK_ANCHOR, sourceRef:source,
  sourceCommitParent:parent, executionRef:EXECUTION_REF, sourceDelta:{paths:[...SOURCE_PATHS]},
  provider:{runId:"34327609174",attempt:"1",jobId:"102388389125",artifactIdentity:`gt-g1-08-${source}`,sourceSha:source,workflow:".github/workflows/g1-08-exact-source.yml"},
  materialization:{relation:"EVIDENCE_ONLY_DESCENDANT",sourceRef:source,sourceChanges:false,ref:materialized},
  lock:{path:"semantic-sdk.lock.json",blobSha:"d".repeat(40),releaseSetId:"14e3d492c9b7f9705dcb89df8dd3f8abbddb7d1bc026bf3084de45bdc317d5ea"},
  machine:{negativePreflight:"PASS",protobufOff:"PASS",legacyDecoder:"PASS",cleanCheckout:"PASS"},
  correctness:{status:"PASS",verifier:"PASS"}, locality:{status:"PASS",verifier:"PASS",deleteReverseScan:"OBSERVED"},
  ctestOn:{command:"ctest --test-dir out/g1-08-protobuf-on --output-on-failure",result:suite,xml},
  ctestOff:{command:"ctest --test-dir out/g1-08-protobuf-off --output-on-failure",result:suite,xml},
  familyOracles:Array.from({length:15},(_,i)=>({family:String(i),ref:`oracle-${i+1}`,status:"PASS"})), ...overrides,
});
test("accepts complete provider-bound facts",()=>assert.doesNotThrow(()=>validateFacts(facts(),{sourceRef:source})));
test("rejects wrong task, anchor, source, and provider identity",()=>{
  assert.throws(()=>validateFacts(facts({taskId:"GT-G1-09"}),{sourceRef:source}));
  assert.throws(()=>validateFacts(facts({taskAnchor:"e".repeat(40)}),{sourceRef:source}));
  assert.throws(()=>validateFacts(facts({sourceRef:"f".repeat(40)}),{sourceRef:source}));
  assert.throws(()=>validateFacts(facts({provider:{...facts().provider,sourceSha:parent}}),{sourceRef:source}));
});
test("rejects missing provider, machine, ctest, and family rows",()=>{
  for(const override of [{provider:{...facts().provider,runId:""}},{machine:{...facts().machine,cleanCheckout:undefined}},{ctestOn:undefined},{familyOracles:facts().familyOracles.slice(0,14)}]) assert.throws(()=>validateFacts(facts(override),{sourceRef:source}));
});
test("rejects wrong materialization provenance and namespace reuse",()=>{
  assert.throws(()=>validateFacts(facts({materialization:{...facts().materialization,sourceRef:parent}}),{sourceRef:source}));
  assert.throws(()=>validateFacts(facts({materialization:{...facts().materialization,sourceChanges:true}}),{sourceRef:source}));
  assert.throws(()=>validateFacts(facts({materialization:{...facts().materialization,ref:source}}),{sourceRef:source}));
});

test("rejects a materialized ref that is not a full SHA",()=>{
  assert.throws(()=>validateFacts(facts({materialization:{...facts().materialization,ref:"pending"}}),{sourceRef:source}));
});
test("rejects wrong path inventory and lock binding",()=>{
  assert.throws(()=>validateFacts(facts({sourceDelta:{paths:["runtime/semantic/src/replay.cpp"]}}),{sourceRef:source}));
  assert.throws(()=>validateFacts(facts({lock:{...facts().lock,releaseSetId:"wrong"}}),{sourceRef:source}));
});
test("declares exact blocking inventory",()=>{assert.equal(REQUIRED.length,5);assert.equal(new Set(REQUIRED).size,5);});
