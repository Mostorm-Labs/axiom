import test from 'node:test';import assert from 'node:assert/strict';
import {CONTINUOUS_CASES,SENTINEL,qualifySentinel,qualifyContinuous}from '../runners/g5/transient_display_retry_contract.mjs';
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
