import test from 'node:test';
import assert from 'node:assert/strict';
import {canonicalIdentity,qualifyQueues} from '../runners/g5/resource_recovery.mjs';

test('resource queue accounting rejects early or duplicate retirement',()=>{
  assert.equal(qualifyQueues([{owner:'1',admitted:'4',ordered_retired:'5',queued:0,needs_service:false}]).status,'INVALID');
  assert.equal(qualifyQueues([{owner:'1',admitted:'4',ordered_retired:'2',queued:1,needs_service:false}]).status,'INVALID');
});
test('completed observation cannot imply ordered convergence',()=>{
  assert.equal(qualifyQueues([{owner:'1',admitted:'4',ordered_retired:'2',queued:2,needs_service:true,observed:2}]).status,'MISSING');
});
test('qualified owners remain distinct and use lossless counters',()=>{
  const a={owner:'1',admitted:'18446744073709551614',ordered_retired:'18446744073709551613',queued:1,needs_service:true};
  const b={...a,ordered_retired:a.admitted,queued:0,needs_service:false};
  assert.equal(qualifyQueues([a,b,{owner:'2',admitted:'1',ordered_retired:'1',queued:0,needs_service:false}]).status,'OBSERVED');
  assert.equal(qualifyQueues([b,a]).status,'INVALID');
});
test('canonical receipt identity keeps operation and document epoch separate from surface',()=>{
  const state={documentId:'a',generation:'7',objects:16,lastCommit:{operationId:'b',ordinal:'9',runtimeEpoch:'3'},surface:{available:true}};
  assert.deepEqual(canonicalIdentity(state),canonicalIdentity({...state,surface:{available:false}}));
  assert.notDeepEqual(canonicalIdentity(state),canonicalIdentity({...state,lastCommit:{...state.lastCommit,runtimeEpoch:'4'}}));
  assert.notDeepEqual(canonicalIdentity(state),canonicalIdentity({...state,lastCommit:{...state.lastCommit,ordinal:'10'}}));
});
