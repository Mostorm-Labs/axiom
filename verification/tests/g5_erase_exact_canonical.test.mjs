import test from 'node:test';
import assert from 'node:assert/strict';
import {tracePoint,WINDOWS,qualifyRounds} from '../runners/g5/erase_exact_canonical_contract.mjs';

test('frozen long trace and explicit window bounds',()=>{
 assert.deepEqual(tracePoint(0,0),[420,160]);
 assert.deepEqual(tracePoint(2,160),[384+7*Math.sin(160*.17),330]);
 assert.equal(WINDOWS['G5EC1-BASELINE'].seconds,180);
 assert.equal(WINDOWS['G5EC1-FINAL-PAIRS'].seconds,360);
 assert.equal(WINDOWS['G5EC1-STRESS'].seconds,180);
 assert.throws(()=>tracePoint(0,161));
});
test('stress intermediate rounds need admission/decision proof while only final tail requires idle',()=>{
 const r={mainCommitted:false,mainContacts:0,immediateDownContacts:1,tinyContacts:0,generationDelta:0,operationDelta:0,noopProven:true,after:{activeContacts:0,generation:'1',lastCommit:{presentedGeneration:'0'}},activity:{pending:1}};
 const final={...r,after:{activeContacts:0,generation:'1',lastCommit:{presentedGeneration:'1'}},activity:{pending:0}};
 assert.equal(qualifyRounds([r,final],2,{stress:true}).status,'QUALIFIED');
 assert.equal(qualifyRounds([r,{...final,noopProven:false}],2,{stress:true}).status,'FAILED');
});
test('completion requires actual per-round commits, closed contacts and tail quiescence',()=>{
 const round={mainCommitted:true,mainContacts:0,immediateDownContacts:1,tinyContacts:0,
  generationDelta:2,operationDelta:2,after:{activeContacts:0,generation:'26',lastCommit:{presentedGeneration:'26'}},activity:{pending:0}};
 const rows=[{...round},{...round},{...round,generationDelta:1,operationDelta:1}];
 assert.equal(qualifyRounds(rows,3).status,'QUALIFIED');
 assert.equal(qualifyRounds(rows.slice(1),3).status,'FAILED');
 assert.equal(qualifyRounds([{...round,mainCommitted:false},...rows.slice(1)],3).status,'FAILED');
 assert.equal(qualifyRounds([{...round,activity:{pending:1}},...rows.slice(1)],3).status,'FAILED');
});
