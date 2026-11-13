// Offline analysis only; never opens a browser or calls the SDK.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {startupMetrics, startupGate} from './settled_zoom_start_metrics.mjs';

const root = path.resolve(new URL('../../..', import.meta.url).pathname);
const out = path.join(root, 'out/G5-settled-zoom-start-rect-repair-r2');
const results = path.join(root, '.aegis/results/G5-settled-zoom-start-rect-repair-r2');
const read = name => JSON.parse(fs.readFileSync(path.join(root, name)));
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const rel = name => path.relative(root, name);
const median = values => {
  const sorted = [...values].sort((a, b) => a - b);
  return (sorted[Math.floor((sorted.length - 1) / 2)] + sorted[Math.ceil((sorted.length - 1) / 2)]) / 2;
};
const rows = [];
for (const [id, browser] of [['SZ2-CHROME-1', 'Chrome'], ['SZ2-CHROME-2', 'Chrome'], ['SZ2-IAB', 'IAB']]) {
  const file = path.join(out, browser === 'Chrome' ? 'web' : 'iab', id, 'raw.json');
  const raw = JSON.parse(fs.readFileSync(file));
  for (const row of raw.rows) {
    const phase = row.phases.at(-1);
    const first = phase.wheels[0].started;
    const lastWheel = phase.wheels.at(-1).end;
    const callbacks = phase.rafCallbacks.filter(value => value.source === 'sdk' && value.started >= first);
    const writes = phase.colorOps.filter(value => value.kind === 'content' && value.started >= first);
    rows.push({
      id, browser, lane: row.lane, case: row.q.name, pixel: !!row.q.pixel,
      raw: {path: rel(file), sha256: sha(fs.readFileSync(file))},
      ...startupMetrics(phase),
      maxRAFStartGapMs: Math.max(0, ...callbacks.slice(1).map((value, i) => value.started - callbacks[i].started)),
      maxContentStartGapMs: Math.max(0, ...writes.slice(1).map((value, i) => value.started - writes[i].started)),
      firstWheelToSettledMs: phase.settledAt - first,
      lastWheelToSettledMs: phase.settledAt - lastWheel,
      rootErrors: row.errors, closed: row.closed, status: row.status,
      initialCamera: phase.preNavigationStable.state.camera,
      finalCamera: row.end.camera,
      reportedBuild: row.build,
    });
  }
}
const comparisons = [];
for (const browser of ['Chrome', 'IAB']) {
  for (const name of ['outside-one-stroke', 'outside-three-strokes-masked', 'central-short']) {
    const matching = rows.filter(row => row.browser === browser && row.case === name && !row.pixel);
    const lanes = Object.fromEntries(['bc40', 'r1', 'r2'].map(lane => {
      const subset = matching.filter(row => row.lane === lane);
      return [lane, {
        count: subset.length,
        firstContentMs: subset.map(row => row.firstContentMs),
        medianFirstContentMs: median(subset.map(row => row.firstContentMs)),
        missedOpportunities: subset.map(row => row.missedOpportunities),
        medianMissedOpportunities: median(subset.map(row => row.missedOpportunities)),
        maxRAFMs: Math.max(...subset.map(row => row.maxRAFMs)),
        maxInputLatenessMs: Math.max(...subset.map(row => row.maxInputLatenessMs)),
        maxRAFStartGapMs: Math.max(...subset.map(row => row.maxRAFStartGapMs)),
        lastWheelToSettledMs: subset.map(row => row.lastWheelToSettledMs),
      }];
    }));
    comparisons.push({browser, case: name, primary: name !== 'central-short', lanes,
      startupCriterion: startupGate(matching.filter(row => row.lane === 'bc40'), matching.filter(row => row.lane === 'r2')),
      relativeStartupReduction: 1 - lanes.r2.medianFirstContentMs / lanes.bc40.medianFirstContentMs,
      maxRAFDeltaMs: lanes.r2.maxRAFMs - lanes.bc40.maxRAFMs,
      maxInputLatenessDeltaMs: lanes.r2.maxInputLatenessMs - lanes.bc40.maxInputLatenessMs,
    });
  }
}
const report = {
  schema: 'axiom-settled-zoom-rectangle-performance-v1',
  ordinary: rows.filter(row => !row.pixel),
  excludedPixelTimingRows: rows.filter(row => row.pixel).length,
  comparisons,
  primaryStartupCriterionSatisfied: comparisons.filter(row => row.primary).every(row => row.startupCriterion.startupImproved),
  qualification: 'Finite two counterbalanced Chrome starts and one IAB page; GL default-target content API call is a proxy, not GPU completion/current-camera pixels/physical photon or canonical receipt. Pixel readbacks excluded from timing qualification. No overall performance PASS.',
  sourceBasis: 'All lanes restored matching exact baseline before-zoom snapshots and metadata; natural settled+1s then original 24-wheel sequence. Exact SDK asset identity overrides coarse embedded sourceRevision labels; no SDK rebuilt during runs.',
};
fs.writeFileSync(path.join(results, 'performance-observation.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({primaryStartupCriterionSatisfied: report.primaryStartupCriterionSatisfied,
  comparisons: comparisons.map(row => ({browser: row.browser, case: row.case,
    baselineMs: row.lanes.bc40.medianFirstContentMs, candidateMs: row.lanes.r2.medianFirstContentMs,
    RAFDeltaMs: row.maxRAFDeltaMs, latenessDeltaMs: row.maxInputLatenessDeltaMs}))}, null, 2));
