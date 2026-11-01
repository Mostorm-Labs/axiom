import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

const moduleURL = new URL('../runners/g5/browser_metrics.mjs', import.meta.url);
let metrics;
test.before(async () => {
  assert.ok(await fs.access(moduleURL).then(() => true, () => false), 'browser performance measurement helpers are missing');
  metrics = await import(moduleURL);
});
test('all input samples and down/up terminals are retained at every length', () => {
  for (const count of [2, 64, 256, 1024, 4096]) {
    const a = metrics.stroke(count, 42), b = metrics.stroke(count, 42);
    assert.deepEqual(a, b);
    assert.equal(a.length, count);
    assert.equal(a[0].phase, 'down');
    assert.equal(a.at(-1).phase, 'up');
    assert.ok(a.slice(1, -1).every(x => x.phase === 'move'));
    assert.ok(a.every((x, i) => !i || x.timestampMs > a[i - 1].timestampMs));
  }
});
test('no warmup samples or long tasks are silently removed from raw timings', () => {
  const value = metrics.statistics([1, 2, 100]);
  assert.equal(value.count, 3);
  assert.equal(value.p95, 100);
  assert.equal(metrics.statistics([]).p95, null);
  assert.throws(() => metrics.statistics([NaN]));
});
test('a browser receipt is not physical latency or a GPU timer', () => {
  const fields = metrics.unavailableMetrics();
  for (const name of ['physical_input_to_photon_ms', 'gpu_execution_ms', 'brush_append_ms',
                      'seal_ms', 'scene_projection_ms', 'skia_upload_bytes', 'preview_present_count']) {
    assert.equal(fields[name].value, null);
    assert.ok(fields[name].reason.length);
  }
});
test('accumulation is grouped by actual canonical object count', () => {
  const runs=[2,8,9,33].map(count=>({after:{objects:count},events:[{type:'pointermove',duration:count},{type:'pointerup',duration:count*2}],metrics:{up_to_canonical_callback_ms:count}}));
  const bins=metrics.accumulationSummary(runs);
  assert.equal(bins.length,3);assert.equal(bins[1].object_count_from,8);
  assert.equal(bins[1].move_ms.count,2);assert.equal(bins[2].up_commit_ms.p95,66);
});
test('the final accumulation bin retains the largest document and warmup object', () => {
  const runs=[1023,1024,1025].map(objects=>({after:{objects},events:[],metrics:{up_to_canonical_callback_ms:1}}));
  const bins=metrics.accumulationSummary(runs);
  assert.equal(bins.reduce((count,bin)=>count+bin.stroke_count,0),runs.length);
  assert.equal(bins.at(-1).object_count_from,1024);
  assert.equal(bins.at(-1).object_count_to,1025);
});
