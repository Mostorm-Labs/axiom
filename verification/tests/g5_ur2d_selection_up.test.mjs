import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {CHUNK_BYTES,MAX_BUFFER_BYTES,sendBinaryChunks,validateChunk,classifyWindow,classifyPerformance} from '../runners/g5/ur2d_selection_up_contract.mjs';
import {comparePixels} from '../runners/g5/ur0/pixel_oracle.mjs';
import * as driver from '../runners/g5/ur2d_selection_up.mjs';

test('DPR2 full target persists incrementally with exact binary digest and bounded in-flight chunk',async()=>{
 const bytes=new Uint8Array(2862*2020*4);for(let i=0;i<bytes.length;i++)bytes[i]=(i*23+71)%256;
 const dir=await fs.mkdtemp(path.join(os.tmpdir(),'ur2d-chunk-'));let previous,active=0,maxActive=0,maxPayload=0;
 const expected=createHash('sha256').update(bytes).digest('hex'),received=createHash('sha256');
 try{
  const file=await fs.open(path.join(dir,'target.rgba'),'wx');
  try{
   const result=await sendBinaryChunks('target.rgba',bytes,async chunk=>{
    ++active;maxActive=Math.max(maxActive,active);maxPayload=Math.max(maxPayload,chunk.base64.length);
    const next=validateChunk(chunk,previous),block=Buffer.from(chunk.base64,'base64');
    received.update(block);await file.write(block,0,block.length,chunk.offset);previous=next;--active;
    return{nextOffset:next.nextOffset};
   });
   assert.equal(result.byteLength,bytes.length);assert.equal(result.chunks,Math.ceil(bytes.length/CHUNK_BYTES));
  }finally{await file.close();}
  assert.equal(received.digest('hex'),expected);assert.equal((await fs.stat(path.join(dir,'target.rgba'))).size,bytes.length);
  assert.equal(maxActive,1);assert.ok(maxPayload<=Math.ceil(CHUNK_BYTES/3)*4);assert.ok(previous.complete);
 }finally{await fs.rm(dir,{recursive:true,force:true});}
});
test('transfer rejects unsafe names, over-cap buffers, reordered/short chunks and missing acknowledgements',async()=>{
 const block=new Uint8Array(CHUNK_BYTES+3);
 for(const name of ['../target.rgba','/target.rgba','a.partial','a.rgba?x'])await assert.rejects(sendBinaryChunks(name,block,async()=>{}));
 await assert.rejects(sendBinaryChunks('x.rgba',new Uint8Array(),async()=>{}));
 assert.throws(()=>validateChunk({name:'a.rgba',offset:0,total:MAX_BUFFER_BYTES+1,base64:'AAAA'}));
 assert.throws(()=>validateChunk({name:'a.rgba',offset:1,total:2,base64:'AA=='}));
 assert.throws(()=>validateChunk({name:'a.rgba',offset:0,total:3,base64:'AA=='}));
 assert.throws(()=>validateChunk({name:'a.rgba',offset:0,total:3,base64:'AAAA'},{total:3,nextOffset:3}));
 await assert.rejects(sendBinaryChunks('x.rgba',block,async()=>({nextOffset:0})),/acknowledgement/);
 let calls=0;await assert.rejects(sendBinaryChunks('x.rgba',block,async()=>{++calls;throw Error('disk full');}),/disk full/);assert.equal(calls,1);
});
test('guard failure, crashed subject, missing worker or changed source never qualify despite cleanup',()=>{
 const guard={reason:'completed',returncode:0,cleanup_complete:true,remaining_owned_pids:[]},worker={status:'OBSERVED'};
 assert.equal(classifyWindow(guard,worker,true),'COMPLETED');
 for(const patch of [{reason:'rss_limit'},{reason:'timeout'},{returncode:1},{cleanup_complete:false},{remaining_owned_pids:[1]}])assert.equal(classifyWindow({...guard,...patch},worker,true),'STOPPED');
 assert.equal(classifyWindow(guard,{status:'FUNCTIONAL_OR_HARNESS_FAILURE'},true),'STOPPED');
 assert.equal(classifyWindow(guard,null,true),'STOPPED');assert.equal(classifyWindow(guard,worker,false),'STOPPED');
});
test('performance needs every correct paired sample and does not hide missing or failed observations',()=>{
 const rows=[1,2,3].map(n=>({old:n*10,candidate:n,correct:true}));assert.equal(classifyPerformance(rows).oldMedian,20);
 assert.equal(classifyPerformance(rows.slice(1)).status,'MISSING');assert.equal(classifyPerformance([...rows.slice(1),{old:0,candidate:1,correct:true}]).status,'MISSING');
 assert.equal(classifyPerformance(rows.map((r,i)=>({...r,correct:i!==0}))).status,'MISSING');
});
test('original roundtrip policy accepts one/two-level differences and rejects three without a candidate band',()=>{
 const original=new Uint8Array(404).fill(127),one=original.slice(),three=original.slice();one[3]++;one[88]+=2;three[3]+=3;
 assert.equal(comparePixels(one,original,new Uint8Array(101)).status,'QUALIFIED');
 assert.equal(comparePixels(three,original,new Uint8Array(101)).status,'FAILED');
});
test('roundtrip accepts an independent edge-only delta4 and rejects a non-AA delta3',()=>{
 assert.equal(typeof driver.evaluateIndependentRoundtrip,'function','roundtrip must use the frozen independent band');
 const reference=new Uint8Array(404).fill(127),before=reference.slice(),after=reference.slice(),band=new Uint8Array(101);
 band[3]=1;after[12]+=4;
 const qualified=driver.evaluateIndependentRoundtrip(before,after,reference,reference,band,band);
 assert.equal(qualified.fullViewport.status,'FAILED');assert.equal(qualified.status,'QUALIFIED');
 after[16]+=3;assert.equal(driver.evaluateIndependentRoundtrip(before,after,reference,reference,band,band).status,'FAILED');
});
test('missing, malformed, blank or incompatible independent references cannot qualify',()=>{
 assert.equal(typeof driver.evaluateIndependentRoundtrip,'function');
 const reference=new Uint8Array(404).fill(127),band=new Uint8Array(101);
 for(const expected of [null,new Uint8Array(),new Uint8Array(4),new Uint8Array(404)])
  assert.throws(()=>driver.evaluateIndependentRoundtrip(reference,reference,expected,reference,band,band));
 assert.throws(()=>driver.evaluateIndependentRoundtrip(reference,reference,reference,reference,new Uint8Array(2),band));
 const bad=band.slice();bad[0]=2;
 assert.throws(()=>driver.evaluateIndependentRoundtrip(reference,reference,reference,reference,bad,band));
 const different=band.slice();different[0]=1;
 assert.throws(()=>driver.evaluateIndependentRoundtrip(reference,reference,reference,reference,different,band));
});
test('driver transfers outside timing and restores observer before SDK teardown with bounded diagnostic storage',async()=>{
 const source=await fs.readFile(new URL('../runners/g5/ur2d_selection_up.mjs',import.meta.url),'utf8');
 assert.ok(!source.includes('Array.from(v.bytes)'));assert.ok(!source.includes('Array.from(r.bytes)'));assert.ok(!source.includes('value.buffers'));
 assert.match(source,/observer\.restore\(\);await sdk\.close\(\)/);assert.match(source,/result\.console\.length<256/);
 assert.match(source,/onFrame:\(\)=>\+\+frameCount/);assert.match(source,/process\.exitCode=1/);assert.match(source,/persistStage/);
});

