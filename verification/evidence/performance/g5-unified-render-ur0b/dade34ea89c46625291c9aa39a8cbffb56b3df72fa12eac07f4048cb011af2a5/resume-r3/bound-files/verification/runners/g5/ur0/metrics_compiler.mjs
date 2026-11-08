import {decodeTrace,expandLeaseCycles} from './decode_trace.mjs';
import {externalMetrics} from './external_metrics.mjs';
import {summarize} from './schedule.mjs';
const identityFields=['runtime_instance','document_epoch','view_id','lifetime_generation','attachment_epoch','committed_generation','scene_generation','scene_revision','surface_generation','metrics_generation','resource_generation','camera_generation','frame_id','commit_revision','preview_revision','document_id','operation_id','body_id','mask_basis'];
const mapped={runtime_instance:'runtime',document_epoch:'documentEpoch',view_id:'view',lifetime_generation:'lifetime',attachment_epoch:'attachment',committed_generation:'semanticGeneration',scene_generation:'sceneGeneration',scene_revision:'sceneRevision',surface_generation:'surface',metrics_generation:'metrics',resource_generation:'resource',camera_generation:'camera',frame_id:'frame',commit_revision:'commitRevision',preview_revision:'previewRevision',document_id:'document',operation_id:'operation',body_id:'bodyId',mask_basis:'maskBasis'};
const basis=row=>Object.fromEntries(identityFields.map(k=>[k,row?.[mapped[k]]??null]));
const routeAlias=r=>({'RECORDING':'CAN-RECORD','CREATION-CANONICAL':'CREATE-CANON-SECOND','CREATION-TRANSIENT':'CREATE-TRANSIENT-SECOND'}[r]??r);
const stages={4:'exact_prepare',5:'publication_prepare',6:'semantic_apply',7:'observer_publication',9:'scene_apply',14:'reference_build',17:'coverage',18:'material',19:'provider_submit',38:'history_prepare',39:'history_finalize',40:'pathops'};
const ownerFields=['runtime','document','documentEpoch','lifetime','attachment','resource','view'];
export function compileMetrics(run,raw,meta){
 if(!/^[0-9a-f]{40}$/.test(run.source_before?.revision??'')||!/^[0-9a-f]{64}$/.test(meta.fixture_sha256??''))throw Error('missing source/fixture identity; no fabricated schema identity');
 if(!/^\d+$/.test(run.init?.environment?.reported_resolution_ns??''))throw Error('missing measured browser clock resolution');
 const m=run.measurement;if(!m?.origin_ns||!m.end_ns||!m.after)throw Error('incomplete actual measurement return shape');
 const decoded=decodeTrace(raw,run.trace?.loss??{critical:0,statistical:0}),records=expandLeaseCycles(raw),external=externalMetrics(m);
 const recordBySeq=new Map(records.map(r=>[r.seq,r]));
 const scopeOwner=r=>{const entry=decoded.owner_bindings?.find(o=>o.context_key===r.job+'/'+r.basis);return entry&&!entry.ambiguous?entry.owners[0]:null;};
 const ownerEqual=(a,b)=>!!a&&!!b&&ownerFields.every(k=>a[k]!==undefined&&a[k]===b[k]);
 const origin=BigInt(m.origin_ns),until=BigInt(m.end_ns),offset=run.calibration?.status==='QUALIFIED'?BigInt(run.calibration.offset_ns):null;
 const browser=x=>x===null||x===undefined||BigInt(x)<origin||BigInt(x)>until?null:String(BigInt(x)-origin);
 const native=x=>offset===null||x===null||x===undefined?null:browser(String(BigInt(x)-offset));
 const timeNumber=x=>x===null?null:BigInt(x);
 const interactions=new Map(),eventMap=new Map(),nativeEvent=new Map(),metrics=[];
 function interaction(id,kind='resource_recovery'){
  if(!interactions.has(id))interactions.set(id,{interaction_id:id,kind,basis:basis(),events:[],commit:{state:kind==='navigation'?'NOT_APPLICABLE':'MISSING',operation_id:null,commit_revision:null,before_generation:null,after_generation:null,commit_ns:null,publication_prepare_ns:null,history_prepare_ns:null,history_finalize_ns:null,observer_ns:null,return_class:kind==='navigation'?'NOT_APPLICABLE':'MISSING'},scene:{state:kind==='navigation'?'NOT_APPLICABLE':'MISSING',start_ns:null,published_ns:null,committed_generation:null,scene_generation:null,scene_revision:null},submits:[],canonical_receipts:[],resource_completions:[],spans:[]});
  return interactions.get(id);
 }
 const kind=e=>e.kind==='wheel'||/before|after|hot/.test(e.interaction_id)?'navigation':e.interaction_id==='RT'?'stroke':e.interaction_id==='D'?'masked_drag':e.interaction_id==='L-D'?'drag':'partial_erase';
 const byDiagnostic=new Map((m.planned??[]).filter(e=>e.diagnostic_event).map(e=>[e.diagnostic_event,e]));
 const evidence={schema:'g5-ur0b-compiler-provenance-v2',primary_qualification:'separate overhead gate',missing:decoded.missing,errors:decoded.errors,commit_time_semantics:'returned-record observation is a publication upper bound, not an exact internal commit instant',jobs:[],feedback_scope:'only actual same-owner/basis submissions; unproved event/pose associations stay MISSING',unsupported:['physical display time','pure GPU instruction time','Skia-internal software coverage','unobserved helper return codes']};
 const metric=(name,i,event,start,end,{status=null,reason=null}={})=>{
  if(!status)status=start===null?'MISSING':end===null?'RIGHT_CENSORED':'COMPLETE';
  if(status==='COMPLETE'&&(end===null||start===null||BigInt(end)<BigInt(start))){status='MISSING';reason='unqualified/reversed cross-clock bounds';}
  const complete=status==='COMPLETE',censor=status==='RIGHT_CENSORED';
  const row={observation_id:'compiled-'+metrics.length,metric_id:name,interaction_id:i.interaction_id,event_id:event?.event_id??null,start_ns:start,end_ns:complete?end:null,status,value_ns:complete?String(BigInt(end)-BigInt(start)):null,lower_bound_ns:censor?String(until-origin-BigInt(start)):null,reason:reason??(complete?null:censor?'not fulfilled before measurement cutoff':'actual endpoint/basis unavailable')};metrics.push(row);return row;
 };
 for(const e of m.planned??[]){
  if(!['pointer','wheel'].includes(e.kind))continue;
  const i=interaction(e.interaction_id,kind(e)),h=(m.handlers??[]).find(h=>h.event_id===e.event_id);
  const facts=records.filter(r=>r.event===e.diagnostic_event),admissions=facts.filter(r=>r.kind===4&&r.tag===28),results=facts.filter(r=>r.kind===16&&r.tag===28);
  const rejected=admissions.some(r=>r.values[0]==='0'),failed=rejected||results.some(r=>r.values[0]==='0');
  let admission=admissions.length?(rejected?'REJECTED':'ACCEPTED'):'MISSING';
  let end=admissions.length?native(admissions.at(-1).at_ns):null;
  const enter=browser(h?.enter_ns),exit=browser(h?.exit_ns);
  if(end!==null&&(enter===null||exit===null||BigInt(end)<BigInt(enter)||BigInt(end)>BigInt(exit)))end=null;
  const keys=facts.filter(r=>r.kind===1&&r.tag===25),key=keys[0]?.values;
  const input={event_id:e.event_id,batch_id:e.event_id,phase:e.kind==='wheel'?'wheel':e.phase,sample_ids:e.samples?.map((s,n)=>e.event_id+'-sample-'+n)??[e.event_id],confirmation:e.kind==='wheel'?'none':'confirmed',planned_ns:browser(e.planned_ns)??String(BigInt(e.planned_ns)-origin),producer_send_ns:browser(e.producer_ns),handler_enter_ns:enter,handler_exit_ns:exit,admission_end_ns:end,admission,reason:failed?'actual Runtime input rejection':admission==='MISSING'?'Runtime admission not observed':null,content_timestamp_ns:key?.[4]??e.content_timestamp_ns??null,source_id:key?.[0]??null,pointer_id:key?.[1]??null,pointer_generation:key?.[2]??null,catch_up:!!e.catch_up};
  i.events.push(input);eventMap.set(e.event_id,{e,input,failed,results});if(e.diagnostic_event)nativeEvent.set(e.diagnostic_event,{i,e,input,failed});
 }
 for(const original of external.metrics){
  const data=original.event_id?eventMap.get(original.event_id):null,i=interaction(original.interaction_id);
  if(original.metric_id==='FIRST_FEEDBACK'&&data?.failed){metrics.push({...original,status:'FAILED',end_ns:null,value_ns:null,lower_bound_ns:null,reason:'actual Runtime input rejection'});}
  else metrics.push({...original});
 }
 for(const data of eventMap.values()){
  const {e,input,failed}=data,i=interactions.get(e.interaction_id);
  if(failed&&!metrics.some(x=>x.metric_id==='FIRST_FEEDBACK'&&x.event_id===e.event_id))metric('FIRST_FEEDBACK',i,e,input.handler_enter_ns,null,{status:'FAILED',reason:'actual Runtime input rejection'});
  if(e.phase==='down'){
   metric('NEXT_DOWN_ADMISSION',i,e,input.handler_enter_ns,input.admission_end_ns,{status:input.admission_end_ns===null?'MISSING':'COMPLETE'});
   for(const [name,start] of [['NEXT_DOWN_PLANNED_TO_FEEDBACK',input.planned_ns],['NEXT_DOWN_ENTER_TO_FEEDBACK',input.handler_enter_ns]])metric(name,i,e,start,null,{status:failed?'FAILED':'MISSING',reason:failed?'actual Runtime input rejection':'no proved same-demand effective feedback join'});
  }
 }
 for(const c of decoded.commits){
  const joined=nativeEvent.get(c.event),i=joined?.i??interaction('UNATTRIBUTED_CANONICAL');
  if(i.kind==='navigation'){evidence.errors.push({reason:'semantic commit cannot be attributed to navigation',operation:c.operation});continue;}
  i.basis=basis({...c,semanticGeneration:c.afterGeneration});
  i.commit={...i.commit,state:'APPLIED',operation_id:c.operation,commit_revision:c.commitRevision,before_generation:c.beforeGeneration,after_generation:c.afterGeneration,commit_ns:native(c.at_ns),return_class:joined?.failed?'POST_COMMIT_ERROR':'COMMITTED'};
  if(i.commit.commit_ns===null)i.commit.state='MISSING';
  const s=decoded.scenes.find(s=>s.operation===c.operation&&ownerFields.every(k=>s[k]===c[k]));
  if(s)i.scene={state:'PUBLISHED',start_ns:null,published_ns:native(s.at_ns),committed_generation:s.semanticGeneration,scene_generation:s.sceneGeneration,scene_revision:s.sceneRevision};
  if(i.scene.published_ns===null)i.scene.state='MISSING';
 }
 const representation=leaves=>{const types=new Set(leaves.map(l=>l.target));return types.has('CPU')&&types.has('GPU')?'MIXED':types.has('CPU')?'CPU_COVERAGE':types.has('GPU')?'GPU_COVERAGE':'MISSING';};
 for(const f of decoded.frames){
  if(!f.scene_qualified)continue;const c=decoded.commits.find(c=>c.operation===f.operation&&ownerFields.every(k=>c[k]===f[k])),i=nativeEvent.get(c?.event)?.i??interaction('UNATTRIBUTED_CANONICAL');
  const at=native(f.at_ns),leaves=decoded.leaves.filter(l=>l.job===f.job),b=basis(f);
  if(at===null||identityFields.some(k=>!['preview_revision','body_id','mask_basis'].includes(k)&&b[k]===null))continue;
  i.submits.push({submit_id:'canonical-'+f.work+'-'+f.frame,purpose:'canonical',at_ns:at,basis:b,demand_id:'commit-'+f.operation,requested_revision:f.commitRevision,submitted_revision:f.commitRevision,pose_id:'camera-'+f.camera,valid_region_proof:'FULL',qualification:leaves.length?'EFFECTIVE':'MISSING',representation:representation(leaves),target_ids:leaves.map(l=>'job-'+l.job+'-actual-'+l.target_label)});
 }
 for(const f of decoded.qualified_completions){
  const i=[...interactions.values()].find(i=>i.submits.some(s=>s.basis.frame_id===f.frame&&s.basis.runtime_instance===f.runtime&&s.basis.operation_id===f.operation&&s.qualification==='EFFECTIVE'));
  const at=native(f.at_ns);if(!i||at===null)continue;
  i.canonical_receipts.push({completion_id:'canonical-completion-'+f.work+'-'+f.frame,at_ns:at,kind:'CANONICAL_QUALIFIED',basis:basis(f),qualification:'QUALIFIED',matches_current_basis:true});
 }
 const feedbackOwner=new Map(),traceDemandRecords=records.filter(r=>r.kind===1&&r.tag===37&&r.values.length===7);
 for(const f of (decoded.render_frames??[]).filter(f=>f.purpose==='edit_feedback')){
  const traces=records.filter(r=>r.job===f.job&&r.kind===5&&r.tag===37&&r.values.length===7),leaves=decoded.leaves.filter(l=>l.job===f.job);
  if(traces.length!==1||!leaves.length)continue;const trace=traces[0],v=trace.values;
  if(v[2]!==f.camera||v[4]!==f.sceneRevision||v[5]!==f.surface||v[6]!==f.metrics)continue;
  const demands=traceDemandRecords.filter(r=>ownerEqual(scopeOwner(r),f)&&r.values[0]===v[0]&&r.values[2]===v[2]&&r.values[3]===v[3]&&r.values[4]===v[4]&&r.values[5]===v[5]&&r.values[6]===v[6]&&BigInt(r.values[1])<=BigInt(v[1])&&BigInt(r.at_ns)<=BigInt(f.at_ns));
  const destinations=[...new Set(demands.map(r=>nativeEvent.get(r.event)?.i).filter(Boolean))];if(destinations.length!==1)continue;
  const i=destinations[0],submits=(decoded.provider_submissions??[]).filter(s=>s.job===f.job&&ownerEqual(s,f)&&s.frame===f.frame&&s.sceneGeneration===f.sceneGeneration&&s.sceneRevision===f.sceneRevision&&s.surface===f.surface&&s.metrics===f.metrics&&s.code==='0');
  if(submits.length!==1)continue;const at=native(submits[0].at_ns);if(at===null)continue;
  const b=basis({...f,semanticGeneration:v[3],previewRevision:v[1],maskBasis:'runtime-eraser-key-'+v[0]});
  i.submits.push({submit_id:'edit-'+f.job+'-'+f.frame,purpose:'edit_feedback',at_ns:at,basis:b,demand_id:'trace-'+v[0]+'-'+v[1],requested_revision:v[1],submitted_revision:v[1],pose_id:'camera-'+f.camera,valid_region_proof:'FULL',qualification:'EFFECTIVE',representation:representation(leaves),target_ids:leaves.map(l=>'job-'+l.job+'-actual-'+l.target_label)});
  feedbackOwner.set(f.job,{i,trace});
 }
 for(const data of eventMap.values()){
  if(data.failed||data.e.phase!=='down')continue;const i=interactions.get(data.e.interaction_id),s=i.submits.find(s=>s.purpose==='edit_feedback'&&s.qualification==='EFFECTIVE');if(!s)continue;
  for(const name of ['FIRST_FEEDBACK','NEXT_DOWN_PLANNED_TO_FEEDBACK','NEXT_DOWN_ENTER_TO_FEEDBACK']){
   const existing=metrics.find(x=>x.event_id===data.e.event_id&&x.metric_id===name);if(!existing)continue;
   const start=name==='NEXT_DOWN_PLANNED_TO_FEEDBACK'?data.input.planned_ns:data.input.handler_enter_ns;
   if(start!==null&&BigInt(s.at_ns)>=BigInt(start))Object.assign(existing,{start_ns:start,end_ns:s.at_ns,value_ns:String(BigInt(s.at_ns)-BigInt(start)),lower_bound_ns:null,status:'COMPLETE',reason:'actual Runtime retained trace prefix, same owner/Scene/camera/surface and accepted full-frame submit'});
  }
 }
 const viewKey=o=>ownerFields.map(k=>o[k]).join('/'),navOwners=new Map(),navFacts=[];
 const navigationFor=owner=>{const key=viewKey(owner);if(!navOwners.has(key)){const i=interaction('NATIVE_NAVIGATION-'+navOwners.size,'navigation');i.basis=basis(owner);navOwners.set(key,i);}return navOwners.get(key);};
 for(const r of records.filter(r=>r.kind===11&&r.tag===12&&r.values.length===10)){
  const v=r.values,owner=scopeOwner(r),runtime=records.filter(x=>x.job===r.job&&x.kind===5&&x.tag===41&&x.values.length===8),renderer=records.filter(x=>x.job===r.job&&x.kind===5&&x.tag===41&&x.values.length===9),leaves=decoded.leaves.filter(l=>l.job===r.job);
  if(!owner||runtime.length!==1||renderer.length!==1||!leaves.length||v[8]!=='0'||v[9]!=='1')continue;
  const a=runtime[0].values,b=renderer[0].values;
  if(v[0]!==owner.view||v[0]!==b[0]||v[1]!==a[2]||v[1]!==b[2]||v[2]!==a[1]||v[2]!==b[1]||v[3]!==a[3]||v[3]!==b[3]||v[4]!==a[4]||v[4]!==b[4]||[0,1,2].some(n=>v[5+n]!==a[5+n]||v[5+n]!==b[6+n]))continue;
  const demands=records.filter(x=>x.kind===1&&x.tag===41&&x.values.length===8&&ownerEqual(scopeOwner(x),owner)&&x.values.every((value,n)=>value===a[n])&&BigInt(x.at_ns)<=BigInt(r.at_ns));
  if(demands.length!==1)continue;const at=native(r.at_ns),start=native(demands[0].at_ns);if(at===null||start===null)continue;
  const nativeNavigation=navigationFor(owner);
  const actualBasis=basis({...owner,semanticGeneration:a[1],sceneGeneration:b[1],sceneRevision:b[2],surface:b[3],metrics:b[4],camera:a[0]});
  nativeNavigation.basis=actualBasis;nativeNavigation.submits.push({submit_id:'navigation-'+r.seq,purpose:'navigation',at_ns:at,basis:actualBasis,demand_id:'camera-'+a[0],requested_revision:a[0],submitted_revision:a[0],pose_id:a.slice(5).join('/'),valid_region_proof:'FULL',qualification:'EFFECTIVE',representation:representation(leaves),target_ids:leaves.map(l=>'job-'+l.job+'-actual-'+l.target_label)});
  navFacts.push({start_ns:start,at_ns:at,demand_id:'camera-'+a[0],submit_seq:r.seq,submit_id:'navigation-'+r.seq,seq:r.seq,owner,values:a});
 }
 // A newer admitted pose supersedes an outstanding demand without resetting
 // its busy interval. Old-pose submissions stay real submissions, but do not
 // shorten the current view's gap. No idle time or virtual presents are added.
 const navigationTimelines=new Map();
 const timeline=o=>{const key=viewKey(o);if(!navigationTimelines.has(key))navigationTimelines.set(key,[]);return navigationTimelines.get(key);};
 for(const r of records.filter(r=>r.kind===1&&r.tag===41&&r.values.length===8)){
  const owner=scopeOwner(r),at=native(r.at_ns);if(!owner||at===null)continue;
  timeline(owner).push({kind:'demand',at_ns:at,seq:r.seq,values:r.values,owner});
 }
 for(const n of navFacts)timeline(n.owner).push({kind:'submit',...n});
 // A proved canonical physical submit can satisfy that same viewport demand.
 // Reference it here without minting a navigation submit or canonical receipt.
 for(const i of interactions.values())for(const s of i.submits.filter(s=>s.purpose==='canonical'&&s.qualification==='EFFECTIVE')){
  const owner=Object.fromEntries(ownerFields.map(k=>[k,s.basis[Object.keys(mapped).find(n=>mapped[n]===k)]]));
  const t=navigationTimelines.get(viewKey(owner));if(!t)continue;
  const matching=t.filter(d=>d.kind==='demand'&&d.values[0]===s.basis.camera_generation&&d.values[1]===s.basis.committed_generation&&d.values[2]===s.basis.scene_revision&&d.values[3]===s.basis.surface_generation&&d.values[4]===s.basis.metrics_generation&&BigInt(d.at_ns)<=BigInt(s.at_ns));
  const poses=new Set(matching.map(d=>d.values.join('/')));if(poses.size!==1)continue;
  t.push({kind:'submit',at_ns:s.at_ns,seq:null,values:matching[0].values,owner,submit_id:s.submit_id,canonical_proxy:true});
 }
 evidence.native_navigation_proofs=navFacts;evidence.navigation_busy_intervals=[];
 for(const [ownerKey,events] of navigationTimelines){
  const nativeNavigation=navigationFor(events[0].owner);
  events.sort((a,b)=>BigInt(a.at_ns)<BigInt(b.at_ns)?-1:BigInt(a.at_ns)>BigInt(b.at_ns)?1:a.seq&&b.seq?(BigInt(a.seq)<BigInt(b.seq)?-1:1):a.kind==='demand'?-1:1);
  let current=null,busy=null;const seen=new Set();
  for(const e of events){
   const pose=e.values.join('/');
   if(e.kind==='demand'){
    if(seen.has(pose))continue;seen.add(pose);current=pose;if(busy===null)busy=e.at_ns;
   }else if(current!==pose&&!e.canonical_proxy){
    const submit=nativeNavigation.submits.find(s=>s.submit_id===e.submit_id);if(submit)submit.qualification='STALE';
   }else if(busy!==null&&current===pose){
    metric('EFFECTIVE_SUBMIT_GAP',nativeNavigation,null,busy,e.at_ns,{reason:'merged same-owner/view busy interval; current pose/source/full-region proof; physical submit proxy only'});
    evidence.navigation_busy_intervals.push({owner_key:ownerKey,start_ns:busy,end_ns:e.at_ns,physical_submit:e.submit_id??'navigation-'+e.submit_seq,canonical_proxy:!!e.canonical_proxy});busy=null;
   }
  }
  if(busy!==null)metric('EFFECTIVE_SUBMIT_GAP',nativeNavigation,null,busy,null,{status:run.trace?.loss?.critical||run.trace?.loss?.statistical?'MISSING':'RIGHT_CENSORED',reason:'current admitted viewport demand unfulfilled at cutoff; no old pose may close it'});
 }
 for(const i of interactions.values()){
  const up=i.events.findLast(e=>e.phase==='up');if(!up)continue;
  const receipt=i.canonical_receipts.at(0),submit=i.submits.find(s=>s.qualification==='EFFECTIVE');
  metric('UP_TO_COMMIT',i,up,up.handler_enter_ns,null,{status:'MISSING',reason:'only a returned-record upper bound, exact canonical publication time is not observed'});
  metric('COMMIT_TO_SCENE',i,up,null,null,{status:'MISSING',reason:'commit timestamp is an observation upper bound; Scene may publish before the returned record'});
  metric('COMMIT_TO_CANONICAL',i,up,null,null,{status:'MISSING',reason:'exact canonical publication endpoint unavailable'});
  metric('SCENE_TO_SUBMIT',i,up,i.scene.published_ns,submit?.at_ns??null);
  metric('UP_TO_CANONICAL',i,up,up.handler_enter_ns,receipt?.at_ns??null,{status:i.commit.state==='APPLIED'?null:'MISSING'});
  metric('UP_RETURN_TO_CANONICAL',i,up,up.handler_exit_ns,receipt?.at_ns??null,{status:i.commit.state==='APPLIED'?null:'MISSING'});
 }
 const grouped=new Map(),jobInteraction=new Map();
 for(const f of decoded.render_frames??[]){
  if(!ownerFields.every(k=>f[k]!==undefined))continue;
  const feedback=feedbackOwner.get(f.job),purpose=f.purpose??'canonical',scene=decoded.scenes.find(s=>ownerFields.every(k=>s[k]===f[k])&&s.sceneRevision===f.sceneRevision&&s.sceneGeneration===f.sceneGeneration);
  const b=basis({...f,semanticGeneration:feedback?.trace.values[3]??scene?.semanticGeneration,operation:purpose==='canonical'?scene?.operation:undefined,commitRevision:purpose==='canonical'?scene?.commitRevision:undefined,previewRevision:feedback?.trace.values[1],maskBasis:feedback?'runtime-eraser-key-'+feedback.trace.values[0]:undefined});
  const logical=[purpose,...ownerFields.map(k=>f[k]),f.sceneGeneration,f.sceneRevision,f.surface,f.metrics,f.camera,scene?.operation??'?',b.preview_revision??'?',b.mask_basis??'?'].join('/');
  const i=feedback?.i??nativeEvent.get(scene?.event)?.i??interaction(purpose==='canonical'?'UNATTRIBUTED_CANONICAL':'UNATTRIBUTED_FEEDBACK');jobInteraction.set(f.job,i);
  let job=grouped.get(logical);if(!job){job={job_id:'demand-'+grouped.size,interaction_id:i.interaction_id,purpose,fidelity:'exact',basis:b,attempts:[],final_state:'MISSING'};grouped.set(logical,job);}
  const span=decoded.spans.find(s=>s.id===f.job),terminal=records.find(r=>r.kind===16&&r.tag===(purpose==='canonical'?10:11)&&r.values[0]===f.job);
  let outcome='MISSING';
  if(terminal?.values[1]==='1')outcome=terminal.values[2]==='2'?'Pending':terminal.values[2]==='0'?'Submitted':'Failed';
  else if(terminal?.values[1]==='2')outcome='Failed';
  else if(terminal?.values[1]==='0'&&terminal.values[2]==='1'&&(decoded.provider_submissions??[]).some(s=>s.job===f.job&&s.code==='0'))outcome='Submitted';
  const end=native(span?.end_ns);if(end===null)outcome='MISSING';
  job.attempts.push({attempt_id:'call-'+f.job,route_id:purpose==='canonical'?'CAN-BUILD':'ERASE-FEEDBACK',start_ns:native(span?.start_ns),end_ns:end,decision:{decision_id:'unobserved-call-decision-'+f.job,origin:'MISSING',outcome:'MISSING',reason:'actual renderer invocation, no synthetic unified routing decision'},leaves:[],outcome,reason:outcome==='MISSING'?'actual boolean result does not distinguish Pending/failure, or endpoint unavailable':null});
  for(const d of decoded.decisions.filter(d=>d.job===f.job)){
   const leaves=decoded.leaves.filter(l=>l.decision===d.id);
   job.attempts.push({attempt_id:'helper-'+d.id,route_id:routeAlias(d.route),start_ns:native(leaves[0]?.at_ns),end_ns:null,decision:{decision_id:d.id,origin:'CURRENT_BRANCH',outcome:'MISSING',reason:'actual target decision; helper-specific terminal result is not observed'},leaves:leaves.map(l=>({leaf_id:'leaf-'+l.seq,route_id:routeAlias(l.route),decision_id:l.decision,actual_target:l.target,target_id:'job-'+l.job+'-actual-'+l.target_label,work_kind:l.target==='RECORDING'?'recording':'none'})),outcome:'MISSING',reason:'helper lifecycle lacks its own observed result; never copy renderer outcome into it'});
  }
  evidence.jobs.push({job_id:job.job_id,actual_call_job:f.job,frame:f.frame,full_observed_owner:ownerFields.map(k=>f[k]),scope_only_ids_not_logical_demand:true});
  if(decoded.qualified_completions.some(c=>c.job===f.job)||(purpose==='edit_feedback'&&feedback))job.final_state='Completed';
  else if(job.final_state!=='Completed')job.final_state=['Pending','Submitted'].includes(outcome)?'RIGHT_CENSORED':outcome==='Failed'?'Failed':'MISSING';
 }
 for(const span of decoded.spans){
  const stage=stages[span.tag];if(!stage||span.inclusive_ns===null||span.exclusive_ns===null)continue;
  const r=recordBySeq.get(span.seq),i=nativeEvent.get(r?.event)?.i??jobInteraction.get(span.job)??interaction('UNATTRIBUTED_WORK');
  const start=native(span.start_ns),end=native(span.end_ns);
  i.spans.push({span_id:span.id,parent_id:span.parent??null,stage,clock_domain:start!==null&&end!==null?'browser':'wasm_local_duration',start_ns:start!==null&&end!==null?start:null,end_ns:start!==null&&end!==null?end:null,inclusive_ns:span.inclusive_ns,exclusive_ns:span.exclusive_ns});
 }
 for(const i of interactions.values()){const ids=new Set(i.spans.map(s=>s.span_id));for(const s of i.spans)if(!ids.has(s.parent_id))s.parent_id=null;}
 const auxiliary=interaction('UNOBSERVED_METRICS');
 for(const name of ['EFFECTIVE_SUBMIT_GAP'])if(!metrics.some(m=>m.metric_id===name))metric(name,auxiliary,null,null,null,{status:'MISSING',reason:'no proved native demand-window/effective-submit association in this observation'});
 const failed=[...eventMap.values()].some(x=>x.failed)||(m.errors?.length??0)>0||m.after.activeContacts!==0;
 const artifacts=Object.entries(run.artifacts_before??{}).filter(([p])=>/\.(mjs|mts|wasm|data)$/.test(p)).map(([path,sha256])=>({role:path.endsWith('.wasm')?'wasm':path.endsWith('.data')?'resource':path.endsWith('.mts')?'declaration':'js',path,sha256}));
 for(const [path,value] of Object.entries(run.source_before?.files??{}))if(path.startsWith('verification/runners/g5/ur0/')&&value.sha256)artifacts.push({role:'runner',path,sha256:value.sha256});
 const loss={critical_dropped:run.trace?.loss?.critical??0,statistical_dropped:run.trace?.loss?.statistical??0,unknown_routes:decoded.leaves.filter(l=>l.route==='UNKNOWN').length,identity_link_failures:decoded.errors.length};
 const document={schema_version:'g5-ur0-metrics-v1',record_origin:meta.record_origin??'OBSERVATION',run_id:meta.run_id,row_id:meta.row_id,repetition:meta.repetition??1,fixture_sha256:meta.fixture_sha256,source_revision:run.source_before.revision,artifacts,clock:{clock_id:'browser-'+(run.init.environment.timeOrigin??meta.run_id),origin:'browser_run_performance_now',unit:'ns_decimal_string',reported_resolution_ns:run.init.environment.reported_resolution_ns,wasm_mode:offset===null?'LOCAL_DURATIONS_ONLY':'CALIBRATED',calibration_error_ns:run.calibration?.error_bound_ns??null},run_outcome:failed?'FAILED':run.status==='OBSERVED'?'COMPLETE':'MISSING',evidence_status:'INCOMPLETE',performance_status:'NOT_EVALUATED_UR0a',interactions:[...interactions.values()],jobs:[...grouped.values()],metrics,summaries:[],loss,observed_until_ns:String(until-origin),error:run.error??null};
 document.metrics.forEach((x,n)=>x.observation_id='observation-'+n);
 document.summaries=[...new Set(document.metrics.map(x=>x.metric_id))].map(metric_id=>({metric_id,...summarize(document.metrics.filter(x=>x.metric_id===metric_id))}));
 return{document,provenance:evidence,decoded};
}
