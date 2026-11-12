import test from 'node:test';import assert from 'node:assert/strict';
import {validateChunk}from'../runners/g5/ur2d_selection_up_contract.mjs';
import {fields,objectValue}from'../runners/g5/cpu_v2_wire.mjs';import fs from'node:fs';
test('frozen fixture provides exact40 unique selection IDs and16 transformed inputs',()=>{
 const b=fs.readFileSync(new URL('../../.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/initial.axsnap',import.meta.url));
 const objects=fields(b).filter(v=>v.number===3).map(v=>objectValue(v.value));
 assert.equal(objects.length,40);assert.equal(new Set(objects.map(o=>o.id)).size,40);
 assert.equal(new Set(objects.map(o=>JSON.stringify(o.pose))).size,16);
});
test('pixel/operation persistence validates size from payload and offsets',()=>{
 const bytes=Buffer.alloc(65536,17),a={name:'AB.rgba',offset:0,total:65539,base64:bytes.toString('base64')};
 const first=validateChunk(a);assert.equal(first.size,65536);assert.equal(first.complete,false);
 const last=validateChunk({...a,offset:65536,base64:Buffer.from([1,2,3]).toString('base64')},first);
 assert.equal(last.complete,true);assert.equal(last.nextOffset,65539);
 assert.throws(()=>validateChunk({...a,offset:65535},first),/order/);
});
