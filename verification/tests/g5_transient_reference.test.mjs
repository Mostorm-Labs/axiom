import test from 'node:test';
import assert from 'node:assert/strict';
import {summarizeDurations, summarizePhase} from '../runners/g5/transient_reference.mjs';

test('local timings retain a long handler and actual content submission gaps', () => {
  assert.deepEqual(summarizeDurations([1, 2, 3, 100]), {count:4, p50:2, p95:100, max:100});
  const row=summarizePhase({started:0, inputEnded:100, ended:150,
    inputEvents:[{started:1,ended:2},{started:30,ended:60}],
    colorOps:[{kind:'clear',ended:5},{kind:'content',ended:20},{kind:'masked',ended:30},{kind:'content',ended:90}]});
  assert.equal(row.handler_ms.max,30);
  assert.equal(row.worst_content_submission_gap_ms,70);
  assert.equal(row.content_writes,2);
  assert.equal(row.canonical_wait_ms,50);
});

test('missing real content writes cannot be reported as a zero submission gap', () => {
  const row=summarizePhase({started:0,inputEnded:100,ended:120,inputEvents:[],colorOps:[{kind:'clear',ended:5}]});
  assert.equal(row.worst_content_submission_gap_ms,null);
  assert.equal(row.content_writes,0);
  assert.throws(()=>summarizeDurations([1,NaN]));
  assert.throws(()=>summarizePhase({started:10,inputEnded:1,ended:20,inputEvents:[],colorOps:[]}));
});
