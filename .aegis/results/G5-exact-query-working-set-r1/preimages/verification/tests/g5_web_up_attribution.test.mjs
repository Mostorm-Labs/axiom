import test from'node:test';import assert from'node:assert/strict';
import{typedWitness,qualifyWitness,reconcilePhases}from'../runners/g5/web_up_attribution_contract.mjs';
import{traceMeasuredPointer}from'../runners/g5/web_up_attribution_contract.mjs';
import fs from'node:fs';import vm from'node:vm';
const selection={primary:'a',generation:'4',transform:[1,0,0,1,900,300],bounds:[899,299,1101,501],width:0,height:0,appearance:{fill:[1,1,.5,1]}};
test('typed sticky uses actual yellow200x200 despite shape-only size0',()=>{
 const w=typedWitness('sticky',selection,{available:true,objectId:'a',width:200,height:200});
 assert.deepEqual(w.color,[255,255,128,255]);assert(w.point[0]>1108&&w.point[0]<1172);
 assert.equal(qualifyWitness(w,new Uint8Array([51,128,204,255]),[80,30]).status,'FAILED');
 assert.equal(qualifyWitness(w,new Uint8Array([255,255,128,255]),[80,30]).status,'QUALIFIED');
 assert.throws(()=>typedWitness('sticky',selection,{available:true,objectId:'foreign',width:200,height:200}));
});
test('Up-only diagnostic gates trace before/after dispatch and keeps Move unobserved',()=>{
 const calls=[];const trace={enable:v=>calls.push(v?'on':'off'),take:()=>{calls.push('drain');return[{records:[]}];}};
 const execute=()=>{calls.push('dispatch');return 7;};
 const move=traceMeasuredPointer('move',execute,trace);assert.deepEqual(calls,['dispatch']);assert.equal(move.value,7);
 calls.length=0;traceMeasuredPointer('up',execute,trace);assert.deepEqual(calls,['on','dispatch','off']);
 calls.push('nextDown');trace.take();assert.deepEqual(calls,['on','dispatch','off','nextDown','drain']);
 calls.length=0;assert.throws(()=>traceMeasuredPointer('up',()=>{throw Error('failure');},trace),/failure/);assert.deepEqual(calls,['on','off']);
});
test('browser contract has no host imports and phase aggregate implementation is guarded',()=>{
 const c=fs.readFileSync(new URL('../runners/g5/web_up_attribution_contract.mjs',import.meta.url),'utf8');assert(!/^import /m.test(c));vm.runInNewContext(c.replaceAll('export ',''),{});
 const source=fs.readFileSync(new URL('../../runtime/foundation/src/g5_observation.cpp',import.meta.url),'utf8');assert(source.includes('#if defined(AXIOM_G5_UR0_DIAGNOSTICS)'));assert(source.includes('tag>=Tag::wa_mask_construct'));
 const driver=fs.readFileSync(new URL('../runners/g5/web_up_attribution.mjs',import.meta.url),'utf8');assert(driver.includes('window.waTrace.enable(false);row.tracePages=(row.tracePages??[]).concat(window.waTrace.take())'));assert(!driver.includes("await import('/g5/web_up_attribution.mjs')"));
});
test('table grid and image resource typed sources, nonfinite/blank/HUD/stale guards',()=>{
 const s={...selection,bounds:[899,299,1141,397],appearance:{fill:[.2,.5,.8,1]}};
 const table=typedWitness('table',s,{available:true,objectId:'a',generation:'4',rows:[{size:48},{size:48}],columns:[{size:120},{size:120}],grid:{color:[.25,.28,.32,1],width:1}});
 assert.equal(table.point[1],378);assert(table.point[0]>1148);assert.equal(table.color[0],64);
 const image=typedWitness('image',{...s,bounds:[1149,749,1231,831],transform:[1,0,0,1,1150,750]},
  {available:true,objectId:'a',content:{width:80,height:80,resourceId:'red'}},'fixedredsha');assert.equal(image.resourceId,'red');
 assert.equal(qualifyWitness(image,new Uint8Array([255,255,255,255]),[80,30]).status,'FAILED');
 assert.equal(qualifyWitness(image,new Uint8Array([255,0,0,255]),[0,0]).status,'FAILED');
 assert.throws(()=>typedWitness('sticky',{...selection,transform:[1,0,0,1,NaN,300]},{available:true,objectId:'a',width:200,height:200}));
 assert.throws(()=>typedWitness('sticky',selection,{available:true,objectId:'a',width:0,height:0}));
});
test('phase reconciliation never double counts inclusive children',()=>{
 const r=reconcilePhases({root:1000,children:[{name:'exact',inclusive:600,exclusive:100},{name:'apply',inclusive:300,exclusive:300}]});
 assert.equal(r.residual,100);assert.equal(r.childInclusive,900);assert.throws(()=>reconcilePhases({root:5,children:[{inclusive:6,exclusive:1}]}));
 for(const exclusive of[NaN,Infinity,601])assert.throws(()=>reconcilePhases({root:1000,children:[{inclusive:600,exclusive}]}));
 const w=typedWitness('sticky',selection,{available:true,objectId:'a',width:200,height:200});assert.equal(qualifyWitness(w,new Uint8Array([252,255,128,255]),[80,30]).status,'FAILED');
});
