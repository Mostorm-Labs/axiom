import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';
import {EvidenceStore} from '../runners/g5/pending_navigation_evidence_io_store.mjs';
import {acknowledgeRow, verifyCompletion} from '../runners/g5/pending_navigation_evidence_io_client.mjs';

const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
const keys=['baseline-nextdraw','candidate-zoom','candidate-pan','candidate-nextdraw'];
const pixel=()=>({metadata:{status:'VALID',restoration:{status:'RESTORED'},canvas:{width:1,height:1},rectangle:{width:1,height:1}},base64:Buffer.from([10,20,30,255]).toString('base64')});
const row=key=>{const [lane,action]=key.split('-');return {q:{lane,action,pixel:true,fixture:{base64:'AA==',metadata:{}}},status:'OBSERVED',closed:true,pixelRAFRestored:true,pixels:[{label:'final',...pixel(),preview:{...pixel(),visibility:'visible'}}],snapshots:{final:{base64:'AQI=',metadata:{}}},operations:[{at:1,base64:'AwQ='}]};};
test('per-row metadata never accumulates pixel base64; gzip bytes and counts are exact',async()=>{
 const directory=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-pnio-'));
 try{const store=new EvidenceStore(directory,keys);const ack=await store.save(row(keys[0]));assert.equal(ack.saved,true);assert.equal(ack.count,1);const index=await store.index();assert.equal(index.rows.length,1);assert.equal(JSON.stringify(index).includes('base64'),false);
 const saved=JSON.parse(await fs.readFile(path.join(directory,ack.path),'utf8'));assert.equal(JSON.stringify(saved).includes('base64'),false);const value=saved.pixels[0];for(const target of [value,value.preview]){const bytes=gunzipSync(await fs.readFile(path.join(directory,target.buffer.path)));assert.equal(bytes.length,4);assert.equal(target.buffer.bytes,4);assert.equal(target.buffer.sha256,sha(bytes));}
 assert.equal(gunzipSync(await fs.readFile(path.join(directory,saved.snapshots.final.buffer.path))).length,2);
 }finally{await fs.rm(directory,{recursive:true,force:true});}
});
test('unknown/duplicate/missing rows cannot become complete',async()=>{
 const directory=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-pnio-'));
 try{const store=new EvidenceStore(directory,keys);await assert.rejects(store.save(row('candidate-unknown')),/unknown/);await store.save(row(keys[0]));await assert.rejects(store.save(row(keys[0])),/duplicate/);assert.throws(()=>verifyCompletion({expected:keys,rows:(store.entries),complete:false},keys),/missing/);for(const key of keys.slice(1))await store.save(row(key));assert.equal(verifyCompletion(await store.index(),keys).complete,true);
 }finally{await fs.rm(directory,{recursive:true,force:true});}
});
test('actual filesystem persistence failure leaves no success ACK/index row',async()=>{
 const directory=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-pnio-'));
 try{const blocked=path.join(directory,'file');await fs.writeFile(blocked,'not a directory');const store=new EvidenceStore(blocked,keys);await assert.rejects(store.save(row(keys[0])));assert.equal(store.entries.length,0);
 }finally{await fs.rm(directory,{recursive:true,force:true});}
});
test('HTTP failure and wrong ACK cannot be reported COMPLETE',async()=>{
 await assert.rejects(acknowledgeRow({ok:false,status:500,text:async()=> 'disk full'},keys[0],1),/HTTP 500/);
 await assert.rejects(acknowledgeRow({ok:true,json:async()=>({saved:false,key:keys[0],count:1})},keys[0],1),/ACK/);
 await assert.rejects(acknowledgeRow({ok:true,json:async()=>({saved:true,key:keys[1],count:1})},keys[0],1),/ACK/);
 assert.equal((await acknowledgeRow({ok:true,json:async()=>({saved:true,key:keys[0],count:1,sha256:'a'.repeat(64)})},keys[0],1)).saved,true);
});
