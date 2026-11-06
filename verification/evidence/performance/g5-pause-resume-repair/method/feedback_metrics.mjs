import {statistics} from './browser_metrics.mjs';
const overlap=(a,b,c,d)=>Math.max(0,Math.min(b,d)-Math.max(a,c));
function covered(intervals,start,end){
  const clipped=intervals.map(r=>[Math.max(start,r.started),Math.min(end,r.ended)]).filter(([a,b])=>b>a).sort((a,b)=>a[0]-b[0]);
  let total=0,last=start;for(const [a,b]of clipped){total+=Math.max(0,b-Math.max(a,last));last=Math.max(last,b);}return total;
}
export function summarizeInputFeedback(phase){
  if(phase.observationVersion!==2)return null;
  const events=phase.inputEvents,ops=phase.colorOps,rafs=phase.rafCallbacks;
  if(!Array.isArray(events)||!Array.isArray(ops)||!Array.isArray(rafs)||
    [...events,...ops,...rafs].some(r=>!Number.isFinite(r.started)||!Number.isFinite(r.ended)||r.ended<r.started))throw Error('invalid feedback ledger');
  if(events.some((e,i)=>i&&e.started<events[i-1].started))throw Error('unordered input ledger');
  if(!events.length)return {version:2,status:'NO_INPUT',cameraQualified:false,eventCount:0};
  const first=events[0].started,last=events.at(-1).started;
  const content=ops.filter(o=>o.kind==='content'&&o.started>=first).sort((a,b)=>a.ended-b.ended);
  const firstResponse=content[0],lastResponse=content.find(o=>o.started>=last);
  const stop=lastResponse?.ended??phase.observationEnded??phase.ended??phase.inputEnded;
  const shown=content.filter(o=>o.ended<=stop),gaps=[];let previous=first;
  for(const op of shown){gaps.push({started:previous,ended:op.ended,durationMs:op.ended-previous});previous=op.ended;}
  const worst=gaps.length?gaps.reduce((a,b)=>a.durationMs>=b.durationMs?a:b):null;
  let worstGap=null;
  if(worst){const {started,ended,durationMs}=worst;
    const callbacks=rafs.filter(r=>overlap(started,ended,r.started,r.ended)>0),dispatches=events.filter(r=>overlap(started,ended,r.started,r.ended)>0);
    const observedCallbackCoveredMs=covered([...callbacks,...dispatches],started,ended);
    const longTasks=(phase.longTasks??[]).map(r=>({started:r.start,ended:r.start+r.duration}));
    const native=(phase.nativeRecords??[]).filter(r=>callbacks.some(c=>c.id===r.rafId)||dispatches.some(e=>e.id===r.inputId));
    worstGap={...worst,observedCallbackCoveredMs,outsideObservedCallbacksMs:Math.max(0,durationMs-observedCallbackCoveredMs),
      rafIds:callbacks.map(r=>r.id),inputIds:dispatches.map(e=>e.id),longTaskOverlapMs:covered(longTasks,started,ended),
      enclosingNativeRecords:native.length};
  }
  const receipts=(phase.receipts??[]).map(r=>r.at).filter(t=>Number.isFinite(t)&&t>=last);
  const firstReceipt=receipts.length?Math.min(...receipts):null;
  return {version:2,status:lastResponse?'OBSERVED':'CENSORED',cameraQualified:false,eventCount:events.length,
    firstInputAt:first,lastInputAt:last,lastInputEndedAt:events.at(-1).ended,preInputWaitMs:first-phase.started,
    inputDispatchSpanMs:events.at(-1).ended-first,legacyInputEndOffsetMs:phase.inputEnded-last,
    firstContentResponseMs:firstResponse?firstResponse.ended-first:null,
    lastInputContentResponseMs:lastResponse?lastResponse.ended-last:null,
    lastInputToCoarseReceiptMs:firstReceipt===null?null:firstReceipt-last,
    lastInputToLastCoarseReceiptMs:receipts.length?Math.max(...receipts)-last:null,
    contentCalls:content.length,clearCalls:ops.filter(o=>o.kind==='clear').length,maskedCalls:ops.filter(o=>o.kind==='masked').length,
    contentGapMs:statistics(gaps.map(g=>g.durationMs)),steadyContentGapMs:statistics(gaps.slice(1).map(g=>g.durationMs)),
    censoredContentGapLowerBoundMs:lastResponse?null:Math.max(0,stop-previous),
    rafCallbackMs:statistics(rafs.map(r=>r.ended-r.started)),worstGap};
}

