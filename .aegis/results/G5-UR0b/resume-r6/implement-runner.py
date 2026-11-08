"""Create the two-path-scoped r6 orchestration from the immutable r5 mechanics."""
import re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
code=(ROOT/'verification/runners/g5/ur0/reverified_remaining_runner.py').read_text()
code=code.replace('One independently authorized r5 window: exact17 reuse, then remaining36.','One authorized r6 window: exact17+1 recorded reuse, then remaining35.')
code=code.replace('import source_identity','import source_identity\nimport reverified_remaining_runner as predecessor')
for a,b in [('resume-r5','resume-r6'),('0feefbab71d9b214bbebdd6a456290a461d767734280d73c86677c0fc5cd06c6','0dc1abe9149759d25bbea9b37565094e943b23fd583f995194324818de44f920'),('control-review-correctness-final','control-review-trace-lifecycle-r1'),('151aad4d4625a671c4132fc614416c9536e76d74c7f23722b2677ef29b93e501','bfaa5b13023b0ebb37dc9d68e47947e9381a869d4df777e19248646784ae5c73'),('9fb07bc5798cd455d9a98a3b8b8cf23d7eb42b7c6e68c51edd3adf50f1e4798d','f76c8b5249efc4b1e3a908e2ee19cc94461cd5592f9856518b9073c289404135'),('8506692699863cfaed74a5d4dd69feca2a3afb91e7192ebc88cee63c46e41632','dcb0f4b140d32b23c370331739379b11b32c424e740e2725d8d31e867e7d7ba7'),('r5 window','r6 window'),('r5 authorization','r6 authorization'),('r5-worker','r6-worker'),('r5 package','r6 package'),('g5-ur0b-r5-window','g5-ur0b-r6-window'),('all36','all35')]:code=code.replace(a,b)
def replace(name,body):
 global code
 start=code.index('def '+name+'(');m=re.search(r'^(?:def |class )',code[start+1:],re.M);end=start+1+m.start() if m else len(code)
 code=code[:start]+body.strip()+'\n'+code[end:]
