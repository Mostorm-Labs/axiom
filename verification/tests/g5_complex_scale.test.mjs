import test from 'node:test';
import assert from 'node:assert/strict';
import {assessStage,assertTierCoverage,summarizeCache} from '../runners/g5/complex_scale_metrics.mjs';
// A successful process or an index-only result cannot qualify full SDK load/render.
test('a partial process, wrong axis count or component cannot qualify root SDK',()=>{
 const expected={axis:'objects',count:1000,scope:'root-sdk'};
 assert.equal(assessStage({status:'completed',scope:'scene-index',objects:1000,points:0,checks:{count:true}},expected).classification,'BLOCKED_EVIDENCE');
 assert.equal(assessStage({status:'completed',scope:'root-sdk',objects:999,points:0,checks:{count:true}},expected).classification,'BLOCKED_EVIDENCE');
 assert.equal(assessStage({status:'partial',scope:'root-sdk',objects:1000,points:0,checks:{count:true}},expected).classification,'BLOCKED_EVIDENCE');
 assert.equal(assessStage({status:'completed',scope:'root-sdk',objects:1000,points:0,checks:{count:true,pixels:false}},expected).classification,'BLOCKED_EVIDENCE');
 assert.equal(assessStage({status:'completed',scope:'root-sdk',objects:1000,points:0,checks:{count:true,pixels:true}},expected).classification,'MEASURED');
});
test('point tiers count canonical points rather than objects or input samples',()=>{
 const expected={axis:'points',count:10000,scope:'root-sdk'};
 assert.equal(assessStage({status:'completed',scope:'root-sdk',objects:10000,input_samples:10000,checks:{count:true}},expected).classification,'BLOCKED_EVIDENCE');
 assert.equal(assessStage({status:'completed',scope:'root-sdk',objects:1,points:10000,checks:{count:true}},expected).classification,'MEASURED');
});
test('resource caps preserve the last stage and never manufacture success',()=>{
 const x=assessStage({status:'completed',scope:'root-sdk',objects:1000,checks:{ok:true},worker:{reason:'rss_limit',last_stage:'restore'}},{axis:'objects',count:1000,scope:'root-sdk'});
 assert.equal(x.classification,'BLOCKED_EVIDENCE');assert.equal(x.reason,'rss_limit');assert.equal(x.last_stage,'restore');
});
test('tier coverage requires an honest outcome for every declared tier',()=>{
 const rows=[{axis:'objects',count:1000},{axis:'objects',count:10000},{axis:'objects',count:100000}];
 assert.throws(()=>assertTierCoverage(rows,'objects',[1000,10000,100000,1000000]));
 assert.doesNotThrow(()=>assertTierCoverage([...rows,{axis:'objects',count:1000000,classification:'BLOCKED_EVIDENCE'}],'objects',[1000,10000,100000,1000000]));
});
test('cache occupancy is a peak gauge; absent diagnostics stay unavailable',()=>{
 assert.equal(summarizeCache([]),null);
 assert.deepEqual(summarizeCache([{budget_used_bytes:100,budget_recorded_bytes:40},{budget_used_bytes:80,budget_recorded_bytes:60}]),{peakUsedBytes:100,peakRecordedBytes:60});
});

test('a leaked worker or wrong DPR remains blocked despite a good object count',()=>{
 const row={status:'completed',scope:'root-sdk',objects:1000,points:0,dpr:1,checks:{ok:true}};
 assert.equal(assessStage({...row,worker:{reason:'completed',cleanup_complete:false}},{axis:'objects',count:1000,scope:'root-sdk'}).classification,'BLOCKED_EVIDENCE');
 assert.equal(assessStage(row,{axis:'objects',count:1000,scope:'root-sdk',dpr:2}).classification,'BLOCKED_EVIDENCE');
});
