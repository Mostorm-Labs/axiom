import test from "node:test";
import assert from "node:assert/strict";
import { validateFacts, REQUIRED } from "../../../tools/generate_g1_08_evidence.mjs";
const source = "a".repeat(40);
const facts = () => ({taskId:"GT-G1-08",packageRef:"notion://3d64c57a-590c-8196-813d-ffd71b4f52f7/GT-G1-08-P31-v0.2",taskAnchor:"a2c3bfa05930b53739886d4d151be31dbcd6be15",sourceRef:source,acceptance:{AC08_L01:"PASS",AC08_L02:"PASS",AC08_L03:"FAIL_MEASURED"},provider:{runId:"1",jobId:"2",sourceSha:source}});
test("accepts bound facts",()=>assert.doesNotThrow(()=>validateFacts(facts(),source)));
test("rejects wrong source",()=>assert.throws(()=>validateFacts(facts(),"b".repeat(40))));
test("declares blocking inventory",()=>assert.deepEqual(REQUIRED.length,5));