replace('validate_plan','''
def validate_plan(rows):
 need(rows==plan() and len(rows)==35 and len({r['run_id'] for r in rows})==35 and sum(r['limit_seconds'] for r in rows)==5850,'changed/extra/reordered remaining35 plan')
 need(all(r.get('mode')=='correctness' and r['phase']=='separate' and r['limit_seconds']==90 for r in rows[:5]) and all(r['phase']=='main' and r['limit_seconds']==180 for r in rows[5:]),'wrong correctness/main order or limits')
 need({r['branch'] for r in rows[:5]}==set(repair.CASES)-{'lower_z_translucent_drag'},'lower-z cannot start again; five distinct cases required')
 old=bound(request()['historical_ledger_ref']);need(old['status']=='CLOSED','old window is not closed')
 for row in rows:
  prior=next(r for r in old['rows'] if r['run_id']==row['predecessor_run_id'])
  need(row['window_run_key']=='G5-UR0b-resume-r6/'+row['run_id'] and prior['status']==row['predecessor_status']=='MISSING' and prior['admission_status']==row['predecessor_admission_status']=='NOT_STARTED','predecessor row identity changed')
 return rows
''')
replace('verify_history','''
def verify_history(history=None):
 trusted=bound(request()['historical_ledger_ref']);need(history is None or history==trusted,'historical ledger changed')
 h=predecessor.verify_history();need(trusted['status']=='CLOSED' and trusted['historical_rows']==h['rows'] and trusted['historical_starts']==33 and trusted['historical_charged_seconds']==1251,'historical33 omitted or reset')
 rows=copy.deepcopy(h['rows']);total=h['seconds']
 for row in trusted['rows']:
  if 'outcome' not in row:continue
  outcome=row['outcome'];g=outcome['guard'];cost=resume.charge(g,row['limit_seconds'])
  need(cost==row['charged_seconds'] and g['cleanup_complete'] and load(Path(outcome['guard_record']))==g,'historical r5 guard/charge differs')
  rows.append({'run_key':'resume-r5/'+row['run_id'],'worker_limit_seconds':row['limit_seconds'],'charged_seconds':cost,'guard':g});total+=cost
 need(len(rows)==34 and total==1264 and trusted['new_starts']==1 and trusted['new_spent_seconds']==13 and trusted['charged_seconds']==1264,'historical34/1264 incomplete')
 return {'starts':34,'seconds':1264,'rows':rows,'window_ref':request()['historical_ledger_ref']}
''')
replace('accepted_reuse','''
def accepted_correctness(case,summary,chain,packet):
 need(packet==acceptance() and packet.get('recorded_correctness_cases')==[case],'foreign recorded correctness/acceptance')
 need(case['case_id']=='lower_z_translucent_drag' and case['status']=='ACCEPTED_RECORDED_CORRECTNESS_AFTER_SCOPED_OFFLINE_REVERIFICATION' and case['fresh'] is False and case['recording_mode']=='WORKER_COMPILE' and case['derived_mode']=='OFFLINE_REVERIFICATION','recorded correctness freshness or status differs')
 need(summary==bound(case['summary_ref']) and chain==bound(case['proof_ref']),'recorded correctness derived bytes differ')
 a=summary.get('analysis',{});need(a.get('mode')=='OFFLINE_REVERIFICATION' and a.get('input_source_tree_sha256')==case['recording_source_tree_sha256'] and a.get('compiler_files')==packet['compiler_file_sha256'],'recorded correctness source/compiler/mode differs')
 need(case['canonical_scope']==chain.get('scope')=='NATIVE_ONLY' and case['metrics_not_applicable'] is True and summary.get('schema_status')=='MISSING' and chain.get('metric_receipt_count') is None,'native-only correctness must not invent metrics')
 need(summary.get('canonical_chain')==chain and summary.get('canonical_chain_ref')=={'path':'canonical-chain.json','sha256':case['proof_ref']['sha256']} and all(chain.get(k)==v for k,v in case['counts'].items()),'recorded correctness proof/count binding differs')
 repair.require_canonical_chain(summary,require_metrics=False)
 for item,key in zip(case['input_refs'],['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']):bound_hash(item);need(a.get(key)==item['sha256'],'recorded correctness input differs')
 raw=bound_hash(case['input_refs'][0]).parent;run=bound(case['input_refs'][1]);inputs=bound(case['input_refs'][2])
 need(run.get('mode')=='correctness' and run.get('branch')==case['case_id'] and 'measurement' not in run,'foreign correctness/measurement recording')
 for source in ['source_before','source_after']:need(run[source]['source_tree_sha256']==case['recording_source_tree_sha256'] and run[source]['revision']==case['recording_revision'],'recorded correctness source/revision differs')
 expected=bound(request()['accepted_artifact_provenance_ref'])['qualified_sdk_artifacts']
 need(run['artifacts_before']==run['artifacts_after']==summary['artifact_identity']==expected and run['source_stable'] and run['artifacts_stable'] and summary['source_stable'] and summary['artifacts_stable'],'recorded correctness artifact/source stability differs')
 scope=repair.trace_scope_for_recording(run,inputs,len([x for x in (raw/'raw-trace.jsonl').read_text().splitlines() if x.strip()]))
 need(scope==case['trace_scope'] and scope is not None,'recorded correctness actual ON-page scope differs');bound_hash(scope['verification_spec_ref'])
 need(len(chain['initialization_chains'])==1,'recorded correctness initialization population differs');origin=chain['initialization_chains'][0]['initialization_basis']
 need(origin.get('trace_scope')==scope and origin['input_refs']=={k:a[k] for k in ['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']},'recorded correctness origin input/scope differs')
 states=[]
 for key in ['init','paired_init']:
  s=run[key]['state'];states.append({'state_path':'worker-result.json/'+key+'/state','public_state':{'documentId':s['documentId'],'generation':s['generation'],'objects':s['objects'],'lastCommit':{k:s['lastCommit'][k] for k in ['ordinal','operationId']},'target':{k:s['target'][k] for k in ['runtimeEpoch','attachmentEpoch','viewId']}},'page_time_origin_ms':run[key]['environment']['timeOrigin']})
 need(origin['recorded_states']==states and origin['source_profile']==packet['repaired_worker_and_helpers'] and origin['recording_source_tree_sha256']==case['recording_source_tree_sha256'],'recorded correctness public-state/profile provenance differs')
 old=bound(case['old_window_ref']);prior=next(r for r in old['rows'] if r['run_id']=='correctness-lower_z_translucent_drag')
 guard=load(raw.parent.parent/'guards/correctness-lower_z_translucent_drag.worker.json')
 need(old['status']=='CLOSED' and prior['admission_status']==case['old_admission_remains']=='REJECTED' and prior['outcome']['guard']==guard==case['guard'],'old window/guard evidence differs')
 check_guard({'limit_seconds':90},{'guard':guard})
 verdict=correctness_verdict(run.get('correctness'),case['case_id']);need(verdict==run['correctness_status']==case['pixel_status']=='QUALIFIED','recorded correctness complete OFF/ON pair differs')
 for e in run['correctness']:
  label=e['toggle'].lower()+'-'+str(e['step']);reference=raw/Path(e['comparison']['reference_snapshot']).name
  need(sha(raw/(label+'.candidate.rgba'))==e['candidate_sha256']==e['source_sha256'] and sha(reference)==e['reference_snapshot_sha256'],'recorded correctness actual pixel/snapshot bytes differ')
  if e['step']==0:
   subject=load(raw/(label+'.subject.json'));need(subject['proof']==e.get('applicability') and e['applicability']['status']==case['subject_status']=='QUALIFIED','recorded correctness subject differs')
 need(chain['chains']==load(raw/'canonical-chain.json')['chains'],'recorded correctness committed chains changed')
 return dict(copy.deepcopy(case),correctness_status=verdict)
def accepted_reuse(packet):
 need(packet==acceptance() and packet.get('status')=='ACCEPTED_CURRENT_COMPILER_17_PREREQUISITES_AND_ONE_CORRECTNESS','not the exact independent scoped acceptance')
 need(packet['historical_r4_packet_now_valid'] is False and packet['recorded_is_fresh'] is False and packet['new_sampling_authorized_by_this_record'] is False,'recorded applicability cannot create fresh permission')
 verify_profiles(packet)
 need(len(packet['records'])==17 and len({r['run_id'] for r in packet['records']})==17 and len(packet['recorded_correctness_cases'])==1,'recorded17+1 population changed')
 initial=load(ROOT/'.aegis/verification/G5-UR0b-initialization-v1/evidence-inputs.json')
 need({r['run_id'] for r in packet['records']}=={r['run_id'] for r in initial['inputs']}|{'realtime-120','realtime-240'},'wrong prerequisite IDs')
 records=[]
 for row in packet['records']:
  summary=bound(row['summary_ref']);chain=bound(row['proof_ref'])
  for ref in row['recorded_input_refs']:bound_hash(ref)
  run=bound(row['recorded_input_refs'][1]);need(run['source_before']['source_tree_sha256']==row['recording_source_tree_sha256'] and run['artifacts_before']==row['accepted_artifacts'] and run['source_stable'] and run['artifacts_stable'],'actual recording source/artifacts differ')
  snapshot=Path(row['recorded_input_refs'][1]['path']).parent/run['final']['snapshot']['path'];need(sha(ROOT/snapshot)==run['final']['snapshot']['sha256'],'recorded final snapshot differs')
  records.append(admit_record(row,summary,chain,packet))
 case=packet['recorded_correctness_cases'][0];correctness=accepted_correctness(case,bound(case['summary_ref']),bound(case['proof_ref']),packet)
 need(packet['accepted_prerequisites']=={'diagnostic_off_on':True,'six_pair_overhead':True,'realtime_hz':[60,120,240],'recorded_correctness':['lower_z_translucent_drag']},'independent scoped prerequisite applicability missing')
 return {'status':'ACCEPTED_SCOPED_17_PLUS_ONE_REUSE','acceptance_ref':request()['accepted_reuse_ref'],'records':records,'recorded_correctness_cases':[correctness],'prerequisites':packet['accepted_prerequisites'],'recorded_is_fresh':False}
''')
replace('require_phase','''
def require_phase(phase,rows,reused):
 packet=acceptance();expected=dict(packet['recorded_correctness_cases'][0],correctness_status='QUALIFIED')
 need(reused.get('status')=='ACCEPTED_SCOPED_17_PLUS_ONE_REUSE' and reused.get('acceptance_ref')==request()['accepted_reuse_ref'] and reused.get('prerequisites')==packet['accepted_prerequisites'] and reused.get('recorded_correctness_cases')==[expected],'exact17 plus distinct native-only recorded correctness missing')
 need(phase in ['separate','main'],'unapproved phase')
 if phase=='main':
  need(len(rows[:5])==5 and all(r['status']=='COMPLETED' and r.get('admission_status')=='ADMITTED' and r.get('correctness_status') in ['QUALIFIED','PRODUCT_RED_IN_BOTH_OFF_ON'] for r in rows[:5]),'main requires all five admitted fresh correctness cases')
  need({r.get('branch') for r in rows[:5]}|{expected['case_id']}==set(repair.CASES) and len({r.get('branch') for r in rows[:5]})==5,'recorded1+fresh5 cannot duplicate or omit a case')
''')
start=code.index('class Window:');end=code.index('def authority(',start)
window=code[start:end]
for a,b in [("'historical_starts':33","'historical_starts':34"),('1251','1264'),('5940','5850'),('36','35')]:window=window.replace(a,b)
code=code[:start]+window+code[end:]
code=code.replace("review['status']=='ACCEPTED_OFFLINE_REPAIR_AND_EXACT_RECORDED_REUSE'","review['status']=='ACCEPTED_TRACE_LIFECYCLE_REPAIR_AND_RECORDED_CORRECTNESS'")
code=code.replace('cwd=ROOT,check=True,timeout=120,stdout=log','cwd=ROOT,check=True,timeout=120,stdin=subprocess.DEVNULL,stdout=log')
code=code.replace("window.data['source_tree_sha256']=proof['source_tree_sha256'];window.save()","window.data['source_tree_sha256']=proof['source_tree_sha256'];window.data['execution_revision']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip();window.save()")
(ROOT/'verification/runners/g5/ur0/scoped_remaining_runner.py').write_text(code)
