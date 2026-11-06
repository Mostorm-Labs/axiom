import test from 'node:test';
import assert from 'node:assert/strict';
import {summarizePhase} from '../runners/g5/navigation_metrics.mjs';

test('exact coverage metadata and replay counts are separate and missing counters stay unknown',()=>{
  const phase={samples:[],displayTimes:[],fences:[],longTasks:[]};
  const counts={compiled_contour_knots:32,record_candidates:7,record_replays:2,record_rejected:19,record_groups:3,record_selection_yields:1,checkpoint_replays:3,checkpoint_replay_knots:96,scalar_coverage_tiles:12};
  assert.deepEqual(summarizePhase(phase,[],[{stages:{},counts},{stages:{},counts}]).exactCoverageWork,
    {compiled_contour_knots:64,record_candidates:14,record_replays:4,record_rejected:38,record_groups:6,record_selection_yields:2,checkpoint_replays:6,checkpoint_replay_knots:192,scalar_coverage_tiles:24});
  assert.equal(summarizePhase(phase,[],[]).exactCoverageWork,null);
  assert.equal(summarizePhase(phase,[],[{stages:{},counts:{}}]).exactCoverageWork,null);
});

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

test('GPU admission deltas remain separate from CPU and unknown old counters', () => {
  const phase={samples:[],displayTimes:[],fences:[],longTasks:[]};
  const row={elapsed_ns:0,overlay_ns:0,gpu_contours:64,coarse_masks:2};
  const value=summarizePhase(phase,[row,{...row,gpu_contours:32,coarse_masks:1}],[]);
  assert.deepEqual(value.navigation.gpuAdmission,{contourPrimitives:96,coarseMasks:3});
  assert.equal(summarizePhase(phase,[{elapsed_ns:0,overlay_ns:0}],[]).navigation.gpuAdmission,null);
});

test('presented source quality uses gauges and observed time, including unrecovered debt', () => {
  const phase={samples:[],displayTimes:[],fences:[],longTasks:[]};
  const row=(ms,low,up,retained=0,presented=true)=>({elapsed_ns:0,overlay_ns:0,
    version:3,start_ns:String(BigInt(ms)*1000000n),presented,low_resolution_fraction:low,
    maximum_upscale:up,retained_fraction:retained,suspended:1,resumed:0});
  const value=summarizePhase(phase,[row(0,1,4),row(5,0,1,1,false),row(10,.2,2,.8),row(30,0,1,1),row(40,.5,3,.5)],[]).navigation.quality;
  assert.equal(value.presentations,4);
  assert.equal(value.maximumUpscale,4);
  assert.equal(value.lowResolutionAreaFraction.max,1);
  assert.ok(Math.abs(value.timeWeightedLowResolutionFraction-14/40)<1e-12);
  assert.equal(value.timeToAdequateMs.max,30);
  assert.equal(value.unrecoveredEpisodes,1);
  assert.equal(value.suspended,5);
});

test('old or incomplete source quality diagnostics remain unavailable', () => {
  const phase={samples:[],displayTimes:[],fences:[],longTasks:[]};
  for(const rows of [[{elapsed_ns:0,overlay_ns:0,presented:true}],
    [{elapsed_ns:0,overlay_ns:0,version:3,presented:false}]])
    assert.equal(summarizePhase(phase,rows,[]).navigation.quality,null);
});

test('shared-source gauges and query deltas stay distinct from historical missing rows',()=>{
  const phase={samples:[],displayTimes:[],fences:[],longTasks:[]};
  const row={version:4,elapsed_ns:0,overlay_ns:0,chunk_candidates:8,index_nodes:11,index_fallbacks:0,
    source_copy_bytes:0,source_aliases:2,source_queries_observed:1,source_bytes:4096,source_hits:3};
  const v=summarizePhase(phase,[row,{...row,index_nodes:15,source_bytes:2048,source_hits:4}],[]).navigation.sharedSources;
  assert.equal(v.chunkCandidates,16);assert.equal(v.indexNodes,26);assert.equal(v.payloadCopyBytes,0);
  assert.equal(v.peakRetainedBytes,4096);assert.equal(v.hits,7);
  assert.equal(summarizePhase(phase,[{version:3,elapsed_ns:0,overlay_ns:0}],[]).navigation.sharedSources,null);
});

