export const IDS=['UR2B1-FQ1','UR2B1-FN1','UR2B1-A1','UR2B1-B1','UR2B1-B2','UR2B1-A2','UR2B1-A3','UR2B1-B3'];
export const PERF_CASES=['S0-E-reerase','S2-E-reerase','S0-P-pan','S2-P-pan'];
export function distribution(values){
 const a=values.filter(Number.isFinite).sort((a,b)=>a-b);
 const q=p=>a.length?a[Math.min(a.length-1,Math.ceil(p*a.length)-1)]:null;
 return{n:a.length,p50:q(.5),p95:q(.95),p99:q(.99),max:a.at(-1)??null};
}
export function summarizeCase(row){
 const p=row.input??{},events=p.inputEvents??[],dur=type=>events.filter(e=>e.type===type).map(e=>e.ended-e.started);
 const pointers=p.schedule?.filter(e=>e.phase)??[],nextDown=[];
 for(let i=1;i<pointers.length;++i)if(pointers[i].phase==='down'&&pointers[i-1].phase==='up')nextDown.push(pointers[i].started-pointers[i-1].ended);
 const delivered=p.schedule?.filter(e=>Number.isFinite(e.planned)&&Number.isFinite(e.actual))??[];
 const content=(p.colorOps??[]).filter(e=>e.kind==='content');
 const wheels=events.filter(e=>e.type==='wheel'),start=wheels[0]?.started??p.started,end=wheels.at(-1)?.ended??p.inputEnded;
 const during=content.filter(c=>c.started>=start&&c.ended<=end);
 const gaps=during.slice(1).map((v,i)=>v.started-during[i].ended);
 const complete=row.final?.frames?.find(f=>f.at>=(p.afterUp?.at??p.inputEnded)&&f.state?.lastCommit?.presentedGeneration===p.after?.generation);
 const exactWait=complete?complete.at-(p.afterUp?.at??p.inputEnded):null;
 return {id:row.id,up:distribution(dur('pointerup')),nextDown:distribution(dur('pointerdown')),move:distribution(dur('pointermove')),
  up_to_next_down_gap:distribution(nextDown),delivery_lateness:distribution(delivered.map(v=>v.actual-v.planned)),
  navigation_content_gap:distribution(gaps),navigation_first_content_ms:during.length?during[0].started-start:null,
  exact_wait_ms:exactWait,settled_wait_ms:row.settled?.at-p.inputEnded,input_events:events.length,
  failures:row.failures??[],scope:'JS monotonic dispatch and observed default-FBO operations; no physical GPU/display duration'};
}
export function validatePerformance(result){
 const failures=[];
 if(result.profile!=='normal'||result.primary_readbacks!==0||result.primary_snapshots!==0||result.primary_trace!==false)failures.push('perturbed primary phase');
 if(result.cases?.length!==4||result.cases.some((c,i)=>c.id!==PERF_CASES[i]))failures.push('case roster');
 for(const c of result.cases??[]){if(c.input?.status!=='INPUT_COMPLETED'||c.final?.errors?.length||c.final?.state?.activeContacts!==0||c.failures?.length)failures.push(c.id+':input/error');
  if(c.settled?.state?.generation!==c.settled?.state?.lastCommit?.presentedGeneration)failures.push(c.id+':unqualified exact');}
 if(!result.artifacts_stable||!result.source_stable||result.page_errors?.length||result.crash)failures.push('identity/environment');
 return{status:failures.length?'FAILED':'OBSERVED',failures,summaries:(result.cases??[]).map(summarizeCase)};
}
export function qualifyExactConsumers(records,tags){
 const leaves=records.filter(r=>r.kind===10&&r.tag===tags.exact_body&&r.values?.[0]==='1');
 const reused=leaves.filter(r=>BigInt(r.values[6])>0n&&BigInt(r.values[5])>0n&&r.values[7]==='0');
 const costs=records.filter(r=>r.kind===10&&r.tag===tags.exact_body&&r.values?.[0]==='2');
 const byJob=new Set(records.filter(r=>r.tag===tags.exact_prepare).map(r=>r.job));
 const joined=reused.filter(r=>r.job&&r.job!=='0'&&byJob.has(r.job));
 return {status:joined.length?'OBSERVED':'MISSING',queries:leaves.length,reused:reused.length,joined:joined.length,
  fallbacks:leaves.filter(r=>r.values[7]!=='0').map(r=>({job:r.job,source:r.values[4],reason:r.values[7]})),cost_rows:costs.length};
}
export function classifyPaired(pairs){
 if(pairs.length!==3||pairs.some(p=>!p.baseline||!p.candidate))throw Error('three complete pairs required');
 const rows=pairs.map(p=>({baseline:p.baseline,candidate:p.candidate}));
 const all=pairs.every(p=>p.candidate<p.baseline);
 return{classification:all?'STRUCTURAL_GAIN_TIMING_IMPROVEMENT_OBSERVED':'STRUCTURAL_GAIN_TIMING_INCONCLUSIVE',rows,
  scope:'Observed paired values only; no SLO, Vibe parity or overall performance PASS'};
}
