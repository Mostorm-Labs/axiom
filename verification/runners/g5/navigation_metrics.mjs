import {statistics} from './browser_metrics.mjs';
const total=values=>values.reduce((a,b)=>a+b,0);
const times=values=>({...statistics(values),total:total(values)});
// A coarse SDK completion after input, not proof of that camera pose: the
// public onFrame receipt deliberately does not expose a camera/work identity.
export function postInputReceiptWait(receipts, inputEnded) {
  const times=receipts.map(r=>r.at).filter(t=>Number.isFinite(t)&&t>=inputEnded);
  return times.length?Math.min(...times)-inputEnded:null;
}
// Source texel density, not an optical metric or physical display latency.
// Only actual presented planes qualify; failed/unpresented calls may carry old
// gauges. Time integration stops at the last observed presentation, never at an
// invented recovery or at the end of an unrelated canonical frame.
export function summarizeQuality(rows) {
  const shown=rows.filter(r=>r.presented===true);
  if(!shown.length||shown.some(r=>r.version<3||!/^\d+$/.test(r.start_ns??'')||
    !['maximum_upscale','low_resolution_fraction','retained_fraction'].every(k=>Number.isFinite(r[k]))))return null;
  const origin=BigInt(shown[0].start_ns);
  const times=shown.map(r=>Number(BigInt(r.start_ns)-origin)/1e6);
  if(times.some((t,i)=>i&&t<times[i-1]))return null;
  let weighted=0,began=null;const recovery=[];
  shown.forEach((r,i)=>{
    if(i)weighted+=shown[i-1].low_resolution_fraction*(times[i]-times[i-1]);
    if(r.low_resolution_fraction>0){if(began===null)began=times[i];}
    else if(began!==null){recovery.push(times[i]-began);began=null;}
  });
  const duration=times.at(-1);
  return {thresholdDevicePixelsPerTexel:1.25,presentations:shown.length,
    maximumUpscale:Math.max(...shown.map(r=>r.maximum_upscale)),
    lowResolutionAreaFraction:statistics(shown.map(r=>r.low_resolution_fraction)),
    retainedAreaFraction:statistics(shown.map(r=>r.retained_fraction)),
    observedMilliseconds:duration,timeWeightedLowResolutionFraction:duration>0?weighted/duration:null,
    timeToAdequateMs:statistics(recovery),unrecoveredEpisodes:began===null?0:1,
    rightCensoredMilliseconds:began===null?null:duration-began,
    suspended:total(rows.map(r=>r.suspended??0)),resumed:total(rows.map(r=>r.resumed??0))};
}
export function summarizePhase(phase, navigationRows, frameRows) {
  const gaps=phase.displayTimes.slice(1).map((t,i)=>t-phase.displayTimes[i]);
  const inputTimes=phase.displayTimes.filter(t=>t>=phase.started&&t<=phase.inputEnded);
  const bounded=Number.isFinite(phase.started)&&Number.isFinite(phase.inputEnded)?[phase.started,...inputTimes,phase.inputEnded]:[];
  const nav=navigationRows.length ? Object.fromEntries(
    ['queries','built','knots','prefix_knots','fallback','cancelled','discarded_knots','throttled','prewarm']
      .map(key=>[key,total(navigationRows.map(row=>row[key]??0))])) : null;
  if(nav){
    nav.calls=navigationRows.length;
    nav.sharedSources=navigationRows.every(r=>r.version>=4&&Number.isFinite(r.source_bytes))?{
      chunkCandidates:total(navigationRows.map(r=>r.chunk_candidates)),indexNodes:total(navigationRows.map(r=>r.index_nodes)),
      fallbacks:total(navigationRows.map(r=>r.index_fallbacks)),payloadCopyBytes:total(navigationRows.map(r=>r.source_copy_bytes)),
      aliases:total(navigationRows.map(r=>r.source_aliases)),queriesObserved:total(navigationRows.map(r=>r.source_queries_observed)),
      peakRetainedBytes:Math.max(...navigationRows.map(r=>r.source_bytes)),hits:total(navigationRows.map(r=>r.source_hits))}:null;
    nav.quality=summarizeQuality(navigationRows);
    nav.cpu_ms=times(navigationRows.map(row=>row.elapsed_ns/1e6));
    nav.query_cpu_ms=times(navigationRows.map(row=>row.query_ns/1e6).filter(Number.isFinite));
    nav.overlay_cpu_ms=times(navigationRows.map(row=>row.overlay_ns/1e6));
    nav.initialization_cpu_ms=times(navigationRows.map(row=>row.initialization_ns/1e6).filter(Number.isFinite));
    nav.gpuAdmission=navigationRows.every(row=>Number.isFinite(row.gpu_contours)&&Number.isFinite(row.coarse_masks))?{
      contourPrimitives:total(navigationRows.map(r=>r.gpu_contours)),
      coarseMasks:total(navigationRows.map(r=>r.coarse_masks))
    }:null;
    nav.prepared=navigationRows.every(row=>Number.isFinite(row.prepared_builds))?{
      builds:total(navigationRows.map(r=>r.prepared_builds)),
      hits:total(navigationRows.map(r=>r.prepared_hits)),
      denied:total(navigationRows.map(r=>r.prepared_denied)),
      peakResidentBytes:Math.max(...navigationRows.map(r=>r.prepared_bytes)),
      contourTested:total(navigationRows.map(r=>r.contour_tested)),
      contourDrawn:total(navigationRows.map(r=>r.contour_drawn)),
      vectorUses:total(navigationRows.map(r=>r.vector_prepared))
    }:null;
  }
  const stageTotals={};
  for(const row of frameRows)for(const [name,stage] of Object.entries(row.stages)){
    const t=stageTotals[name]??={inclusive_ms:0,exclusive_ms:0,calls:0};
    t.inclusive_ms+=stage.inclusive_ns/1e6;t.exclusive_ms+=stage.exclusive_ns/1e6;t.calls+=stage.calls;
  }
  const exactKeys=['compiled_contour_knots','record_candidates','record_replays','record_rejected','record_groups','record_selection_yields','checkpoint_replays','checkpoint_replay_knots'];
  const exactCoverageWork=frameRows.length&&frameRows.every(r=>exactKeys.every(k=>Number.isSafeInteger(r.counts?.[k])&&r.counts[k]>=0))
    ?Object.fromEntries(exactKeys.map(k=>[k,total(frameRows.map(r=>r.counts[k]))])):null;
  return {
    raf_ms:statistics(phase.samples.map(row=>row.interval)),
    display_submission_gap_ms:statistics(gaps),
    input_display_submission_gap_ms:statistics(bounded.slice(1).map((t,i)=>t-bounded[i])),
    updatedInputs:phase.samples.filter(row=>row.draws>0).length,
    inputCount:phase.samples.length,
    input_dispatch_ms:statistics(phase.samples.map(row=>row.dispatchMs).filter(Number.isFinite)),
    canonicalReceiptsDuringInput:phase.framesDuringInput??null,
    settle_ms:phase.receipts?postInputReceiptWait(phase.receipts,phase.inputEnded):phase.settleMilliseconds??null,
    navigation:nav, exactAttempts:frameRows.length, exactStages:stageTotals,exactCoverageWork,
    client_wait_cpu_ms:times(phase.fences.filter(row=>row.name==='clientWaitSync').map(row=>row.duration)),
    maximum_fence_timeout:Math.max(0,...phase.fences.map(row=>row.timeout??0)),
    fenceCalls:phase.fences.filter(row=>row.name==='fenceSync').length,
    longTasks:phase.longTasks,
    gpu_execution_ms:{value:null,reason:'CPU GL calls and polling do not measure GPU execution.'},
    input_to_photon_ms:{value:null,reason:'Synthetic input and default-framebuffer draw calls do not observe physical display.'}
  };
}