test('coarse CPU mask encoding counts stay distinct from GPU work and old missing observations',()=>{
  const phase={samples:[],displayTimes:[],fences:[],longTasks:[]};
  const rows=[{elapsed_ns:0,overlay_ns:0,coarse_masks:3,alpha_masks:2},{elapsed_ns:0,overlay_ns:0,coarse_masks:2,alpha_masks:2}];
  assert.deepEqual(summarizePhase(phase,rows,[]).navigation.maskEncoding,{alphaStarts:4,n32Starts:1});
  assert.equal(summarizePhase(phase,[{elapsed_ns:0,overlay_ns:0,coarse_masks:2}],[]).navigation.maskEncoding,null);
  assert.equal(summarizePhase(phase,[{elapsed_ns:0,overlay_ns:0,coarse_masks:2,alpha_masks:3}],[]).navigation.maskEncoding,null);
});

test('navigation components and region compactions preserve old unknown diagnostics',()=>{
  const phase={samples:[],displayTimes:[],fences:[],longTasks:[]};
  const value=summarizePhase(phase,[
    {elapsed_ns:0,overlay_ns:0,partition_ns:1000000,contour_ns:3000000,material_ns:5000000,other_draw_ns:7000000,region_compactions:2},
    {elapsed_ns:0,overlay_ns:0,partition_ns:2000000,contour_ns:4000000,material_ns:6000000,other_draw_ns:8000000,region_compactions:3}
  ],[]).navigation;
  assert.deepEqual(value.componentsMs,{partition:3,contour:7,material:11,otherDraw:15});
  assert.equal(value.regionCompactions,5);
  const old=summarizePhase(phase,[{elapsed_ns:0,overlay_ns:0}],[]).navigation;
  assert.equal(old.componentsMs,null);assert.equal(old.regionCompactions,null);
});

test('v2 starts at real input and separates clear activity,content return and coarse receipt',()=>{
  const raw={started:0,inputEnded:100,ended:180,samples:[],displayTimes:[10,40,60,90,150],fences:[],longTasks:[],
    receipts:[{at:150},{at:170}],observationVersion:2,
    inputEvents:[{id:1,started:20,ended:21,type:'wheel'},{id:2,started:80,ended:81,type:'wheel'}],
    colorOps:[{kind:'clear',started:9,ended:10},{kind:'content',started:39,ended:40},
      {kind:'content',started:59,ended:60},{kind:'content',started:89,ended:90}],
    rafCallbacks:[{id:1,started:65,ended:75,timestamp:64}],nativeRecords:[]};
  const v=summarizePhase(raw,[],[]).feedbackV2;
  assert.ok(v,'missing v2 feedback observation');
  assert.equal(v.preInputWaitMs,20);assert.equal(v.firstContentResponseMs,20);
  assert.equal(v.lastInputContentResponseMs,10);assert.equal(v.lastInputToCoarseReceiptMs,70);
  assert.equal(v.lastInputToLastCoarseReceiptMs,90);
  assert.equal(v.legacyInputEndOffsetMs,20);assert.equal(v.contentGapMs.max,30);
  assert.equal(v.steadyContentGapMs.max,30);assert.equal(v.clearCalls,1);
  // Worst interval60..90 includes RAF65..75 and dispatch80..81.
  assert.equal(v.worstGap.observedCallbackCoveredMs,11);assert.equal(v.worstGap.outsideObservedCallbacksMs,19);
  assert.equal(v.cameraQualified,false);
  assert.equal(summarizePhase(raw,[],[]).settle_ms,50,'legacy receipt boundary changed');
});

