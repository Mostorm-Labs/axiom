import test from 'node:test';
import assert from 'node:assert/strict';
import {gpuProbeStatus} from '../runners/g5/gpu_probe_status.mjs';
test('an early process exit cannot qualify unfinished native GPU work',()=>{
  assert.equal(gpuProbeStatus({code:0,errors:[]},[]),'FAIL');
});
test('stderr rejection cannot be hidden by a zero process exit',()=>{
  assert.equal(gpuProbeStatus({nativeDone:true,code:0,errors:['pixel mismatch']},[]),'FAIL');
  assert.equal(gpuProbeStatus({nativeDone:true,code:1,errors:[]},[]),'FAIL');
});
test('only completed native work without browser or native errors passes',()=>{
  const native={nativeDone:true,code:0,errors:[]};
  assert.equal(gpuProbeStatus(native,['WebGL error']),'FAIL');
  assert.equal(gpuProbeStatus(native,[]),'PASS');
});
