import test from 'node:test';import assert from 'node:assert/strict';
import * as metrics from '../runners/g5/feedback_metrics.mjs';
test('ABBA separates a10ms source effect from linear2ms order drift',()=>{
 assert.deepEqual(metrics.counterbalancedEffect?.([10,22,24,16]),{baseline:13,candidate:23,effect:10});
});
test('a short pilot cannot establish nonregression even with perfect signs',()=>{
 assert.equal(metrics.compareBlockEffects?.(Array(7).fill(-1))?.status,'INCONCLUSIVE');
});
test('eight independent signs distinguish nonregression,regression and ties',()=>{
 const below=metrics.compareBlockEffects?.(Array(8).fill(-1));assert.ok(below);
 assert.equal(below.status,'NONREGRESSION_SUPPORTED');assert.equal(below.nonregressionP,1/256);
 assert.equal(metrics.compareBlockEffects?.(Array(8).fill(1))?.status,'REGRESSION_DETECTED');
 assert.equal(metrics.compareBlockEffects?.(Array(8).fill(0))?.status,'INCONCLUSIVE');
});
