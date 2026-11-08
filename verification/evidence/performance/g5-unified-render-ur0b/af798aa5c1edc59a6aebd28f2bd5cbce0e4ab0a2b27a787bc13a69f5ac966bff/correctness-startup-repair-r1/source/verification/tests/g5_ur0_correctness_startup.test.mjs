import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {createStartupFrontier} from '../runners/g5/ur0/correctness_startup.mjs';
const doc='9'.repeat(32);
function model({accept=17,retries=3,origin=1000,pending=1}={}){
 const page={},state={documentId:doc,generation:'0',objects:0,activeContacts:0,lastCommit:{ordinal:'0',operationId:'0'.repeat(32)},target:{runtimeEpoch:'1',attachmentEpoch:'2',viewId:'1',viewEpoch:'1'}};
 let ns=0n,turn=0,queued=pending,recording=false;const errors=[],observed=[],order=[];
 const frontier=createStartupFrontier({page,documentId:doc,timeOrigin:origin,nowNs:()=>String(ns),timeoutMs:60});
 const probe=async()=>{turn++;ns+=1000000n;if(turn===accept){order.push('accepted');frontier.onFrame(page,{presented:'1'});if(recording)observed.push('orphan-startup');}queued=turn<accept+retries?1:0;};
 const context={readState:()=>state,pendingSdk:()=>queued,probe,errors:()=>errors,isCurrent:()=>true};
 return{page,state,errors,frontier,context,order,observed,get turns(){return turn;},setQueued:n=>queued=n,async begin(){const evidence=await frontier.wait(context);order.push('frontier','reset','enable','fixture');recording=true;return evidence;}};
}
test('STARTUP RED witness and corrected delayed boundary: generation0/disabled pending_gpu0 cannot drain a late accepted frame',async()=>{
 const old={generation:'0',pending_gpu:0};assert.equal(old.generation,'0');let enabled=true;const orphan=[];for(let i=0;i<17;i++)if(i===16&&enabled)orphan.push('accepted-without-draw');assert.equal(orphan.length,1);
 const m=model();const evidence=await m.begin();assert.ok(m.turns>10);assert.equal(evidence.pending_sdk_raf,0);assert.equal(evidence.accepted_callback_count,1);assert.equal(evidence.last_presented_count,'1');assert.deepEqual(m.observed,[]);assert.ok(m.order.indexOf('accepted')<m.order.indexOf('reset'));
});
test('EARLY-DELAYED/OFF-ON: early callbacks survive createRuntime return and both fresh pages use the same frontier',async()=>{
 const off=model({accept:1,retries:0}),on=model({accept:25,retries:4,origin:2000});
 off.frontier.onFrame(off.page,{presented:'1'});off.setQueued(0);off.context.probe=async()=>{};
 const a=await off.begin(),b=await on.begin();assert.equal(a.page_time_origin_ms,1000);assert.equal(b.page_time_origin_ms,2000);assert.equal(a.scheme,b.scheme);assert.ok(on.turns>=29);assert.deepEqual(off.order.slice(-4),['frontier','reset','enable','fixture']);assert.deepEqual(on.order.slice(-4),off.order.slice(-4));
});
test('FALSE-FRONTIERS and deadlines: idle queue, fixed10ticks and generation equality alone never qualify',async()=>{
 const m=model({accept:1000,pending:0});m.context.pendingSdk=()=>0;
 await assert.rejects(m.begin(),/deadline/);assert.ok(m.turns>10);assert.ok(!m.order.includes('reset'));
 const retry=model({accept:1,retries:1000});await assert.rejects(retry.begin(),/deadline/);assert.ok(!retry.order.includes('fixture'));
});
test('PENDING-RETRY: positive onFrame still waits for SDK-owned retry queue to drain',async()=>{
 const m=model({accept:1,retries:15});const e=await m.begin();assert.ok(m.turns>=16);assert.ok(BigInt(e.quiescent_at_ns)>BigInt(e.accepted_at_ns));
});
test('ERROR-TIMEOUT-FOREIGN: wrong page/stale callback, SDK errors and foreign/nonempty state fail before collector mutation',async()=>{
 for(const edit of [m=>m.frontier.onFrame({}, {presented:'1'}),m=>m.frontier.onFrame(m.page,{presented:'0'}),m=>m.errors.push('SDK error'),m=>m.state.documentId='a'.repeat(32),m=>m.state.objects=1,m=>m.state.generation='1',m=>m.state.activeContacts=1,m=>m.state.lastCommit.operationId='f'.repeat(32),m=>m.state.target.viewId='0',m=>m.context.isCurrent=()=>false]){
  const m=model();edit(m);await assert.rejects(m.begin());assert.ok(!m.order.includes('reset'));
 }
 const a=model(),b=model({origin:2000});b.frontier.onFrame(a.page,{presented:'1'});await assert.rejects(b.begin(),/page/);
});
test('PRIMARY-UNCHANGED: measured bodies stay byte-identical and correctness startup requires a gated init',()=>{
 const base='verification/evidence/performance/g5-unified-render-ur0b/a3d022abba7d5909aa6dc1e97f5b0f03af0d34154aae585a0a0db21d2f3162c4/resume-r6/source/verification/runners/g5/ur0/';
 const old=fs.readFileSync(base+'consumer.mjs','utf8'),now=fs.readFileSync('verification/runners/g5/ur0/consumer.mjs','utf8');
 const body=s=>s.slice(s.indexOf('async function setup('),s.indexOf('window.ur0='));assert.equal(body(now),body(old));
 const worker=fs.readFileSync('verification/runners/g5/ur0/worker.mjs','utf8'),oldWorker=fs.readFileSync(base+'worker.mjs','utf8');
 const marker="else if(mode==='realtime')";assert.ok(worker.indexOf(marker)>0&&oldWorker.indexOf(marker)>0);assert.equal(worker.slice(worker.indexOf(marker)),oldWorker.slice(oldWorker.indexOf(marker)));
 assert.match(now,/correctness startup frontier missing/);assert.match(worker,/correctnessOnly:true/);
});
