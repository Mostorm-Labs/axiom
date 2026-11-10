import test from 'node:test';import assert from 'node:assert/strict';
import {CONTINUOUS_CASES,SENTINEL,qualifySentinel,qualifyContinuous}from '../runners/g5/transient_display_retry_contract.mjs';
const opaqueBaseline=(count=101)=>{const v=new Uint8Array(count*4);for(let i=0;i<v.length;i+=4)v.set([13,27,51,255],i);return v;};
test('two-level redraw quantization compares against actual fixed baseline RGBA',()=>{
 const before=opaqueBaseline(),after=before.slice();for(let i=0;i<after.length;i+=4)after.set([13,29,53,255],i);
 assert.equal(qualifySentinel(before,after).status,'PRESERVED');
});
test('a single selected pixel with a three-level change in any RGBA channel fails',()=>{
 for(const [channel,value]of [[0,16],[1,30],[2,54],[3,252]]){const before=opaqueBaseline(),after=before.slice();after[channel]=value;const r=qualifySentinel(before,after);assert.equal(r.status,'FAILED');assert.equal(r.lost,1);}
});
test('clear, transparent target and a missing opaque chunk fail without candidate-driven selection',()=>{
 const before=opaqueBaseline();for(const after of [new Uint8Array(before.length),new Uint8Array(before.length).fill(255)])assert.equal(qualifySentinel(before,after).status,'FAILED');
 const after=before.slice();after.fill(0,16,80);assert.equal(qualifySentinel(before,after).lost,16);
});
test('missing, blank, malformed and fewer than 100 baseline pixels cannot qualify',()=>{
 assert.throws(()=>qualifySentinel(null,opaqueBaseline()));assert.throws(()=>qualifySentinel(opaqueBaseline(),undefined));assert.throws(()=>qualifySentinel(opaqueBaseline(),new Uint8Array(4)));
 assert.equal(qualifySentinel(new Uint8Array(),new Uint8Array()).status,'FAILED');assert.equal(qualifySentinel(new Uint8Array(404),new Uint8Array(404)).status,'FAILED');
 assert.equal(qualifySentinel(opaqueBaseline(99),opaqueBaseline(99)).status,'FAILED');assert.equal(qualifySentinel(opaqueBaseline(100),opaqueBaseline(100)).status,'PRESERVED');
});
test('cumulative 0 to 1 to 2 to 3 redraw drift is measured against the same immutable baseline',()=>{
 const before=opaqueBaseline(),original=before.slice();const states=[];
 for(const delta of [0,1,2,3]){const after=before.slice();for(let i=0;i<after.length;i+=4)after[i+1]+=delta;states.push(qualifySentinel(before,after).status);}
 assert.deepEqual(states,['PRESERVED','PRESERVED','PRESERVED','FAILED']);assert.deepEqual(before,original);
});
test('exact six normal continuous cases preserve DPR and fixture matrix',()=>{assert.equal(CONTINUOUS_CASES.length,6);assert.equal(new Set(CONTINUOUS_CASES.map(x=>x.id)).size,6);assert.deepEqual(CONTINUOUS_CASES.map(x=>x.dpr),[1,2,4,1,2,4]);});
test('global cleared target is detected even when no present was reported',()=>{const before=new Uint8Array(800);for(let i=0;i<before.length;i+=4)before.set(SENTINEL.ink,i);const after=new Uint8Array(before.length).fill(255);assert.equal(qualifySentinel(before,after).status,'FAILED');assert.equal(qualifySentinel(before,before).status,'PRESERVED');});
test('partial missing ink and malformed/blank reference cannot qualify',()=>{const before=new Uint8Array(800);for(let i=0;i<before.length;i+=4)before.set(SENTINEL.ink,i);const after=before.slice();after.fill(255,16,20);assert.equal(qualifySentinel(before,after).lost,1);assert.equal(qualifySentinel(after,after).status,'PRESERVED');assert.equal(qualifySentinel(new Uint8Array(800),new Uint8Array(800)).status,'FAILED');assert.throws(()=>qualifySentinel(before,new Uint8Array(4)));});
test('semantic/contact/oracle gaps fail regardless of reported frame success',()=>{const row={initial:{generation:'16'},afterInput:{generation:'24'},final:{state:{activeContacts:0,objects:16,generation:'26',lastCommit:{presentedGeneration:'26',operationId:'a'}}},operations:Array(10).fill({}),schedule:[{same_dom_task_after_up:true}],live:Array(24).fill({qualifier:{status:'PRESERVED'}}),cold:['before','after','undo','redo'].map(stage=>({stage,oracle:{status:'QUALIFIED'}})),roundtrip:{status:'QUALIFIED'},errors:[]};assert.equal(qualifyContinuous(row).status,'OBSERVED');for(const patch of [{operations:[]},{afterInput:{generation:'25'}},{final:{state:{activeContacts:1}}},{live:[]},{roundtrip:{status:'FAILED'}}])assert.equal(qualifyContinuous({...row,...patch}).status,'FAILED');});

test('frozen trajectory rejects foreign fixture, camera, missing proof and swept sentinel overlap',async()=>{
 const {validateTrajectoryPlan,requireTrajectoryCamera}=await import('../runners/g5/transient_display_retry_contract.mjs');
 const h='a'.repeat(64),ids=Array.from({length:16},(_,i)=>i.toString(16).padStart(32,'0'));
 const refs=()=>Object.fromEntries(['input.json','derived.json','request.json','decisions.json'].map(n=>[n,{path:n,sha256:h}]));
 const trace=i=>({index:i,camera:{scale:1,x:0,y:0},view_points:Array.from({length:9},(_,j)=>[465+i*20,180+j*3]),world_points:Array.from({length:9},(_,j)=>[465+i*20,180+j*3]),radius_world:16,expected_target_ids:ids,proof:{1:refs(),2:refs(),4:refs()}});
 const plan={schema:'g5-transient-positive-trajectories-v2',viewport_css:[800,500],radius_world:16,AA_guard_css:16,sentinel:{left:40,top:40,right:300,bottom:460},fixtures:{S0:{snapshot_sha256:h,snapshot_length:2080135,traces:Array.from({length:8},(_,i)=>trace(i)),expected_commits:8,expected_operations_with_undo_redo:10,DPR:[1,2,4],cancel_does_not_append_history:true}}};
 assert.equal(validateTrajectoryPlan(plan,'S0',h,4).traces.length,8);
 assert.throws(()=>validateTrajectoryPlan(plan,'S0','b'.repeat(64),4),/fixture/);
 assert.throws(()=>validateTrajectoryPlan(plan,'S0',h,3),/DPR/);
 const missing=structuredClone(plan);delete missing.fixtures.S0.traces[0].proof[4];assert.throws(()=>validateTrajectoryPlan(missing,'S0',h,4),/proof/);
 const overlap=structuredClone(plan);overlap.fixtures.S0.traces[0].view_points[0][0]=310;assert.throws(()=>validateTrajectoryPlan(overlap,'S0',h,4),/sentinel|mapping/);
 assert.doesNotThrow(()=>requireTrajectoryCamera({scale:1,x:0,y:0},plan.fixtures.S0.traces[0]));
 assert.throws(()=>requireTrajectoryCamera({scale:1,x:1,y:0},plan.fixtures.S0.traces[0]),/camera/);
});
