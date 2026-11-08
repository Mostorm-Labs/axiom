import test from 'node:test';import assert from 'node:assert/strict';
import {CASES,consumer,retainColdStages} from '../runners/g5/eraser_readmission.mjs';
test('independent post-Up branches keep real wheel, preselected drag and cold oracle separate',()=>{
  assert.deepEqual(CASES,['reerase-cancel-navigation','pan','pinch','preselected-drag']);
  const src=consumer.toString();
  assert.match(src,/new WheelEvent\('wheel'/);assert.match(src,/ctrlKey:pinch/);
  assert.match(src,/p\.afterUp\.at\+4/);assert.match(src,/cold point selection/);
  assert.match(src,/point selection mutated document/);assert.match(src,/next Down rejected/);
  const hot=src.slice(src.indexOf('const run='),src.indexOf('const pixels='));
  assert.doesNotMatch(hot,/readPixels|captureSnapshot|settled\(\).*nextDown/);
  assert.match(src,/check\(activePhase===null/);assert.match(src,/checkpoint_mapping/);
  assert.doesNotMatch(src,/HEAP|_sdk_|performance\.memory/);
});

test('cold transaction preserves all available stages before a failed pixel predicate',async()=>{
  const saved=[];let result,error;
  try{result=await retainColdStages({
    capture:async stage=>({stage,rgba_sha256:stage==='before'?'a':'b',generation:stage==='before'?'1':stage==='undo'?'2':'3',pixels:Uint8Array.of(1,stage==='redo'?2:1),snapshot:Uint8Array.of(4,5)}),
    undo:async()=>{},redo:async()=>{},save:async s=>saved.push(s),
    check:(value,label,actual,expected)=>{if(!value)throw Object.assign(Error(label),{predicate:{actual,expected}});},
  });}catch(e){error=e;}
  assert.equal(saved.length,3,'failed predicate discarded the three cold stages');
  assert.deepEqual(saved.map(s=>s.stage),['before','undo','redo']);
  assert.deepEqual(Array.from(saved[2].pixels),[1,2]);
  assert.ok(error||result.status==='FAILED','mismatch must stay a failed observation');
});
