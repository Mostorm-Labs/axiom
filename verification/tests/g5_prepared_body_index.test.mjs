import test from 'node:test';
import assert from 'node:assert/strict';
import {CASES,historyCase,summarizePrepared,restoreCamera} from '../runners/g5/prepared_body_index.mjs';

const record=(overrides={})=>({service:'0xabc',lookups:0,index_queries:0,builds:0,identity_hits:0,fingerprint_bytes:0,content_comparisons:0,candidate_checks:0,navigation_queries:0,ink_queries:0,source_footprint_evaluations:0,source_known_footprint_hits:0,aliases:1,bodies:1,index_reservation_bytes:512,resident_accounted_bytes:4096,...overrides});
test('actual ordered consumers require the same service and a reused body',()=>{
  const nav=record({lookups:2,builds:1,navigation_queries:2});
  const ink=record({purpose:'edit',lookups:1,ink_queries:1,identity_hits:1});
  const value=summarizePrepared([nav,ink]);assert.equal(value.status,'OBSERVED');
  assert.deepEqual(value.shared_sequences[0],{service:'0xabc',navigation_record:0,ink_record:1,ink_identity_hits:1,ink_verified_comparisons:0});
  assert.equal(summarizePrepared([nav,{...ink,service:'other'}]).status,'MISSING');
  assert.equal(summarizePrepared([ink,nav]).status,'MISSING');
  assert.equal(summarizePrepared([nav,{...ink,builds:1}]).status,'MISSING');
});
test('unknown equal allocations may use complete comparison before sharing',()=>{
  const value=summarizePrepared([record({navigation_queries:1,builds:1}),record({ink_queries:1,content_comparisons:1,candidate_checks:1,fingerprint_bytes:70})]);
  assert.equal(value.status,'OBSERVED');assert.equal(value.shared_sequences[0].ink_verified_comparisons,1);
});
test('a service address or parent trace alone does not establish actual consumer work',()=>{
  assert.equal(summarizePrepared([record(),record({ink_queries:1})]).status,'MISSING');
  assert.equal(summarizePrepared([]).status,'MISSING');
  const invalid=summarizePrepared([record({navigation_queries:1}),record({ink_queries:1,identity_hits:1,lookups:2**54})]);
  assert.equal(invalid.status,'MISSING');assert.deepEqual(invalid.invalid_records,[1]);
  assert.equal(summarizePrepared([record({navigation_queries:-1})]).status,'MISSING');
});
test('fixed saved-fixture cases preserve drag/cancel and add settled erase/navigation',()=>{
  assert.deepEqual(CASES,['S0-canonical','S0-drag','S0-erase-navigation','S2-canonical','S2-drag','S2-erase-navigation']);
  assert.deepEqual(CASES.map(historyCase),[false,true,true,false,true,true]);
});

test('public camera restore awaits absolute zoom before calculating pan',async()=>{
  let camera={scale:2.6,x:12,y:-4};const calls=[];
  const sdk={async zoomAt(x,y,scale){await Promise.resolve();calls.push(['zoom',x,y,scale]);camera={scale,x:44,y:55};},
    readState:()=>({camera}),async panBy(x,y){calls.push(['pan',x,y]);camera={...camera,x:camera.x+x,y:camera.y+y};}};
  await restoreCamera(sdk,{scale:1,x:0,y:0});
  assert.deepEqual(calls,[['zoom',400,250,1],['pan',-44,-55]]);assert.deepEqual(camera,{scale:1,x:0,y:0});
});