// One block,not one pointer sample,is the inferential unit. ABBA cancels
// linear position drift;it cannot eliminate nonlinear environmental effects.
export function counterbalancedEffect(values){
  if(!Array.isArray(values)||values.length!==4||values.some(v=>!Number.isFinite(v)))throw Error('invalid ABBA block');
  const baseline=(values[0]+values[3])/2,candidate=(values[1]+values[2])/2;
  return {baseline,candidate,effect:candidate-baseline};
}
function signTail(wins,n){
  let coefficient=1n,sum=0n;for(let k=0;k<=n;++k){if(k>=wins)sum+=coefficient;coefficient=coefficient*BigInt(n-k)/BigInt(k+1);}return Number(sum)/2**n;
}
// Effects must already be centered on the prespecified endpoint margin.
// Conditional on independent blocks/common median;never a Gate verdict.
export function compareBlockEffects(effects,{minimumBlocks=8,familyTests=8,alpha=.05}={}){
  if(!Array.isArray(effects)||effects.length>64||effects.some(v=>!Number.isFinite(v))||
    !Number.isSafeInteger(minimumBlocks)||minimumBlocks<1||!Number.isSafeInteger(familyTests)||familyTests<1||!(alpha>0&&alpha<.5))throw Error('invalid block inference');
  const n=effects.length,nonregressionP=signTail(effects.filter(v=>v<0).length,n),regressionP=signTail(effects.filter(v=>v>0).length,n);
  const threshold=alpha/familyTests;
  return {blocks:n,nonregressionP,regressionP,threshold,
    status:n<minimumBlocks?'INCONCLUSIVE':nonregressionP<=threshold?'NONREGRESSION_SUPPORTED':regressionP<=threshold?'REGRESSION_DETECTED':'INCONCLUSIVE'};
}

// Browser-side console interception timestamps the actual return-side diagnostic
// write. No C++/JS clock-origin assumption or private SDK access is required.
export function summarizeRuntimeSubmits(phase){
  if(phase.observationVersion!==2)return null;
  const stamps=[],accepted=[];let recognized=0;
  for(const record of phase.nativeRecords??[]){
    let row,kind;try{
      if(record.text.startsWith('AXIOM_NAVIGATION_TIMING ')){row=JSON.parse(record.text.slice(24));kind='navigation';}
      else if(record.text.startsWith('AXIOM_FRAME_TIMING ')){row=JSON.parse(record.text.slice(19));kind='canonical';}
      else continue;
    }catch{continue;}
    const ids=kind==='navigation'?['scene','surface']:['view','frame','semantic','scene','camera','surface','metrics'];
    if(!ids.every(k=>typeof row[k]==='string'&&/^\d{1,20}$/.test(row[k])&&BigInt(row[k])<=18446744073709551615n)||!Number.isFinite(record.at))continue;
    if(kind==='navigation'?row.version!==5:row.version!==1)continue;
    ++recognized;
    if(kind==='navigation'?row.presented!==true:row.outcome!=='accepted')continue;
    accepted.push({kind:'content',started:record.at,ended:record.at,rafId:record.rafId,inputId:record.inputId});
    stamps.push({kind,...Object.fromEntries(ids.map(k=>[k,row[k]]))});
  }
  const v=summarizeInputFeedback({...phase,colorOps:accepted});
  return {version:2,status:recognized?v.status:'UNAVAILABLE',cameraQualified:false,source:'Runtime local accepted submission diagnostic',
    acceptedCalls:accepted.length,recognizedRecords:recognized,observedIdentities:stamps,
    firstAcceptedSubmitMs:v.firstContentResponseMs??null,lastInputAcceptedSubmitMs:v.lastInputContentResponseMs??null,
    acceptedGapMs:v.contentGapMs??statistics([]),steadyAcceptedGapMs:v.steadyContentGapMs??statistics([]),
    censoredGapLowerBoundMs:v.censoredContentGapLowerBoundMs??null,worstGap:v.worstGap??null};
}