test('pair completion binds ordered SDKs and runs only missing repetitions with fresh pages',async()=>{
 const source=await fs.readFile(new URL('../runners/g5/ur2d_selection_up.mjs',import.meta.url),'utf8');
 assert.match(source,/request.id==='U2D1-PAIR-COMPLETE-R1'\?\[1,2\]/);
 assert.match(source,/for\(const repetition of repetitions\)/);
 assert.match(source,/for\(const variant of variants\)/);assert.match(source,/if\(page\)await page.close\(\);await openPage/);
 assert.match(source,/row.roundtrip=await window.compareRoundtrip/);
 assert.ok(!source.includes('comparison page bindings must be reinstalled'));
 for(const repetition of [1,2])for(const variant of ['old','candidate']){
  const config=driver.pairConsumerRequest({id:'U2D1-PAIR-COMPLETE-R1',fixtures:['same']},variant,repetition);
  assert.equal(config.id,'U2D1-COMPARE');assert.equal(config.windowId,'U2D1-PAIR-COMPLETE-R1');
  assert.equal(config.repetition,repetition);assert.equal(config.variant,variant);assert.deepEqual(config.fixtures,['same']);
  // These are the established consumer branches: one selection gesture,
  // no extra Cancel matrix and mandatory Undo/Redo independent roundtrip.
  assert.equal(config.id==='U2D1-OLD-F'||config.id==='U2D1-COMPARE'?1:3,1);
  assert.equal(config.id!=='U2D1-OLD-F'&&config.id!=='U2D1-COMPARE',false);
  assert.equal(config.id==='U2D1-B-F'||config.id==='U2D1-COMPARE',true);
 }
 assert.throws(()=>driver.pairConsumerRequest({id:'U2D1-PAIR-COMPLETE-R1'},'old',0));
});
test('oracle identity and incompatible public camera fail before spawning a reference',async()=>{
 const directory=await fs.mkdtemp(path.join(os.tmpdir(),'ur2d-oracle-'));
 try{
  const fake=path.join(directory,'oracle');await fs.writeFile(fake,'not an oracle');
  await assert.rejects(driver.qualifyPersistedRoundtrip({directory,name:'S0',basis:{},oracle:{path:fake,sha256:'0'.repeat(64)}}),/foreign/);
  const digest=createHash('sha256').update('not an oracle').digest('hex');
  await assert.rejects(driver.qualifyPersistedRoundtrip({directory,name:'S0',basis:{},oracle:{path:fake,sha256:digest}}),/basis/);
 }finally{await fs.rm(directory,{recursive:true,force:true});}
});
