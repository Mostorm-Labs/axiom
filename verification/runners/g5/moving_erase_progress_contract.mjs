export const POLICY='g5-moving-erase-progress-v1';
export const IDS=['UR2CMP1-R1','UR2CMP1-Q1','UR2CMP1-N1','UR2CMP1-N2'];
export const LIMITS=[120,240,360,600];
export function point(kind,g,i){return [145+48*g+10*Math.sin(.15*i),(kind==='known'?285+15*g:355+12*g)+(kind==='known'?.8:.35)*i];}
export function gestures(){return ['known','sustained'].flatMap(kind=>Array.from({length:3},(_,g)=>({kind,g,moves:kind==='known'?60:180,gap:kind==='known'?1000:0})));}
export function summarize(values){const a=[...values].sort((a,b)=>a-b);return {count:a.length,p50:a.length?a[Math.floor((a.length-1)*.5)]:null,p95:a.length?a[Math.floor((a.length-1)*.95)]:null,max:a.length?a.at(-1):null};}
export function qualifyTiming(row){
 const cases=row.gestures.map(g=>{
  const calls=row.observation.colorOps.filter(x=>x.kind==='content'&&x.started>=g.down&&x.ended<=g.up);
  const groups=new Map();for(const c of calls){const key=c.rafId!==null?'raf-'+c.rafId:c.inputId!==null?'input-'+c.inputId:'call-'+c.started;groups.set(key,Math.max(groups.get(key)??0,c.ended));}
  const at=[...groups.values()].sort((a,b)=>a-b),intervals=[];let prior=g.down;
  for(const t of at){intervals.push(t-prior);prior=t;}intervals.push(g.up-prior);
  const first=at.length?at[0]-g.down:g.up-g.down,stats=summarize(intervals);
  return {kind:g.kind,g:g.g,first_ms:first,intervals_ms:stats,publications:at.length,status:at.length&&first<=150&&stats.max<=150?'QUALIFIED':'FAILED'};
 });
 return {policy:POLICY,status:cases.length===6&&cases.every(x=>x.status==='QUALIFIED')?'QUALIFIED':'FAILED',cases,
  input_dispatch_ms:summarize(row.events.filter(x=>x.phase==='move').map(x=>x.ended-x.started)),
  input_lateness_ms:summarize(row.events.filter(x=>x.phase==='move').map(x=>Math.max(0,x.started-x.planned))),
  SDK_callback_ms:summarize(row.observation.rafCallbacks.filter(x=>x.source==='sdk').map(x=>x.ended-x.started)),
  evidence_kind:'normal actual target content publication opportunity proxy; requires independent prefix and active pixel conjunction'};
}
export function qualifyFreshness(rows){
 if(!rows.length)return {status:'FAILED',reason:'no consumed active prefixes'};
 let prior=-1;for(const r of rows){if(!r.accepted||!r.sameBasis||r.prefix<prior||r.prefix>r.inputPrefix||r.inputPrefix-r.prefix>9||r.at>=r.up)return {status:'FAILED',reason:'stale/foreign/nonmoving prefix'};prior=r.prefix;}
 return {status:'QUALIFIED'};
}
export function qualifyJoint({timing,freshness,activePixels}){
 return {status:timing.status==='QUALIFIED'&&freshness.status==='QUALIFIED'&&activePixels.length&&activePixels.every(x=>x.status==='QUALIFIED'&&x.positive&&x.beforeUp)?'QUALIFIED':'FAILED'};
}
