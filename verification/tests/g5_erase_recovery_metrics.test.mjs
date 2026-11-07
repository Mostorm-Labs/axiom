import test from'node:test';import assert from'node:assert/strict';
import{canonicalRecoveryTiming}from'../runners/g5/erase_recovery_metrics.mjs';
const trace=(at,coverage,child=0)=>({at,text:'AXIOM_FRAME_TIMING '+JSON.stringify({stages:{attempt:{inclusive_ns:20e6},cpu_coverage:coverage,contour_draw:{inclusive_ns:child}}})});
const raw=records=>({upAt:100,started:90,inputEnded:120,ended:150,nativeRecords:records});
test('nested contour scopes do not make coverage CPU disappear or double count',()=>{
 const a=canonicalRecoveryTiming(raw([trace(110,{inclusive_ns:12e6,exclusive_ns:12e6})]));
 const b=canonicalRecoveryTiming(raw([trace(110,{inclusive_ns:12e6,exclusive_ns:2e6},10e6)]));
 assert.equal(a.cpuCoverageMs,12);assert.equal(b.cpuCoverageMs,12);
});
test('unavailable diagnostics are not measured zero CPU',()=>{
 const v=canonicalRecoveryTiming(raw([]));assert.equal(v.cpuCoverageMs,null);assert.equal(v.maxCanonicalAttemptMs,null);assert.equal(v.diagnosticCanonicalAttempts,0);
});
test('canonical attempts before pointer Up stay outside recovery totals',()=>{
 const v=canonicalRecoveryTiming(raw([trace(95,{inclusive_ns:99e6,exclusive_ns:99e6}),trace(110,{inclusive_ns:7e6,exclusive_ns:1e6},6e6),trace(115,{inclusive_ns:5e6,exclusive_ns:5e6})]));
 assert.equal(v.cpuCoverageMs,12);assert.equal(v.maxCanonicalAttemptMs,20);assert.equal(v.recoveryAfterInputMs,30);
});