test('v2 clear-only work cannot hide a censored content gap',()=>{
  const v=summarizePhase({started:0,inputEnded:120,ended:160,samples:[],displayTimes:[20,40,60,80,100,120],fences:[],longTasks:[],
    observationVersion:2,inputEvents:[{id:1,type:'wheel',started:10,ended:11},{id:2,type:'wheel',started:110,ended:111}],
    colorOps:[20,40,60,80,100,120].map(ended=>({kind:'clear',started:ended-1,ended})),rafCallbacks:[],nativeRecords:[]},[],[]).feedbackV2;
  assert.ok(v);assert.equal(v.status,'CENSORED');assert.equal(v.firstContentResponseMs,null);
  assert.equal(v.contentGapMs.max,null);assert.equal(v.censoredContentGapLowerBoundMs,150);
});

test('v2 legacy timestamps remain unavailable and input callback work is not scheduler delay',()=>{
  assert.equal(summarizePhase({samples:[],displayTimes:[],fences:[],longTasks:[]},[],[]).feedbackV2,null);
  const v=summarizePhase({started:0,inputEnded:60,ended:90,samples:[],displayTimes:[],fences:[],longTasks:[{start:20,duration:35}],
    observationVersion:2,inputEvents:[{id:1,type:'wheel',started:10,ended:50}],
    colorOps:[{kind:'content',started:50,ended:55}],rafCallbacks:[{id:1,started:30,ended:55,timestamp:29}],nativeRecords:[]},[],[]).feedbackV2;
  assert.ok(v);assert.equal(v.worstGap.durationMs,45);assert.equal(v.worstGap.observedCallbackCoveredMs,45);
  assert.equal(v.worstGap.outsideObservedCallbacksMs,0);assert.equal(v.worstGap.longTaskOverlapMs,35);
});

test('v2 accepted Runtime submits qualify empty-view feedback while retries and unknown logs do not',()=>{
  const phase={started:0,inputEnded:105,ended:160,observationEnded:160,samples:[],displayTimes:[],fences:[],longTasks:[],
    observationVersion:2,inputEvents:[{id:1,started:10,ended:11},{id:2,started:100,ended:101}],
    colorOps:[{kind:'clear',started:49,ended:50}],rafCallbacks:[],
    nativeRecords:[
      {at:20,text:'AXIOM_NAVIGATION_TIMING '+JSON.stringify({version:5,scene:'15',surface:'1',presented:true})},
      {at:30,text:'AXIOM_NAVIGATION_TIMING '+JSON.stringify({version:5,scene:'15',surface:'1',presented:false})},
      {at:45,text:'AXIOM_FRAME_TIMING '+JSON.stringify({version:1,outcome:'retry'})},
      {at:60,text:'AXIOM_NAVIGATION_TIMING '+JSON.stringify({version:5,scene:'15',surface:'1',presented:true})},
      {at:110,text:'AXIOM_FRAME_TIMING '+JSON.stringify({version:1,outcome:'accepted',view:'1',frame:'99',semantic:'3',scene:'15',camera:'80',surface:'1',metrics:'1'})}]};
  const v=summarizePhase(phase,[],[]).runtimeSubmitV2;
  assert.ok(v);assert.equal(v.status,'OBSERVED');assert.equal(v.acceptedCalls,3);
  assert.equal(v.firstAcceptedSubmitMs,10);assert.equal(v.lastInputAcceptedSubmitMs,10);
  assert.equal(v.acceptedGapMs.max,50);assert.equal(v.cameraQualified,false);
  assert.equal(summarizePhase({...phase,nativeRecords:[]},[],[]).runtimeSubmitV2.status,'UNAVAILABLE');
});

test('runtime submit identities reject numeric or overwide IDs rather than losing precision',()=>{
 const base={started:0,inputEnded:10,ended:20,samples:[],displayTimes:[],fences:[],longTasks:[],observationVersion:2,
 inputEvents:[{id:1,started:1,ended:2}],colorOps:[],rafCallbacks:[]};
 for(const frame of [42,'18446744073709551616']){
  const row={version:1,outcome:'accepted',view:'1',frame,semantic:'3',scene:'15',camera:'80',surface:'1',metrics:'1'};
  assert.equal(summarizePhase({...base,nativeRecords:[{at:12,text:'AXIOM_FRAME_TIMING '+JSON.stringify(row)}]},[],[]).runtimeSubmitV2.status,'UNAVAILABLE');
 }
});
