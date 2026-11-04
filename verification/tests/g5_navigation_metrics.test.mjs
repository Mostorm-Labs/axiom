import test from 'node:test';
import assert from 'node:assert/strict';
import {summarizePhase} from '../runners/g5/navigation_metrics.mjs';

// Dropping the slow frame or adding cumulative counts instead of per-call
// deltas must change these hand-derived expected values.
test('navigation summary retains tails and sums diagnostic deltas', () => {
  const phase = {samples:[{interval:10,draws:1},{interval:20,draws:0},{interval:90,draws:1}],
    displayTimes:[0,120], framesDuringInput:0, settleMilliseconds:150,
    fences:[{name:'clientWaitSync',duration:2,timeout:0}], longTasks:[]};
  const rows = [{elapsed_ns:10,overlay_ns:2,queries:1,built:2,knots:32,prefix_knots:64,fallback:3,cancelled:0},
    {elapsed_ns:20,overlay_ns:3,queries:2,built:1,knots:64,prefix_knots:0,fallback:4,cancelled:1}];
  const value = summarizePhase(phase, rows, []);
  assert.equal(value.raf_ms.p95,90);
  assert.equal(value.display_submission_gap_ms.max,120);
  assert.equal(value.updatedInputs,2);
  assert.equal(value.navigation.queries,3);
  assert.equal(value.navigation.fallback,7);
  assert.ok(Math.abs(value.navigation.overlay_cpu_ms.total-0.000005)<1e-18);
  assert.equal(value.gpu_execution_ms.value,null);
});

test('missing diagnostic rows remain unavailable rather than invented zero', () => {
  const value=summarizePhase({samples:[],displayTimes:[],fences:[],longTasks:[]},[],[]);
  assert.equal(value.navigation,null);
  assert.equal(value.raf_ms.p95,null);
  assert.equal(value.display_submission_gap_ms.max,null);
});

test('input display gap includes the first response and excludes exact settle', () => {
  const value=summarizePhase({started:10,inputEnded:100,samples:[],
    displayTimes:[40,60,600],fences:[],longTasks:[]},[],[]);
  assert.equal(value.input_display_submission_gap_ms.max,40);
  assert.equal(value.display_submission_gap_ms.max,540);
});

test('prepared geometry deltas are summed while resident bytes are a peak gauge', () => {
  const phase={samples:[],displayTimes:[],fences:[],longTasks:[]};
  const value=summarizePhase(phase,[
    {elapsed_ns:0,overlay_ns:0,prepared_builds:1,prepared_hits:2,prepared_denied:0,prepared_bytes:100,contour_tested:20,contour_drawn:3,vector_prepared:0},
    {elapsed_ns:0,overlay_ns:0,prepared_builds:0,prepared_hits:4,prepared_denied:1,prepared_bytes:80,contour_tested:10,contour_drawn:2,vector_prepared:1}
  ],[]);
  assert.equal(value.navigation.prepared.builds,1);
  assert.equal(value.navigation.prepared.hits,6);
  assert.equal(value.navigation.prepared.denied,1);
  assert.equal(value.navigation.prepared.peakResidentBytes,100);
  assert.equal(value.navigation.prepared.contourDrawn,5);
  assert.equal(value.navigation.prepared.vectorUses,1);
});

test('an interrupted selection receipt cannot qualify navigation settlement', () => {
  const phase={started:10,inputEnded:100,samples:[],displayTimes:[],fences:[],longTasks:[],
    settleMilliseconds:-50,receipts:[{at:50}]};
  assert.equal(summarizePhase(phase,[],[]).settle_ms,null);
  phase.receipts.push({at:145},{at:160});
  assert.equal(summarizePhase(phase,[],[]).settle_ms,45);
});
