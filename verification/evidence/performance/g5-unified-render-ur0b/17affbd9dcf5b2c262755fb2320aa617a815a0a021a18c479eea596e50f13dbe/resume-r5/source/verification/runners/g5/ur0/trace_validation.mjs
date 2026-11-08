// Dynamic identity oracle. Independent of the producer's JSON shape validator.
const u64=x=>typeof x==='string'&&/^(0|[1-9][0-9]*)$/.test(x)&&BigInt(x)<=18446744073709551615n;
const identity=['runtime','document','lifetime','attachment','documentEpoch','resource','view','frame','surface','metrics','sceneGeneration','semanticGeneration','sceneRevision','camera','work','operation','commitRevision'];
const same=(a,b,fields)=>fields.every(k=>a[k]===b[k]);
const commitFields=['runtime','document','lifetime','attachment','documentEpoch','operation','commitRevision'];
const mainRoutes=new Set(['CAN-DIRECT','CAN-PREP-V4','EDIT-MASK-DIRECT','EDIT-MASK-ISOLATED','EDIT-ORDERED-RUN','EDIT-PLAIN','ERASE-COMMIT-SCENE','ERASE-FEEDBACK','ERASE-LAYERS','ERASE-MISS','ERASE-NEXT-DOWN','ERASE-RESIDENT','ERASE-UP-EXACT','NAV-COARSE-CPU','NAV-COARSE-GPU','NAV-RETAINED','NAV-UNPREPARED']);
export function validateTimeline(trace){
 const errors=[];const fail=reason=>errors.push(reason);
 const commits=trace.commits??[],scenes=trace.scenes??[],frames=trace.frames??[],completions=trace.completions??[];
 const rows=[...commits,...scenes,...frames,...completions];
 for(const row of rows){for(const k of identity){
  if(k==='document'||k==='operation'){if(typeof row[k]!=='string'||!/^[0-9a-f]{32}$/.test(row[k]))fail('invalid '+k);}
  else if(!u64(row[k]))fail('invalid/lossy '+k);
 }
 if(row.at_ns!==null&&row.at_ns!==undefined&&!u64(row.at_ns))fail('invalid time');
 }
 if(errors.length)return{valid:false,errors};
 for(const c of commits){
  if(!u64(c.beforeGeneration)||!u64(c.afterGeneration)||BigInt(c.afterGeneration)<=BigInt(c.beforeGeneration))fail('commit generation did not advance');
 }
 for(const s of scenes){
  if(s.state&&s.state!=='PUBLISHED')continue;
  const c=commits.find(c=>same(c,s,commitFields)&&c.afterGeneration===s.semanticGeneration);
  if(!c||s.sceneGeneration!==s.semanticGeneration)fail('Scene/commit generation mismatch');
  if(c?.at_ns&&s.at_ns&&BigInt(s.at_ns)<BigInt(c.at_ns))fail('Scene before commit');
 }
 for(const f of frames){
  if(f.purpose==='navigation')continue; // navigation has no canonical receipt
  const s=scenes.find(s=>(!s.state||s.state==='PUBLISHED')&&same(s,f,[...commitFields,'sceneGeneration','semanticGeneration','sceneRevision']));
  if(!s)fail('frame has no matching authoritative Scene basis');
  if(s?.at_ns&&f.at_ns&&BigInt(f.at_ns)<BigInt(s.at_ns))fail('frame before Scene');
 }
 for(const c of completions){
  if(c.purpose==='navigation'){fail('invented navigation canonical receipt');continue;}
  if(c.qualification!=='QUALIFIED')continue;
  const f=frames.find(f=>f.purpose==='canonical'&&same(f,c,identity));
  if(!f)fail('foreign/stale canonical completion');
  if(f?.at_ns&&c.at_ns&&BigInt(c.at_ns)<BigInt(f.at_ns))fail('completion before submission');
 }
 const decisionIds=new Set(),decisions=new Map();
 for(const d of trace.decisions??[]){
  if(decisionIds.has(d.id))fail('duplicate decision');decisionIds.add(d.id);decisions.set(d.id,d);
  if(d.origin!=='CURRENT_BRANCH')fail('unimplemented unified policy claim');
 }
 const catalog=trace.routeCatalog?new Set(trace.routeCatalog):mainRoutes;
 for(const leaf of trace.leaves??[]){
  const d=decisions.get(leaf.decision);
  if(!d||d.job!==leaf.job||d.basis!==leaf.basis)fail('foreign decision/leaf basis');
  if(!catalog.has(leaf.route))fail('unknown route');
  if(d?.outcome==='PreparedGpuDraw'&&leaf.target!=='GPU')fail('GPU request hid actual target');
  if(!['GPU','CPU','RECORDING','NONE'].includes(leaf.target))fail('unknown actual target');
 }
 if((trace.loss?.critical??0)>0||(trace.loss?.unknown??0)>0)fail('critical/route evidence missing');
 return{valid:errors.length===0,errors,canonicalCompletions:completions.filter(c=>c.qualification==='QUALIFIED').length};
}
