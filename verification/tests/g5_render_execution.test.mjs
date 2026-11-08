import test from 'node:test';import assert from 'node:assert/strict';
import {CASES,consumer,retainColdStages} from '../runners/g5/render_execution.mjs';
test('UR1b frozen endpoints exclude suspended UR2 flow',()=>{
 assert.deepEqual(CASES,['S0-canonical','S0-drag','S2-canonical','S2-drag']);
 assert.doesNotMatch(consumer.toString(),/nextDown|eraserBegin|pendingBrush|partialEraser/);
 assert.match(consumer.toString(),/selected-drag-commit/);assert.match(consumer.toString(),/selected-drag-cancel/);
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

test('cold capture failure preserves its completed prefix',async()=>{
 const saved=[];const result=await retainColdStages({capture:async stage=>{if(stage==='undo')throw Error('snapshot unavailable');return {stage,generation:'0',rgba_sha256:'a',pixels:Uint8Array.of(1)};},undo:async()=>{},redo:async()=>{},save:async s=>saved.push(s),check:assert.ok});
 assert.equal(result.status,'FAILED');assert.equal(saved.length,1);assert.equal(result.stages[0].stage,'before');assert.match(result.failure.message,/snapshot unavailable/);
});
