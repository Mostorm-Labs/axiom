// Frozen fresh-empty recording profile; generation zero alone is never a basis.
import {createHash} from 'node:crypto';
import {isDeepStrictEqual} from 'node:util';
export const initializationProfile=Object.freeze({
 'verification/runners/g5/ur0/consumer.mjs':'4512faa07223db9a9a761eeccdc2210e8fef65becf65a9b3e46135553117aee7',
 'verification/runners/g5/ur0/worker.mjs':'a483fcc8763eb8ed8a47005e81d12316ad4e9fa04fa08165eec42b8a100425f1'
});
// Exact final worker/cold-helper tuple, frozen before proof use.
// Never read current files to authorize an arbitrary recording.
export const repairedInitializationProfile=Object.freeze({
 "verification/runners/g5/ur0/consumer.mjs": "4512faa07223db9a9a761eeccdc2210e8fef65becf65a9b3e46135553117aee7",
 "verification/runners/g5/ur0/worker.mjs": "3e3d5e7946900d053b8561720354c7bc8c5a7d852d0ed5b509fa56bda1a5b735",
 "verification/runners/g5/ur0/correctness_driver.mjs": "0e97e180e3111b99f935922b07a4be70b210c03b79194d98cd6f043d0ec64721",
 "verification/runners/g5/ur0/pixel_oracle.mjs": "dba87ac4edd0a36d51360e7e2adeb2433efab569ffe357c617952610261c7be1",
 "verification/runners/g5/ur0/stream_transfer.mjs": "4dd695cd3b0f01e41b1ea08d741a8bc9a0e146b881bddcaea4173d4da0d39210",
 "verification/runners/g5/ur0/correctness_transfer.mjs": "87565a69adf1c167a888365f0b9c74ffee588667651c30a8eeb3a730eaf5a3c8",
 "verification/runners/g5/ur0/correctness_contract.mjs": "682080e52acf98d8cbd0d5c5b7cfa2f3f8ded04a80cb54ae6cbf42334b316da0"
});
export const startupInitializationProfile=Object.freeze({
 'verification/runners/g5/ur0/consumer.mjs':'aabe47cdaca09aa4270488e84788c50348514169df4b1aff434b1a194f5191ff',
 'verification/runners/g5/ur0/worker.mjs':'2d47ccfd246fd6d52d5e4194d62368658a340cad87477213b1987a1801b1db78',
 'verification/runners/g5/ur0/correctness_driver.mjs':'0e97e180e3111b99f935922b07a4be70b210c03b79194d98cd6f043d0ec64721',
 'verification/runners/g5/ur0/pixel_oracle.mjs':'dba87ac4edd0a36d51360e7e2adeb2433efab569ffe357c617952610261c7be1',
 'verification/runners/g5/ur0/stream_transfer.mjs':'4dd695cd3b0f01e41b1ea08d741a8bc9a0e146b881bddcaea4173d4da0d39210',
 'verification/runners/g5/ur0/correctness_transfer.mjs':'87565a69adf1c167a888365f0b9c74ffee588667651c30a8eeb3a730eaf5a3c8',
 'verification/runners/g5/ur0/correctness_contract.mjs':'682080e52acf98d8cbd0d5c5b7cfa2f3f8ded04a80cb54ae6cbf42334b316da0',
 'verification/runners/g5/ur0/correctness_startup.mjs':'5f08d489884f1b242bfdee3e19b932447137adf7d098579ae853ef39e6207d39'
});
const hash=x=>createHash('sha256').update(x).digest('hex');
const hex=x=>typeof x==='string'&&/^[0-9a-f]{64}$/.test(x);
const uint=x=>typeof x==='string'&&/^(0|[1-9][0-9]*)$/.test(x)&&BigInt(x)<2n**64n;
const ownerFields=['runtime','lifetime','attachment','document','documentEpoch','resource','view'];
const sameOwner=(a,b)=>a&&b&&ownerFields.every(k=>a[k]!==undefined&&a[k]===b[k]);
const publicState=s=>({documentId:s.documentId,generation:s.generation,objects:s.objects,lastCommit:{ordinal:s.lastCommit?.ordinal,operationId:s.lastCommit?.operationId},target:{runtimeEpoch:s.target?.runtimeEpoch,attachmentEpoch:s.target?.attachmentEpoch,viewId:s.target?.viewId}});
const matchesState=(s,c)=>s?.documentId===c.document&&s.target?.attachmentEpoch===c.attachment&&s.target?.viewId===c.view&&s.target?.runtimeEpoch===c.documentEpoch;
const lifecycleSpec={path:'.aegis/verification/G5-UR0b-trace-lifecycle-v1/verification-spec.json',sha256:'bd6e1d2f36f7e20317a326fc64ff9856523927cb054acc2ec443cbc18052ed5a'};
const correctnessCases=['lower_z_translucent_drag','permanent_live_mask','transparent_color_bg','warm_miss_pressure','dpr_switch','feedback_canonical'];
const startupSpec={path:'.aegis/verification/G5-UR0b-correctness-startup-v1/verification-spec.json',sha256:'7bb77a8fe61739267ac23f46d15b4920e9b3331de7c2054932a1b99a9bcac534'};
function validStartup(record,budget){
 const e=record?.correctness_startup,s=record?.state;
 if(!e||!s||e.scheme!=='CORRECTNESS_STARTUP_QUIESCENT_V1'||typeof e.page_time_origin_ms!=='number'||!Number.isFinite(e.page_time_origin_ms)||e.page_time_origin_ms<=0||e.page_time_origin_ms!==record.environment?.timeOrigin||!isDeepStrictEqual(e.verification_spec_ref,startupSpec)||!Number.isSafeInteger(e.accepted_callback_count)||e.accepted_callback_count<1||!uint(e.last_presented_count)||BigInt(e.last_presented_count)===0n||e.pending_sdk_raf!==0)return false;
 if(!['created_at_ns','accepted_at_ns','quiescent_at_ns'].every(k=>uint(e[k]))||BigInt(e.created_at_ns)>BigInt(e.accepted_at_ns)||BigInt(e.accepted_at_ns)>BigInt(e.quiescent_at_ns)||BigInt(e.quiescent_at_ns)-BigInt(e.created_at_ns)>BigInt(budget)*1000000n)return false;
 const expected={documentId:s.documentId,generation:s.generation,objects:s.objects,activeContacts:s.activeContacts,lastCommit:{ordinal:s.lastCommit?.ordinal,operationId:s.lastCommit?.operationId},target:Object.fromEntries(['runtimeEpoch','attachmentEpoch','viewId','viewEpoch'].map(k=>[k,s.target?.[k]]))};
 return s.documentId==='9'.repeat(32)&&s.generation==='0'&&s.objects===0&&s.activeContacts===0&&s.lastCommit?.ordinal==='0'&&s.lastCommit?.operationId==='0'.repeat(32)&&Object.values(expected.target).every(v=>uint(v)&&BigInt(v)>0n)&&isDeepStrictEqual(e.public_state,expected);
}
function finalOnTraceScope(run,inputs,raw){
 if(run.schema!=='g5-ur0b-worker-v1'||run.mode!=='correctness'||run.toggle!=='ON'||!correctnessCases.includes(run.branch)||run.status!=='OBSERVED'||run.close?.closed!==true)return null;
 if(run.source_stable!==true||run.artifacts_stable!==true||!isDeepStrictEqual(run.source_before,run.source_after)||!isDeepStrictEqual(run.artifacts_before,run.artifacts_after))return null;
 if(!run.artifacts_before||!Object.keys(run.artifacts_before).length||!Object.values(run.artifacts_before).every(hex))return null;
 if(['schema','mode','branch','toggle'].some(k=>inputs[k]!==run[k])||!isDeepStrictEqual(inputs.source_before,run.source_before)||!isDeepStrictEqual(inputs.artifacts_before,run.artifacts_before))return null;
 if(run.trace?.records!==raw.length||run.trace?.loss?.critical!==0)return null;
 const entries=run.correctness;
 if(!Array.isArray(entries)||entries.length!==4||entries.some(e=>e.case_id!==run.branch||!['OFF','ON'].includes(e.toggle)||![0,1].includes(e.step)||!['QUALIFIED','FAILED','MISSING'].includes(e.comparison?.status)||!hex(e.candidate_sha256)||e.candidate_sha256!==e.source_sha256||!hex(e.reference_snapshot_sha256)))return null;
 if(new Set(entries.map(e=>e.toggle+'/'+e.step)).size!==4)return null;
 const off=run.init?.environment?.timeOrigin,on=run.paired_init?.environment?.timeOrigin;
 if(!run.init?.state||!run.paired_init?.state||![off,on].every(x=>typeof x==='number'&&Number.isFinite(x)&&x>0)||on<=off)return null;
 return {scheme:'CORRECTNESS_FINAL_ON_PAGE_V1',qualifying_state_path:'worker-result.json/paired_init/state',off_page_time_origin_ms:off,trace_page_time_origin_ms:on,verification_spec_ref:{...lifecycleSpec}};
}
export function initializationBasis(completion,decoded,raw,recording){
 const absent=reason=>({status:'UNRESOLVED',reason});
 if(decoded.errors?.length||decoded.loss?.critical)return absent('loss or identity error prevents initialization proof');
 if(!recording?.run||!recording.input_refs)return absent('missing archived initial-state/input binding');
 const bound=[['raw_trace_jsonl','raw_trace_sha256'],['worker_result_json','worker_result_sha256'],['run_inputs_json','run_inputs_sha256']];
 if(bound.some(([file,key])=>typeof recording[file]!=='string'||!hex(recording.input_refs[key])||hash(recording[file])!==recording.input_refs[key]))return absent('initial-state input bytes/hash binding differs');
 let run,inputs;
 try{
  run=JSON.parse(recording.worker_result_json);inputs=JSON.parse(recording.run_inputs_json);
  if(!isDeepStrictEqual(run,recording.run)||!isDeepStrictEqual(raw,recording.raw_trace_jsonl.trim().split('\n').filter(Boolean).map(JSON.parse)))return absent('initial-state recording differs from actual input bytes');
 }catch{return absent('malformed initial-state recording input');}
 const matches=p=>Object.entries(p).every(([name,expected])=>hex(expected)&&run.source_before?.files?.[name]?.sha256===expected);
 const old=matches(initializationProfile),repaired=!old&&['correctness','fixture','main','realtime','legacy','setup'].includes(run.mode)&&matches(repairedInitializationProfile),startup=!old&&!repaired&&run.mode==='correctness'&&matches(startupInitializationProfile);
 if(!hex(run.source_before?.source_tree_sha256)||!old&&!repaired&&!startup)return absent('fresh-empty creation source profile unproved');
 if(startup&&(!validStartup(run.init,45000)||!validStartup(run.paired_init,30000)))return absent('correctness startup frontier unproved');
 const scoped=(repaired||startup)&&run.mode==='correctness',trace_scope=scoped?finalOnTraceScope(run,inputs,raw):null;
 if(scoped&&!trace_scope)return absent('final ON-page correctness trace lifecycle unproved');
 const c=completion,candidates=(scoped?['init','paired_init']:['init']).filter(k=>run[k]?.state).map(k=>({state_path:'worker-result.json/'+k+'/state',state:run[k].state,...(scoped?{page_time_origin_ms:run[k].environment.timeOrigin}:{})}));
 const eligible=scoped?candidates.filter(r=>r.state_path===trace_scope.qualifying_state_path):candidates;
 const matching=old?[{state_path:'worker-result.json/init/state',state:run.init?.state}]:eligible.filter(r=>matchesState(r.state,c));
 if(matching.length!==1)return absent('public initialization state absent, foreign or ambiguous');
 const s=matching[0].state;
 if(!s||s.documentId!==c.document||s.generation!=='0'||s.objects!==0||s.lastCommit?.ordinal!=='0'||s.lastCommit?.operationId!=='0'.repeat(32))return absent('missing/nonempty/restored or contradictory public initial state');
 if(s.target?.attachmentEpoch!==c.attachment||s.target?.viewId!==c.view||s.target?.runtimeEpoch!==c.documentEpoch)return absent('public initial target differs from actual native attachment/view/document epoch');
 if(!/^[0-9a-f]{32}$/.test(c.document??'')||ownerFields.filter(k=>k!=='document').some(k=>!uint(c[k]))||['work','frame','camera','surface','metrics','job','basis','capture_seq','capture_ns','at_ns'].some(k=>!uint(c[k])))return absent('incomplete/lossy native initialization identity');
 if(c.operation!==null||['semanticGeneration','sceneGeneration','sceneRevision','commitRevision'].some(k=>c[k]!=='0'))return absent('nonzero/contradictory native initialization basis');
 const before=raw.filter(r=>(r.kind===6&&r.tag===27||r.kind===7&&r.tag===9)&&(!uint(r.at_ns)||BigInt(r.at_ns)<=BigInt(c.capture_ns)));
 for(const r of before){
  const binding=decoded.owner_bindings?.find(o=>o.context_key===r.job+'/'+r.basis);
  if(!binding||binding.ambiguous||binding.owners.length!==1||sameOwner(binding.owners[0],c))return absent('matching or unproved-owner commit/Scene at or before initialization capture');
 }
 return {status:'PROVEN_FRESH_EMPTY_INITIALIZATION',state_path:matching[0].state_path,
  public_state:publicState(s),
  source_profile:{...(old?initializationProfile:repaired?repairedInitializationProfile:startupInitializationProfile)},recording_source_tree_sha256:run.source_before.source_tree_sha256,input_refs:{...recording.input_refs},
  ...(repaired||startup?{recording_mode:run.mode,recorded_states:candidates.map(r=>({state_path:r.state_path,public_state:publicState(r.state),...(scoped?{page_time_origin_ms:r.page_time_origin_ms}:{})}))}:{}),
  ...(scoped?{trace_scope}:{}),
  ...(startup?{startup_frontiers:{init:run.init.correctness_startup,paired_init:run.paired_init.correctness_startup}}:{}),
  applicability:'frozen fresh-empty createRuntime recording plus actual native chain; no synthetic Operation or Scene'};
}
