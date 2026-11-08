import test from 'node:test';import assert from 'node:assert/strict';import {createHash} from 'node:crypto';
let api={};try{api=await import('../runners/g5/ur0/stream_transfer.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
test('public snapshot transport keeps exact bytes with a bounded single chunk',async()=>{
 assert.equal(typeof api.SnapshotTransfer,'function','bounded snapshot transport missing');
 const bytes=Uint8Array.from({length:1024*1024+3},(_,i)=>i%251),transfer=new api.SnapshotTransfer();
 transfer.begin(bytes);const hash=createHash('sha256');let received=0,max=0;
 await api.copySnapshot({length:bytes.length,pull:(offset,count)=>transfer.pull(offset,count),write:chunk=>{hash.update(chunk);received+=chunk.length;max=Math.max(max,chunk.length);}});
 assert.equal(received,bytes.length);assert.ok(max<=32768);assert.equal(hash.digest('hex'),createHash('sha256').update(bytes).digest('hex'));
 transfer.end();assert.throws(()=>transfer.pull(0,1),/active/);
});
test('snapshot transport rejects overlap, missing bytes, duplicate begin and oversized chunks',()=>{
 assert.equal(typeof api.SnapshotTransfer,'function','bounded snapshot transport missing');
 const transfer=new api.SnapshotTransfer();transfer.begin(new Uint8Array(50000));
 assert.throws(()=>transfer.begin(new Uint8Array(3)),/active/);assert.throws(()=>transfer.pull(0,32769),/bounded/);
 transfer.pull(0,100);assert.throws(()=>transfer.pull(0,1),/offset/);assert.throws(()=>transfer.end(),/incomplete/);transfer.end(true);
});
let cold={};try{cold=await import('../runners/g5/ur0/correctness_transfer.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
test('BOUNDED-TRANSFER: cold batches roundtrip zero, unit, batch boundary and multi-MiB with source and received SHA',async()=>{
 assert.equal(typeof cold.copyCorrectnessBytes,'function');assert.equal(typeof cold.encodeChunkBatch,'function');
 for(const length of [0,1,32768,262144,262145,3*1024*1024+17]){
  const bytes=Uint8Array.from({length},(_,i)=>i%251),s=new api.SnapshotTransfer();s.begin(bytes);const parts=[];let done=0,cancel=0,maxBatch=0;
  const result=await cold.copyCorrectnessBytes({length,source_sha256:sha(bytes),pullBatch:offset=>{const b=cold.encodeChunkBatch({length,offset,pull:(o,n)=>s.pull(o,n)});assert.ok(b.units.length<=8);assert.ok(b.units.every(u=>u.byte_length<=32768));maxBatch=Math.max(maxBatch,b.decoded_bytes);return b;},write:b=>parts.push(b),finish:()=>{s.end();++done;},cancel:()=>{s.end(true);++cancel;}});
  assert.equal(result.received_sha256,sha(bytes));assert.equal(sha(Buffer.concat(parts)),sha(bytes));assert.ok(maxBatch<=262144);assert.equal(done,1);assert.equal(cancel,0);
 }
});
test('BOUNDED-TRANSFER: corrupt offsets, encoding, count, batch bounds, hash and interrupted writes cancel ownership exactly once',async()=>{
 assert.equal(typeof cold.copyCorrectnessBytes,'function');
 for(const corrupt of [b=>b.offset++,b=>b.units[0].offset++,b=>b.units.push(b.units[0]),b=>b.decoded_bytes++,b=>b.units[0].base64='***',b=>b.units[0].base64='AB==',b=>b.units[0].byte_length++,b=>b.units=[],b=>b.next_offset--,b=>b.units=Array(9).fill(b.units[0])]){
  const bytes=new Uint8Array(32769),s=new api.SnapshotTransfer();s.begin(bytes);let cancelled=0,finished=0;
  await assert.rejects(cold.copyCorrectnessBytes({length:bytes.length,source_sha256:sha(bytes),pullBatch:offset=>{const b=cold.encodeChunkBatch({length:bytes.length,offset,pull:(o,n)=>s.pull(o,n)});corrupt(b);return b;},write:()=>{},finish:()=>++finished,cancel:()=>{s.end(true);++cancelled;}}));
  assert.equal(cancelled,1);assert.equal(finished,0);
 }
 for(const why of ['hash','write']){let cancelled=0;const s=new api.SnapshotTransfer();s.begin(Uint8Array.of(1));await assert.rejects(cold.copyCorrectnessBytes({length:1,source_sha256:sha(Uint8Array.of(why==='hash'?2:1)),pullBatch:offset=>cold.encodeChunkBatch({length:1,offset,pull:(o,n)=>s.pull(o,n)}),write:()=>{if(why==='write')throw Error('disk');},finish:()=>s.end(),cancel:()=>{s.end(true);++cancelled;}}));assert.equal(cancelled,1);}
 const exhausted=new api.SnapshotTransfer();exhausted.begin(Uint8Array.of(1));exhausted.pull(0,1);assert.throws(()=>exhausted.pull(1,1),/exhausted/);exhausted.end();
});
