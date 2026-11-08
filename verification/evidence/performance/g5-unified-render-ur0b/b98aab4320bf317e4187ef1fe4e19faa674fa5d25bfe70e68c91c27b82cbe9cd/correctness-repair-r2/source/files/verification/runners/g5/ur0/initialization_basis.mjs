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
const hash=x=>createHash('sha256').update(x).digest('hex');
const hex=x=>typeof x==='string'&&/^[0-9a-f]{64}$/.test(x);
const uint=x=>typeof x==='string'&&/^(0|[1-9][0-9]*)$/.test(x)&&BigInt(x)<2n**64n;
const ownerFields=['runtime','lifetime','attachment','document','documentEpoch','resource','view'];
const sameOwner=(a,b)=>a&&b&&ownerFields.every(k=>a[k]!==undefined&&a[k]===b[k]);
const publicState=s=>({documentId:s.documentId,generation:s.generation,objects:s.objects,lastCommit:{ordinal:s.lastCommit?.ordinal,operationId:s.lastCommit?.operationId},target:{runtimeEpoch:s.target?.runtimeEpoch,attachmentEpoch:s.target?.attachmentEpoch,viewId:s.target?.viewId}});
const matchesState=(s,c)=>s?.documentId===c.document&&s.target?.attachmentEpoch===c.attachment&&s.target?.viewId===c.view&&s.target?.runtimeEpoch===c.documentEpoch;
export function initializationBasis(completion,decoded,raw,recording){
 const absent=reason=>({status:'UNRESOLVED',reason});
 if(decoded.errors?.length||decoded.loss?.critical)return absent('loss or identity error prevents initialization proof');
 if(!recording?.run||!recording.input_refs)return absent('missing archived initial-state/input binding');
 const bound=[['raw_trace_jsonl','raw_trace_sha256'],['worker_result_json','worker_result_sha256'],['run_inputs_json','run_inputs_sha256']];
 if(bound.some(([file,key])=>typeof recording[file]!=='string'||!hex(recording.input_refs[key])||hash(recording[file])!==recording.input_refs[key]))return absent('initial-state input bytes/hash binding differs');
 let run;
 try{
  run=JSON.parse(recording.worker_result_json);JSON.parse(recording.run_inputs_json);
  if(!isDeepStrictEqual(run,recording.run)||!isDeepStrictEqual(raw,recording.raw_trace_jsonl.trim().split('\n').filter(Boolean).map(JSON.parse)))return absent('initial-state recording differs from actual input bytes');
 }catch{return absent('malformed initial-state recording input');}
 const matches=p=>Object.entries(p).every(([name,expected])=>hex(expected)&&run.source_before?.files?.[name]?.sha256===expected);
 const old=matches(initializationProfile),repaired=!old&&['correctness','fixture','main','realtime','legacy','setup'].includes(run.mode)&&matches(repairedInitializationProfile);
 if(!hex(run.source_before?.source_tree_sha256)||!old&&!repaired)return absent('fresh-empty creation source profile unproved');
 const c=completion,candidates=(repaired&&run.mode==='correctness'?['init','paired_init']:['init']).filter(k=>run[k]?.state).map(k=>({state_path:'worker-result.json/'+k+'/state',state:run[k].state}));
 const matching=old?[{state_path:'worker-result.json/init/state',state:run.init?.state}]:candidates.filter(r=>matchesState(r.state,c));
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
  source_profile:{...(old?initializationProfile:repairedInitializationProfile)},recording_source_tree_sha256:run.source_before.source_tree_sha256,input_refs:{...recording.input_refs},
  ...(repaired?{recording_mode:run.mode,recorded_states:candidates.map(r=>({state_path:r.state_path,public_state:publicState(r.state)}))}:{}),
  applicability:'frozen fresh-empty createRuntime recording plus actual native chain; no synthetic Operation or Scene'};
}
