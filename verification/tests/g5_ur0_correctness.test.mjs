import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import {copySnapshot} from '../runners/g5/ur0/stream_transfer.mjs';
import {comparePixels,publicHudBand} from '../runners/g5/ur0/pixel_oracle.mjs';
import {correctnessDriver} from '../runners/g5/ur0/correctness_driver.mjs';
let contract={},transport={};
try{contract=await import('../runners/g5/ur0/correctness_contract.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
try{transport=await import('../runners/g5/ur0/correctness_transfer.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
const archive=path.resolve(import.meta.dirname,'../evidence/performance/g5-unified-render-ur0b/d33166e423e7558bf4d18b87108425b2cb2553ab6ff15271fd4752a0c200d73a/resume-r4/execution-and-checks/execution/raw-runs/correctness-lower_z_translucent_drag');
const read=n=>JSON.parse(fs.readFileSync(path.join(archive,n)));
const frame=()=>({state:{documentId:'a'.repeat(32),objects:2,selected:['1'.repeat(32)],primary:'1'.repeat(32),generation:'2',activeContacts:0,camera:{scale:1,x:0,y:0},target:{attachmentEpoch:'2',viewId:'1',runtimeEpoch:'1'},lastCommit:{ordinal:'2',operationId:'2'.repeat(32),presentedGeneration:'2'}},selection:{count:1,primary:'1'.repeat(32),roots:['1'.repeat(32)],kind:5,generation:'2',bounds:[76,70,724,430],transform:[1,0,0,1,0,0]},metrics:{width:1600,height:1000,logical_width:800,logical_height:500,dpr:2}});
function subject(){
 const before=frame(),live=structuredClone(before),after=structuredClone(before);live.state.activeContacts=1;
 after.state.generation='3';after.state.lastCommit={ordinal:'3',operationId:'3'.repeat(32),presentedGeneration:'3'};
 after.selection.generation='3';after.selection.transform=[1,0,0,1,24,10];after.selection.bounds=[100,80,748,440];
 const nativeBefore={status:'DECODED',document_revision:'2',object_count:2,objects:[{id:'1'.repeat(32),kind:5,transform:[1,0,0,1,0,0]},{id:'4'.repeat(32),kind:1,transform:[1,0,0,1,0,0]}]};
 const nativeAfter=structuredClone(nativeBefore);nativeAfter.document_revision='3';nativeAfter.objects[0].transform=[1,0,0,1,24,10];
 return{before,live,after,nativeBefore,nativeAfter};
}
test('REAL-OLD-HUD-RED: actual OFF1 remains FAILED with 16112 differences until retained public selection is handled',()=>{
 const row=read('off-1.correctness.json'),actual=fs.readFileSync(path.join(archive,'off-1.candidate.rgba')),expected=fs.readFileSync(path.join(archive,'off-1.reference.rgba')),edge=fs.readFileSync(path.join(archive,'off-1.reference.edge'));
 const old=comparePixels(actual,expected,edge,publicHudBand(row.request));assert.equal(old.status,'FAILED');assert.equal(old.interior_mismatched_pixels,16112);
 assert.equal(typeof contract.selectionHudRequest,'function','retained selection handling is missing');
 const after=read('off-1-after.json'),hud=contract.selectionHudRequest({selection:after.selection,camera:after.state.camera,caseId:row.case_id,step:1});
 const repaired=comparePixels(actual,expected,edge,publicHudBand({...row.request,...hud}));assert.equal(repaired.status,'QUALIFIED');assert.equal(repaired.interior_mismatched_pixels,0);
 assert.equal(read('off-1.correctness.json').comparison.status,'FAILED');
});
test('RETAINED-SELECTION: hand mode does not clear the selection; malformed bounds fail closed',()=>{
 assert.equal(typeof contract.selectionHudRequest,'function');
 const s=frame();const hud=contract.selectionHudRequest({selection:s.selection,camera:{scale:2,x:10,y:-5},caseId:'lower_z_translucent_drag',step:1});
 assert.deepEqual(hud.hud_bounds,[[162,135,1458,855]]);assert.equal(hud.hud_guide_y,undefined);
 assert.deepEqual(contract.selectionHudRequest({selection:{count:0},camera:s.state.camera,caseId:'lower_z_translucent_drag',step:1}).hud_bounds,[]);
 for(const bounds of [null,[0,0,NaN,20],[4,2,1,3]])assert.throws(()=>contract.selectionHudRequest({selection:{count:1,bounds},camera:s.state.camera}));
});
test('LIVE-SUBJECT: expected transform is independently fixed by before bounds and rectangle center',()=>{
 assert.equal(typeof contract.fixedLiveExpectation,'function');const b=read('off-0-before.json');
 assert.deepEqual(contract.fixedLiveExpectation(b).transform,[1,0,0,1,24,10.50576400756836]);
 const s=subject();assert.equal(contract.validateLiveSubject(s).status,'QUALIFIED');
 const changes=[s=>s.after.selection.transform[5]=12,s=>s.nativeAfter.objects[0].transform[4]=0,s=>s.nativeAfter.objects.reverse(),s=>s.live.state.documentId='f'.repeat(32),s=>s.after.selection.primary='f'.repeat(32),s=>s.live.state.camera.scale=2,s=>s.after.metrics.dpr=4,s=>s.live.state.generation='3',s=>s.after.state.generation='4',s=>s.after.state.activeContacts=1,s=>s.after.state.lastCommit.presentedGeneration='2',s=>s.after.state.lastCommit.ordinal='2',s=>s.before.selection.transform[4]=1];
 for(const change of changes){const x=subject();change(x);assert.throws(()=>contract.validateLiveSubject(x));}
});
test('LIVE-SUBJECT: opaque snapshot capture state must agree with the driver public subject',()=>{
 assert.equal(typeof contract.validateLiveSubject,'function');const s=subject();s.capturedBefore=structuredClone(s.before);s.capturedAfter=structuredClone(s.after);
 assert.equal(contract.validateLiveSubject(s).status,'QUALIFIED');s.capturedAfter.state.documentId='f'.repeat(32);assert.throws(()=>contract.validateLiveSubject(s));
});
test('LIVE-SUBJECT: a transform-only commit cannot also change the stroke body or its masks',()=>{
 for(const field of ['mask_count','knots','config']){const s=subject();s.nativeAfter.objects[0][field]=field==='config'?{algorithmVersion:99}:999;assert.throws(()=>contract.validateLiveSubject(s));}
});
test('HUD-SUPPORT: fixed live translation uses snapped expectation, guide exists only in lower-z live step',()=>{
 assert.equal(typeof contract.selectionHudRequest,'function');const before=frame();
 const h=contract.selectionHudRequest({selection:before.selection,camera:before.state.camera,caseId:'lower_z_translucent_drag',step:0,before});
 assert.deepEqual(h.hud_bounds,[[76,70,724,430],[100,80,748,440]]);assert.equal(h.hud_guide_y,260);
 for(const dpr of [1,2,4]){
  const band=publicHudBand({width:800*dpr,height:500*dpr,logical_width:800,logical_height:500,...h});const at=(x,y)=>band[(y*dpr)*800*dpr+x*dpr];
  assert.equal(at(400,32),1);assert.equal(at(400,31),0);assert.equal(at(200,200),0);assert.equal(at(20,260),1);assert.equal(at(20,263),0);
 }
 assert.throws(()=>publicHudBand({width:100,height:100,logical_width:100,logical_height:100,hud_bounds:[],hud_guide_y:260,case_id:'other',step:0}));
});
test('BODY-MUTANTS: body/order/opacity/wrong-transform/mask leakage outside HUD and AA still fail',()=>{
 const request={width:200,height:200,logical_width:200,logical_height:200,hud_bounds:[[20,45,80,90]]},hud=publicHudBand(request),edge=new Uint8Array(40000),expected=new Uint8Array(160000).fill(128);
 for(const [name,pixel,rgba] of [['body',12060,[255,0,0,255]],['order',12070,[0,0,0,255]],['opacity',14050,[128,128,128,80]],['transform',12040,[255,255,255,255]],['mask-leak',12050,[0,0,0,0]]]){
  assert.equal(hud[pixel],0);const a=expected.slice();a.set(rgba,pixel*4);assert.equal(comparePixels(a,expected,edge,hud).status,'FAILED',name);assert.deepEqual(publicHudBand(request),hud);
 }
 const all=new Uint8Array(40000).fill(1);assert.equal(comparePixels(expected,expected,edge,all).status,'MISSING');assert.equal(comparePixels(expected,expected,new Uint8Array(40000).fill(1)).status,'MISSING');
});
test('STAGE-TERMINALS: pending await retains exact last-started stage with paired monotonic completed spans',async()=>{
 assert.equal(typeof transport.createStageRecorder,'function');let tick=0;const events=[],stages=transport.createStageRecorder({emit:e=>events.push(e),clock:()=>String(++tick),domain:'test'});
 await stages.run('snapshot acquire/transfer',async()=>42);let release;const pending=stages.run('pixel transfer',()=>new Promise(r=>release=r));
 assert.equal(stages.lastStarted,'pixel transfer');assert.deepEqual(events.map(e=>[e.stage,e.event]),[['snapshot acquire/transfer','begin'],['snapshot acquire/transfer','end'],['pixel transfer','begin']]);
 assert.equal(await Promise.race([pending.then(()=>'finished'),Promise.resolve('fake-timeout')]),'fake-timeout');assert.equal(stages.lastStarted,'pixel transfer');
 assert.ok(events.every((e,i)=>!i||BigInt(events[i-1].monotonic_ns)<BigInt(e.monotonic_ns)));
 release();await pending;assert.equal(events.at(-1).event,'end');
 await assert.rejects(stages.run('close',async()=>{throw Error('close failed');}));assert.equal(events.at(-1).status,'failed');
});
test('DRIVER-INTEGRATION: fixed pointer recipe produces snapped live HUD and retained hand selection, without a browser',async()=>{
 const base=frame(),calls=[];let running=false;
 const gl={drawingBufferWidth:1600,drawingBufferHeight:1000,RGBA:6408,UNSIGNED_BYTE:5121,NO_ERROR:0,readPixels:(x,y,w,h,f,t,bytes)=>bytes.fill(255),getError:()=>0};
 const sdk={canvas:{getContext:()=>gl,getBoundingClientRect:()=>({width:800,height:500})},readState:()=>structuredClone(base.state),readSelection:()=>structuredClone(base.selection),readBrushConfig:()=>({family:'round-handdrawn',algorithmVersion:4}),selectBrush:async()=>{},setInk:async()=>{},setAppearance:async()=>{},selectShape:async()=>{},selectTool:async name=>{base.state.tool=name==='hand'?3:1;}};
 const pointer=(phase,x,y)=>{calls.push([phase,x,y]);if(running){base.state.activeContacts=phase==='up'?0:1;if(phase==='up'){base.state.generation='3';base.state.lastCommit={ordinal:'3',operationId:'3'.repeat(32),presentedGeneration:'3'};base.selection.generation='3';base.selection.bounds=[100,80,748,440];base.selection.transform=[1,0,0,1,24,10];}}};
 const Driver=process.env.UR0_DRIVER_RED?(await import(pathToFileURL(path.resolve(process.env.UR0_DRIVER_RED)))).correctnessDriver:correctnessDriver;
 const driver=Driver({sdk,pointer,raf:async()=>{},settled:async()=>{},diag:()=>null});await driver.setup('lower_z_translucent_drag',false);
 assert.equal(calls.length,262);assert.deepEqual(calls[0],['down',100,100]);assert.equal(calls[256][0],'up');assert.deepEqual(calls.slice(257,260),[['down',280,150],['move',540,370],['up',540,370]]);
 running=true;const live=await driver.next();assert.deepEqual(live.request.hud_bounds,[[76,70,724,430],[100,80,748,440]]);assert.equal(live.request.hud_guide_y,260);
 assert.equal(live.subject.live.state.generation,'2');assert.equal(live.subject.after.state.generation,'3');assert.match(live.source_sha256,/^[0-9a-f]{64}$/);
 await copySnapshot({length:live.byte_length,pull:(o,n)=>driver.pull(o,n),write:()=>{}});driver.release(false);
 const hand=await driver.next();assert.deepEqual(hand.request.hud_bounds,[[100,80,748,440]]);assert.equal(hand.request.hud_guide_y,undefined);
 await copySnapshot({length:hand.byte_length,pull:(o,n)=>driver.pull(o,n),write:()=>{}});driver.release(false);
});
