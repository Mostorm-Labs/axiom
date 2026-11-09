export const CONTINUOUS_CASES=['S0','S2'].flatMap(f=>[1,2,4].map(dpr=>({id:`${f}-continuous-DPR${dpr}`,fixture:f,dpr})));
// Defined from the frozen fixture viewport and planned swept eraser regions,
// never selected from a candidate-vs-reference difference image.
export const SENTINEL={left:40,top:40,right:300,bottom:460,aaGuard:16,ink:[13,26,51,255]};
export const ERASE_CENTERS=[[465,180],[520,205],[580,220],[635,260],[690,295],[735,330],[480,330],[600,340]];
export function qualifySentinel(before,after){
 if(before.length!==after.length||before.length%4)throw Error('sentinel buffer shape');
 let interior=0,lost=0;
 for(let i=0;i<before.length;i+=4){if(!SENTINEL.ink.every((v,c)=>Math.abs(before[i+c]-v)<=2))continue;++interior;if(!SENTINEL.ink.every((v,c)=>Math.abs(after[i+c]-v)<=2))++lost;}
 return {status:interior>=100&&lost===0?'PRESERVED':'FAILED',interior,lost,compared_bytes:before.length,selection:'fixture-defined opaque ink interior in fixed unchanged ROI; no candidate difference selection'};
}
export function qualifyContinuous(row){
 const failures=[];if(row.errors?.length)failures.push('SDK errors');
 if(row.final?.state.activeContacts!==0)failures.push('active contact leak');
 if(row.final?.state.objects!==16||row.final?.state.generation!==row.final?.state.lastCommit?.presentedGeneration||!row.final?.state.lastCommit?.operationId)failures.push('stable complete display receipt');
 if(BigInt(row.afterInput?.generation??0)!==BigInt(row.initial?.generation??0)+8n)failures.push('exactly eight non-noop commits');
 if(row.operations?.length!==10)failures.push('eight commits plus Undo/Redo operations');
 if(!row.schedule?.some(e=>e.same_dom_task_after_up))failures.push('missing immediate next Down');
 if(row.live?.length<24||row.live.some(r=>r.qualifier.status!=='PRESERVED'))failures.push('live target continuity');
 if(row.cold?.length!==4||!['before','after','undo','redo'].every(stage=>row.cold.some(c=>c.stage===stage&&c.oracle?.status==='QUALIFIED')))failures.push('original cold oracle');
 if(row.roundtrip?.status!=='QUALIFIED')failures.push('after/redo');
 return {status:failures.length?'FAILED':'OBSERVED',failures};
}
