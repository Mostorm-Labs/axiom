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
