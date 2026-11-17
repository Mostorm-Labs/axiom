// Verification-only values and accounting; existing frozen algorithms/input
// contracts remain the source. No document, stroke or navigation implementation.
export const mixedRows=Object.freeze(['append-zoom-out-immediate','partial-erase-zoom-out-immediate','partial-erase-pan-immediate']);
export const primaryMove=i=>{
 if(!Number.isInteger(i)||i<1||i>7200)throw Error('primary input index');
 return [Math.fround(420+7*Math.sin(i*.17)),Math.fround(160+170*i/7200)];
};
export const qualifiedCanonical=state=>!!state&&!state.activeContacts&&!state.pending?.intents?.length&&state.generation===state.lastCommit?.presentedGeneration;
export function eligibility(facts,workerRuns){
 if(facts?.error==='LEGACY_ROUTE_INELIGIBLE_DOCUMENT')return 'ACTUAL_LEGACY_ROUTE_INELIGIBLE_DOCUMENT';
 if(workerRuns>0)return 'ACTUAL_PRIVATE_WORKER_USED';
 if(facts?.capability===false||facts?.enabled===false)return 'DOCUMENT_INELIGIBLE_OR_NOT_ENABLED_NO_WORKER';
 return 'NO_WORKER_OBSERVED_REASON_UNPROVEN';
}
export function unionIntervals(intervals,lo=-Infinity,hi=Infinity){
 const values=intervals.map(([a,b])=>[Math.max(a,lo),Math.min(b,hi)]).filter(([a,b])=>Number.isFinite(a)&&Number.isFinite(b)&&b>a).sort((a,b)=>a[0]-b[0]);
 const merged=[];for(const [a,b]of values){const last=merged.at(-1);if(last&&a<=last[1])last[1]=Math.max(last[1],b);else merged.push([a,b]);}
 return merged;
}
export function partitionWindow(lo,hi,owner,nestedCanonical=[]){
 if(!Number.isFinite(lo)||!Number.isFinite(hi)||hi<lo)throw Error('window');
 const union=unionIntervals(owner,lo,hi),ownerMs=union.reduce((n,[a,b])=>n+b-a,0);
 return {elapsedMs:hi-lo,ownerCallUnionMs:ownerMs,outsideOwnerCallMs:hi-lo-ownerMs,
  nestedCanonicalMs:unionIntervals(nestedCanonical,lo,hi).reduce((n,[a,b])=>n+b-a,0),
  qualification:'Nested canonical is a breakdown only; never add it to ownerCallUnionMs. Outside owner calls is not necessarily idle CPU or GPU hardware wait.'};
}
export function assertRecordBatch(value,next){
 if(!value||value.sequence!==next||!Array.isArray(value.records)||value.records.length>32||Buffer.byteLength(JSON.stringify(value))>131072)throw Error('record sequence/bound');
 return {nextSequence:next+1,records:value.records.length};
}
