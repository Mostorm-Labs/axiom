import test from 'node:test';import assert from 'node:assert/strict';
import{rowIds,rowSpec,confirmedTrace,navigationEvents,expectedCamera}from'../runners/g5/cpu_edit_navigation_contract.mjs';
test('frozen rows retain both edit counts, monotonic confirmed times and excluded Up location',()=>{
 assert.equal(rowIds.length,12);assert.equal(new Set(rowIds).size,12);
 for(const edit of ['append','partial-erase']){const t=confirmedTrace(edit);assert.equal(t.filter(v=>v.phase==='move').length,edit==='append'?160:24);assert.equal(t.at(-1).x,999);for(let i=1;i<t.length;++i)assert.equal(t[i].timestampNs-t[i-1].timestampNs,8_000_000);}
 assert.throws(()=>rowSpec('append-pan-unknown'));
});
test('actual wheel schedule has six fixed events and independently checked final camera',()=>{
 const camera={scale:1.5,x:-200,y:-120};assert.deepEqual(expectedCamera(camera,'pan'),{scale:1.5,x:-140,y:-90});
 const inward=expectedCamera(camera,'zoom-in');assert.ok(Math.abs(inward.scale-1.725)<1e-12);assert.ok(Math.abs(inward.x-(-337.25))<1e-9);
 const outward=expectedCamera(camera,'zoom-out');assert.ok(Math.abs(outward.x-(-62.75))<1e-9);
 assert.equal(navigationEvents('pan').reduce((s,e)=>s+e.deltaX,0),-60);assert.equal(navigationEvents('zoom-out').at(-1).plannedOffsetMs,83.5);
});
