export const CONTINUOUS_CASES=['S0','S2'].flatMap(f=>[1,2,4].map(dpr=>({id:`${f}-continuous-DPR${dpr}`,fixture:f,dpr})));
// Defined from the frozen fixture viewport and planned swept eraser regions,
// never selected from a candidate-vs-reference difference image.
export const SENTINEL={left:40,top:40,right:300,bottom:460,aaGuard:16,ink:[13,26,51,255]};
export function requireTrajectoryCamera(actual,trace){
 if(!['scale','x','y'].every(k=>Number.isFinite(actual?.[k])&&actual[k]===trace.camera[k]))throw Error('frozen trajectory camera mismatch');
}
export function validateTrajectoryPlan(plan,name,snapshotSha,dpr){
 const fail=m=>{throw Error('trajectory '+m);},hash=v=>/^[a-f0-9]{64}$/.test(v??'');
 if(plan?.schema!=='g5-transient-positive-trajectories-v2'||JSON.stringify(plan.viewport_css)!=='[800,500]'||plan.radius_world!==16||plan.AA_guard_css!==16)fail('schema');
 if(!['left','top','right','bottom'].every(k=>plan.sentinel?.[k]===SENTINEL[k]))fail('sentinel definition');
 const f=plan.fixtures?.[name];if(!f||!hash(snapshotSha)||f.snapshot_sha256!==snapshotSha)fail('fixture identity');
 if(![1,2,4].includes(dpr)||JSON.stringify(f.DPR)!=='[1,2,4]')fail('DPR');
 if(f.expected_commits!==8||f.expected_operations_with_undo_redo!==10||f.cancel_does_not_append_history!==true||f.traces?.length!==8)fail('operation closure');
 for(const [i,t]of f.traces.entries()){
  if(t.index!==i||t.radius_world!==16||!Number.isFinite(t.camera?.scale)||t.camera.scale<=0||!Number.isFinite(t.camera.x)||!Number.isFinite(t.camera.y)||t.view_points?.length!==9||t.world_points?.length!==9)fail('trace shape');
  if(!t.expected_target_ids?.length||new Set(t.expected_target_ids).size!==t.expected_target_ids.length||t.expected_target_ids.some(x=>!(/^[a-f0-9]{32}$/.test(x))))fail('target identity');
  for(const d of [1,2,4])for(const key of ['input.json','derived.json','request.json'])if(!hash(t.proof?.[d]?.[key]?.sha256))fail('proof missing');
  if(!hash(t.proof[1]['decisions.json']?.sha256))fail('proof decision');
  for(const [j,p]of t.view_points.entries()){
   if(p.length!==2||p.some(v=>!Number.isFinite(v))||p[0]<0||p[0]>800||p[1]<0||p[1]>500)fail('view point');
   if(p[0]-16*t.camera.scale-plan.AA_guard_css<=SENTINEL.right)fail('sentinel overlap');
   const mapped=[Math.fround(Math.fround(Math.fround(p[0])-Math.fround(t.camera.x))/Math.fround(t.camera.scale)),Math.fround(Math.fround(Math.fround(p[1])-Math.fround(t.camera.y))/Math.fround(t.camera.scale))];
   if(!mapped.every((v,k)=>v===t.world_points[j][k]))fail('mapping');
  }
 }
 return f;
}
export function qualifySentinel(before,after){
 if(before.length!==after.length||before.length%4)throw Error('sentinel buffer shape');
 let interior=0,lost=0;
 for(let i=0;i<before.length;i+=4){if(!SENTINEL.ink.every((v,c)=>Math.abs(before[i+c]-v)<=2))continue;++interior;if(!SENTINEL.ink.every((_,c)=>Math.abs(after[i+c]-before[i+c])<=2))++lost;}
 return {status:interior>=100&&lost===0?'PRESERVED':'FAILED',interior,lost,compared_bytes:before.length,selection:'fixture-defined opaque ink interior in fixed unchanged ROI; no candidate difference selection',reference_rule:'fixed-validated-before-rgba-pairwise2-v1'};
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
