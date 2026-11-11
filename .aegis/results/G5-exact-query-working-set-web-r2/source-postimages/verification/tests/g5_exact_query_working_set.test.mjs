import test from 'node:test';import assert from 'node:assert/strict';
import {plannedCases,typedWitness,qualifyWitness,navigationWitness} from '../runners/g5/exact_query_working_set_contract.mjs';
test('three paired waveforms use matching ordinary variants and isolated pages',()=>{
 const rows=plannedCases('G5QW1-FINAL-PAIRS','oldSDK','newSDK');assert.equal(rows.length,12);assert.equal(new Set(rows.map(v=>v.id)).size,12);
 for(const pair of[0,1,2])for(const mode of['long','growth']){const a=rows.filter(v=>v.pair===pair&&v.mode===mode);assert.equal(a.length,2);assert.deepEqual(a.map(v=>v.variant),['old','candidate']);assert.equal(a[0].moves,a[1].moves);}
 assert.throws(()=>plannedCases('foreign','old','new'));
});
test('pan and zoom witnesses cannot match an unchanged old footprint',()=>{
 for(const kind of['pan','zoom']){const w=navigationWitness(kind,{scale:1,x:0,y:0});assert(w.point[0]<w.old[0]-7||w.point[0]>w.old[2]+7);assert(w.point[0]>w.next[0]+7&&w.point[0]<w.next[2]-7);}
 assert.throws(()=>navigationWitness('pan',{scale:2,x:0,y:0}));
});
test('ten rounds plus all eight successors are finite and retain typed geometry',()=>{
 const rows=plannedCases('G5QW1-CONTINUOUS','old','new');assert.equal(rows.length,9);assert.equal(rows[0].roundCount,10);
 assert.deepEqual(rows.slice(1).map(v=>v.kind),['ready','erase','pan','zoom','rectangle','sticky','table','image']);
 const s={primary:'a',generation:'1',bounds:[899,299,1021,381],transform:[1,0,0,1,900,300],appearance:{fill:[.2,.5,.8,1]}};
 const w=typedWitness('rectangle',s,{available:true,objectId:'a',width:120,height:80});assert.deepEqual(w.color,[51,128,204,255]);
 assert.equal(qualifyWitness(w,new Uint8Array([255,255,128,255]),[80,30]).status,'FAILED');
 assert.equal(qualifyWitness(w,new Uint8Array([51,128,204,255]),[80,30]).status,'QUALIFIED');
});
