// Numeric record decoder. Every join cites actual record sequence numbers.
// Unknown/absent fields stay absent; no placeholder zero or expected-stamp fill.
const hexWord=value=>{let n=BigInt(value),s='';for(let i=0;i<8;++i){s+=(n&255n).toString(16).padStart(2,'0');n>>=8n;}return s;};
const id=(a,b)=>hexWord(a)+hexWord(b);
const routeNames=['NONE','CAN-PREP-V4','CAN-DIRECT','EDIT-PLAIN','EDIT-MASK-ISOLATED','EDIT-MASK-DIRECT','ERASE-LAYERS','ERASE-RESIDENT','ERASE-MISS','NAV-COARSE-GPU','NAV-COARSE-CPU','NAV-UNPREPARED','NAV-RETAINED','PREVIEW-PREFIX','PREVIEW-TAIL','CREATION-CANONICAL','CREATION-TRANSIENT','RECORDING'];
const targetNames=['NONE','CPU','GPU','RECORDING'];
export function expandLeaseCycles(records){
 if(!records.some(r=>r.kind===10&&r.tag===43&&r.values?.[0]==='6'))return records;
 const expanded=[];
 for(const r of records){
  const v=r.values;
  if(r.kind===10&&r.tag===43&&v?.[0]==='6'){
   if(v.length!==16||v[3]!=='0'||v[6]!=='0'||[8,9,10,13,14,15].some(i=>!/^\d+$/.test(v[i]))||BigInt(v[8])>BigInt(v[9])||BigInt(v[9])>BigInt(v[10])||BigInt(v[13])>=BigInt(v[14])||BigInt(v[14])>=BigInt(v[15]))throw Error('invalid compact lease lifecycle');
   for(let i=0;i<3;++i)expanded.push({...r,seq:v[13+i],at_ns:v[8+i],values:[String(i+1),...v.slice(1,7),v[[7,11,12][i]]],compressed_source_seq:r.seq});
  }else expanded.push(r);
 }
 expanded.sort((a,b)=>BigInt(a.seq)<BigInt(b.seq)?-1:BigInt(a.seq)>BigInt(b.seq)?1:0);
 for(let i=1;i<expanded.length;++i)if(expanded[i-1].seq===expanded[i].seq)throw Error('duplicate expanded lease sequence');
 return expanded;
}
export function decodeTrace(records,loss){
 const out={schema:'g5-ur0b-decoded-v1',errors:[],missing:[],commits:[],scenes:[],frames:[],qualified_completions:[],unqualified_completions:[],decisions:[],leaves:[],spans:[],gpu:[],counters:[],loss};
 const fail=(reason,r)=>out.errors.push({reason,seq:r?.seq??null});
 if(loss?.critical)fail('critical record loss');
 if(records.some(r=>!Array.isArray(r.values)||r.values.some(v=>typeof v!=='string'||!/^\d+$/.test(v)))){fail('malformed lossless record values');return out;}
 try{records=expandLeaseCycles(records);}catch(e){fail(String(e));return out;}
 const begins=new Map(),groups=new Map(),owners=new Map(),renderFrames=[],providerSubmits=[],workBasis=new Map(),work=new Map(),completedWork=new Set(),commits=[],scenes=[];
 const ownerFields=['runtime','lifetime','attachment','document','documentEpoch','resource','view'];
 const ownerKey=o=>ownerFields.map(k=>o?.[k]??'?').join('/');
 const sameOwner=(a,b)=>!!a&&!!b&&ownerFields.every(k=>a[k]!==undefined&&a[k]===b[k]);
 const observedBefore=(a,b)=>typeof a==='string'&&typeof b==='string'&&/^\d+$/.test(a)&&/^\d+$/.test(b)&&BigInt(a)<=BigInt(b);
 const contextKey=r=>r.job+'/'+r.basis;
 const ownerFor=r=>{
  const found=owners.get(contextKey(r));
  if(!found||found.size!==1){out.missing.push({reason:found?'ambiguous actual owner for job/basis':'actual owner for job/basis unavailable',seq:r.seq});return null;}
  return found.values().next().value;
 };
 for(const r of records){const v=r.values;
  if(r.kind===5&&r.tag===1&&v?.length===8){
   const o={runtime:v[0],lifetime:v[1],attachment:v[2],document:id(v[3],v[4]),documentEpoch:v[5],resource:v[6],view:v[7],owner_seq:r.seq};
   const key=contextKey(r),set=owners.get(key)??new Map();set.set(ownerKey(o),o);owners.set(key,set);
  }
 }
 for(const r of records){const v=r.values;
  if(r.kind===6&&r.tag===27){
   const owner=ownerFor(r);
   if(!owner){out.missing.push({reason:'commit owner unavailable',seq:r.seq});continue;}
   const c={...owner,document:id(v[0],v[1]),operation:id(v[2],v[3]),beforeGeneration:v[4],afterGeneration:v[5],documentEpoch:v[6],commitRevision:v[7],at_ns:r.at_ns,event:r.event,job:r.job,provenance:[owner.owner_seq,r.seq]};
   if(c.document!==owner.document||c.documentEpoch!==owner.documentEpoch){fail('foreign commit document/epoch',r);continue;}commits.push(c);
  }
 }
 for(const [recordIndex,r] of records.entries()){
  const v=r.values;
  if(!Array.isArray(v)||v.some(n=>typeof n!=='string'||!/^[0-9]+$/.test(n))){fail('invalid lossless raw values',r);continue;}
  if(r.kind===2)begins.set(v[0],{id:v[0],parent:v[2]??null,tag:r.tag,job:r.job,start_ns:v[3]??r.at_ns,end_ns:null,inclusive_ns:null,exclusive_ns:null,seq:r.seq});
  if(r.kind===3){const s=begins.get(v[0]);if(!s)out.missing.push({reason:'span begin missing',seq:r.seq});else{s.end_ns=v[1]??r.at_ns;s.inclusive_ns=String(BigInt(s.end_ns)-BigInt(s.start_ns));s.exclusive_ns=v[2]??null;}}
  if(r.kind===10&&[17,22].includes(r.tag)&&v[0]==='9')begins.set(v[1],{id:v[1],parent:v[2],tag:r.tag,job:r.job,start_ns:null,end_ns:null,inclusive_ns:v[4],exclusive_ns:v[5],invocations:v[3],clock_domain:'wasm_local_duration',seq:r.seq});
  if(r.kind===7&&r.tag===9){
   const owner=ownerFor(r),candidates=commits.filter(c=>sameOwner(c,owner)&&c.afterGeneration===v[0]);
   const unique=new Map(candidates.map(c=>[c.operation+'/'+c.commitRevision,c]));
   if(v[0]!==v[1]){fail('published Scene generation disagrees with commit',r);continue;}
   if(unique.size!==1){out.missing.push({reason:unique.size?'ambiguous Scene commit owner/basis':'Scene commit outside observed owner/window',seq:r.seq});continue;}
   const commit=unique.values().next().value;
   scenes.push({...commit,semanticGeneration:v[0],sceneGeneration:v[1],sceneRevision:v[2],at_ns:r.at_ns,commit_observed_after_publication:BigInt(commit.at_ns)>BigInt(r.at_ns),provenance:[...commit.provenance,r.seq]});
  }
  if(r.kind===5&&r.tag===26&&v.length===10)renderFrames.push({...ownerFor(r),purpose:'canonical',view:v[0],frame:v[1],sceneGeneration:v[2],sceneRevision:v[3],camera:v[4],surface:v[5],metrics:v[6],gpu_resource_epoch:v[7],job:r.job,basis:r.basis,at_ns:r.at_ns,seq:r.seq,record_index:recordIndex});
  if(r.kind===5&&r.tag===11&&v.length===8)renderFrames.push({...ownerFor(r),purpose:'edit_feedback',view:v[0],frame:v[1],sceneGeneration:v[2],sceneRevision:v[3],camera:v[4],surface:v[5],metrics:v[6],gpu_resource_epoch:v[7],job:r.job,basis:r.basis,at_ns:r.at_ns,seq:r.seq,record_index:recordIndex});
  if(r.kind===11&&r.tag===19)providerSubmits.push({...ownerFor(r),view:v[0],frame:v[1],sceneGeneration:v[2],sceneRevision:v[3],surface:v[4],metrics:v[5],code:v[6],job:r.job,basis:r.basis,at_ns:r.at_ns,seq:r.seq,record_index:recordIndex});
  if(r.kind===5&&r.tag===26&&v.length===11)workBasis.set(contextKey(r)+'/'+v[0],{runtime:v[1],lifetime:v[2],attachment:v[3],document:id(v[4],v[5]),documentEpoch:v[6],commitRevision:v[7],resource:v[8],sceneRevision:v[9],camera:v[10],seq:r.seq});
  if(r.kind===11&&r.tag===29){
   const capturedOwner=workBasis.get(contextKey(r)+'/'+v[0]),actualOwner=ownerFor(r);
   const owner=capturedOwner?{...capturedOwner,view:v[1]}:null;
   if(!owner||!actualOwner){out.missing.push({reason:'work capture has no full observed owner',seq:r.seq});continue;}
   if(!sameOwner(owner,actualOwner)){fail('work capture owner differs from actual caller',r);continue;}
   const match=s=>s.purpose!=='edit_feedback'&&sameOwner(s,owner)&&s.view===v[1]&&s.frame===v[2]&&s.sceneGeneration===v[4]&&s.surface===v[5]&&s.metrics===v[6];
   const submits=providerSubmits.filter(s=>match(s)&&s.code==='0');
   if(submits.length!==1){out.missing.push({reason:submits.length?'work capture has ambiguous accepted provider submits':'work capture has no observed accepted provider submit',seq:r.seq,work:v[0]});continue;}
   const submitted=submits[0];
   // The frame may have many preparation attempts. Only the exact job/basis
   // that actually submitted can identify its drawn attempt; never choose the
   // first/latest frame record or discard earlier attempts from the output.
   const attempts=renderFrames.filter(match),draws=attempts.filter(s=>s.job===submitted.job&&s.basis===submitted.basis);
   if(draws.length!==1){out.missing.push({reason:draws.length?'accepted provider job has ambiguous render attempts':'accepted provider job has no observed canonical render attempt',seq:r.seq,work:v[0]});continue;}
   const draw=draws[0];
   if(owner.sceneRevision!==draw.sceneRevision||owner.sceneRevision!==submitted.sceneRevision||owner.camera!==draw.camera){fail('work capture, provider and actual drawn basis differ',r);continue;}
   if(draw.record_index>=submitted.record_index||!observedBefore(draw.at_ns,submitted.at_ns)||!observedBefore(submitted.at_ns,r.at_ns)){fail('canonical draw/provider/capture chronology mismatch',r);continue;}
   const sceneMatches=scenes.filter(s=>sameOwner(s,owner)&&s.sceneGeneration===v[4]&&s.semanticGeneration===v[3]&&s.sceneRevision===draw.sceneRevision);
   const scene=sceneMatches.length===1?sceneMatches[0]:null;
   if(!scene){out.missing.push({reason:'frame Scene publication outside observed window',seq:r.seq,work:v[0]});}
   else if(owner.commitRevision!==scene.commitRevision||owner.documentEpoch!==scene.documentEpoch||owner.document!==scene.document){fail('frame commit and published Scene differ',r);continue;}
   const key=ownerKey(owner)+'/'+v[0];
   if(work.has(key)){fail('duplicate canonical work capture',r);work.set(key,null);continue;}
   const f={...owner,work:v[0],view:v[1],frame:v[2],semanticGeneration:v[3],sceneGeneration:v[4],surface:v[5],metrics:v[6],operation:scene?.operation??null,scene_qualified:!!scene,at_ns:submitted.at_ns,job:draw.job,basis:draw.basis,gpu_resource_epoch:draw.gpu_resource_epoch,
    attempt_jobs:[...new Set(attempts.filter(s=>s.sceneRevision===owner.sceneRevision&&s.camera===owner.camera).map(s=>s.job))],capture_seq:r.seq,capture_ns:r.at_ns,draw_seq:draw.seq,provider_seq:submitted.seq,
    provenance:[owner.seq,actualOwner.owner_seq,r.seq,submitted.seq,draw.seq,...(scene?.provenance??[])]};
   out.frames.push(f);work.set(key,f);
  }
  if(r.kind===12&&r.tag===29&&v[0]!=='0'){
   const completionOwner=ownerFor(r),f=work.get(ownerKey(completionOwner)+'/'+v[1]),observed={runtime:v[2],lifetime:v[3],attachment:v[4],view:v[5],frame:v[6],semanticGeneration:v[7],sceneGeneration:v[8],surface:v[9],metrics:v[10]};
   if(v[0]==='1'){
    if(completionOwner&&['runtime','lifetime','attachment','view'].some(k=>completionOwner[k]!==observed[k])){fail('foreign accepted completion caller',r);continue;}
    if(!f){out.missing.push({reason:'accepted completion outside observed work',seq:r.seq});continue;}
    if(Object.keys(observed).some(k=>f[k]!==observed[k])){fail('foreign/stale accepted completion',r);continue;}
    if(!observedBefore(f.capture_ns,r.at_ns)){fail('canonical completion precedes observed capture',r);continue;}
    const key=ownerKey(completionOwner)+'/'+v[1];if(completedWork.has(key)){fail('duplicate accepted canonical completion',r);continue;}completedWork.add(key);
    const c={...f,...observed,at_ns:r.at_ns,provenance:[...f.provenance,r.seq],qualification:f.scene_qualified?'QUALIFIED':'MISSING_SETUP_SCENE_OBSERVATION'};
    (f.scene_qualified?out.qualified_completions:out.unqualified_completions).push(c);
   }else out.unqualified_completions.push({...observed,work:v[1],at_ns:r.at_ns,qualification:'REJECTED'});
  }
  if(r.kind===8){out.decisions.push({id:v[0],route:routeNames[Number(v[1])]??'UNKNOWN',job:r.job,basis:r.basis,actual_target:targetNames[Number(v[2])]??'UNKNOWN',requested_gpu:v[3]==='1',origin:'CURRENT_BRANCH',seq:r.seq});}
  if(r.kind===9){
   const decision=out.decisions.findLast(d=>d.id===v[0]);const leaf={decision:v[0],route:routeNames[Number(v[1])]??'UNKNOWN',target_label:v[2],target:targetNames[Number(v[3])]??'UNKNOWN',width:v[4],height:v[5],format:v[6],job:r.job,basis:r.basis,at_ns:r.at_ns,seq:r.seq,target_identity_scope:'equivalent actual target uses within job; address may be reused, never a persistent/canonical ID'};
   if(!decision||decision.job!==leaf.job||decision.basis!==leaf.basis||decision.route!==leaf.route)fail('foreign decision/leaf basis',r);
   if(leaf.route==='UNKNOWN'||leaf.route==='NONE')fail('unknown/unbound observed route',r);out.leaves.push(leaf);
  }
  if(r.kind===10){out.counters.push(r);if(r.tag===24&&v[0]==='9'){
   const leaf=out.leaves.findLast(l=>l.decision===v[1]);if(!leaf||leaf.job!==r.job)fail('target-use aggregate has foreign job',r);else{leaf.actual_uses=v[9];leaf.last_use_ns=v[10];}
  }}
  if([13,14,15].includes(r.kind)){
   const group=groups.get(v[0])??{group:v[0],job:r.job,basis:r.basis,submit_ns:null,completed_ns:null,retired_ns:null,pure_gpu_duration_ns:null,provenance:[]};groups.set(v[0],group);group.provenance.push(r.seq);
   if(r.kind===13){group.context_label=v[1];group.epoch=v[2];group.submit_ns=r.at_ns;group.submit_accepted=v[3]==='1';}
   if(r.kind===14){group.completed_ns=r.at_ns;group.abandoned=v[2]==='1';group.physical_lost=v[3]==='1';}
   if(r.kind===15)group.retired_ns=r.at_ns;
  }
 }
 out.commits=commits;out.scenes=scenes;out.render_frames=renderFrames;out.provider_submissions=providerSubmits;
 out.owner_bindings=[...owners].map(([key,set])=>({context_key:key,ambiguous:set.size!==1,owners:[...set.values()]}));
 for(const s of begins.values()){
  if(s.inclusive_ns!==null){if(s.exclusive_ns===null){const child=[...begins.values()].filter(c=>c.parent===s.id&&c.inclusive_ns!==null).reduce((n,c)=>n+BigInt(c.inclusive_ns),0n);s.exclusive_ns=String(BigInt(s.inclusive_ns)-child);}if(BigInt(s.exclusive_ns)<0n)fail('invalid nested stage durations');}
  else s.status='RIGHT_CENSORED';out.spans.push(s);
 }
 for(const g of groups.values()){g.callback_before_submit_return=g.submit_ns!==null&&g.completed_ns!==null&&BigInt(g.completed_ns)<BigInt(g.submit_ns);g.status=g.abandoned||g.physical_lost?'LOST':g.retired_ns?'RETIRED':g.completed_ns?'COMPLETED_WAITING_RETIREMENT':g.submit_ns?'PENDING':'MISSING_SUBMIT';out.gpu.push(g);}
 return out;
}
