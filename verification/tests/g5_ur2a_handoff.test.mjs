import test from 'node:test';import assert from 'node:assert/strict';
import {captureColdStage,captureFirstSequence}from '../runners/g5/ur2a_cold_capture.mjs';
import {contentFeedback,qualifyCold,qualifyDisplayJoins,CASES}from '../runners/g5/ur2a_handoff_contract.mjs';
test('all available cold buffers persist before action failures are classified',async()=>{
 const saved=[];const capture=s=>captureColdStage(s,{readState:()=>({state:{}}),readSurface:r=>({bytes:new Uint8Array([1]),visible:false,role:r}),snapshot:()=>({bytes:new Uint8Array([2])}),persist:async(k,b)=>{saved.push([k,b[0]]);return {sha256:'actual'};}});
 const r=await captureFirstSequence({capture,action:()=>{throw Error('after action');},undo:()=>{},redo:()=>{}});
 assert.equal(saved.length,12);assert.deepEqual(r.rows.map(r=>r.stage),['before','after','undo','redo']);assert.equal(r.errors[0].stage,'action');
});
test('readback and snapshot errors preserve other surface and state',async()=>{
 const saved=[];const r=await captureColdStage('after',{readState:()=>({generation:'8'}),readSurface:role=>{if(role==='canonical')throw Error('readback');return{bytes:new Uint8Array(4)};},snapshot:()=>{throw Error('snapshot');},persist:(k)=>{saved.push(k);return{sha256:'x'};}});
 assert.deepEqual(saved,['after-preview']);assert.equal(r.generation,'8');assert.equal(r.errors.length,2);
});
test('missing before or Undo and visible overlay cannot pass cold qualification',()=>{
 const row=s=>({stage:s,snapshot:{sha256:'x'},surfaces:{canonical:{sha256:'x'},preview:{sha256:'x',visible:false,nonzeroAlpha:0}},oracle:{status:'QUALIFIED'}});
 const rows=['before','after','undo','redo'].map(row);assert.equal(qualifyCold(rows).status,'QUALIFIED');
 for(const s of ['before','undo'])assert.equal(qualifyCold(rows.filter(r=>r.stage!==s)).status,'FAILED');
 rows[1].surfaces.preview.visible=true;rows[1].surfaces.preview.nonzeroAlpha=1;assert.equal(qualifyCold(rows).status,'FAILED');
});
test('interior mismatch stays failed and clear/HUD/RAF-only operations are not feedback',()=>{
 assert.equal(contentFeedback({inputEvents:[{type:'wheel',started:1,ended:3}],colorOps:[{kind:'clear',started:2,ended:2.1}],rafCallbacks:[{}]}).status,'MISSING');
 assert.equal(contentFeedback({inputEvents:[{type:'wheel',started:1,ended:4}],colorOps:[{kind:'content',started:2,ended:3}]}).status,'OBSERVED');
 assert.equal(qualifyCold([{stage:'after',snapshot:{sha256:'x'},surfaces:{},oracle:{status:'FAILED'}}]).status,'FAILED');
});
const tags={display_record:45,display_roster:46,canonical_receipt:28};
const row=(kind,tag,values)=>({kind,tag,values:values.map(String),at_ns:'1000'});
const basis=()=>[row(6,45,[7,5,11,12,1,30,2,2]),row(5,46,[20,3,3,1,1,1,1,1]),row(5,46,[20,7]),row(11,46,[20,9,2,3,8,1]),row(12,28,[1,8,9,2,3,1,20,3,3,1,1]),row(12,45,[7,1,9,2,3,8,20,11,12,1,30])];
test('successor work is distinct from own visibility and requires frozen serial membership',()=>{
 const r=qualifyDisplayJoins(basis(),tags);assert.equal(r.status,'OBSERVED');assert.equal(r.outcomes[0].own_visible_ns,null);assert.equal(r.outcomes[0].outcome,'ReplacedByCurrentFrame');
 assert.equal(qualifyDisplayJoins(basis().filter(r=>r.kind!==5),tags).status,'INVALID');
});
test('foreign work/commit and absent actual qualification fail closed',()=>{
 for(const [i,n]of [[5,2],[5,9],[4,2],[1,3]]){const rows=basis();rows[i].values[n]='999';assert.equal(qualifyDisplayJoins(rows,tags).status,'INVALID');}
 assert.equal(qualifyDisplayJoins(basis().filter(r=>r.tag!==28),tags).status,'INVALID');assert.equal(CASES.length,10);
});
