// Verification-only deterministic workload and honest measurement vocabulary.
export function stroke(count, seed = 42) {
  if (!Number.isInteger(count) || count < 2 || count > 65536) throw Error('sample count must be 2..65536');
  let state = seed >>> 0;
  return Array.from({length: count}, (_, i) => {
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    return {phase: i === 0 ? 'down' : i === count - 1 ? 'up' : 'move',
      x: 30 + i / (count - 1) * 700, y: 220 + Math.sin(i * .08) * 90 + (state / 2 ** 32 - .5),
      pressure: .5, timestampMs: 1000 + i * 4};
  });
}
export function statistics(samples) {
  if (samples.some(x => typeof x !== 'number' || !Number.isFinite(x) || x < 0)) throw Error('invalid timings');
  const sorted = [...samples].sort((a, b) => a - b);
  const p = q => sorted.length ? sorted[Math.max(0, Math.ceil(q * sorted.length) - 1)] : null;
  return {count: sorted.length, p50: p(.5), p95: p(.95), p99: p(.99), max: sorted.at(-1) ?? null,
    mean: sorted.length ? sorted.reduce((a, b) => a + b, 0) / sorted.length : null};
}
export function unavailableMetrics() {
  return Object.fromEntries(Object.entries({
    physical_input_to_photon_ms: 'Requires timestamped physical input and screen observation; synthetic DOM events cannot measure it.',
    gpu_execution_ms: 'No GPU timestamp query is exposed by the root SDK.',
    brush_append_ms: 'Contained within synchronous pointer dispatch; no production instrumentation added.',
    seal_ms: 'Contained within pointerup dispatch, together with Operation apply and Scene projection.',
    scene_projection_ms: 'Contained within commit; cannot isolate by subtracting unlike measurements.',
    skia_upload_bytes: 'No exact upload byte counter is exposed by the root SDK.',
    preview_present_count: 'Canonical onFrame callbacks do not count preview provider presents.',
  }).map(([key, reason]) => [key, {value: null, reason}]));
}
export function accumulationSummary(runs) {
  const boundaries=[1,8,32,128,512,1024];
  const maximum=runs.reduce((value,run)=>Math.max(value,run.after.objects),0);
  return boundaries.flatMap((boundary,index) => {
    const end=index+1<boundaries.length?boundaries[index+1]-1:Math.max(boundary,maximum);
    const selected = runs.filter(r => r.after.objects >= boundary && r.after.objects <= end);
    return selected.length ? [{object_count_from:boundary, object_count_to:end,
      stroke_count:selected.length,
      move_ms:statistics(selected.flatMap(r=>r.events.filter(e=>e.type==='pointermove').map(e=>e.duration))),
      up_commit_ms:statistics(selected.flatMap(r=>r.events.filter(e=>e.type==='pointerup').map(e=>e.duration))),
      canonical_callback_ms:statistics(selected.map(r=>r.metrics.up_to_canonical_callback_ms).filter(x=>x!==null))}] : [];
  });
}
