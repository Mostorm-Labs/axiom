"""One authorized r6 window: exact17+1 recorded reuse, then remaining35."""
import argparse,copy,fcntl,hashlib,json,math,os,re,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE))
import remaining_runner as previous
import repair_runner as repair
import resume_runner as resume
import source_identity
import reverified_remaining_runner as predecessor
PACKAGE=ROOT/'.aegis/packages/G5-UR0b-resume-r6/package.json'
PACKAGE_SHA='0dc1abe9149759d25bbea9b37565094e943b23fd583f995194324818de44f920'
CONTROL=ROOT/'.aegis/results/G5-UR0b/control-review-trace-lifecycle-r1'
REQUEST_SHA='bfaa5b13023b0ebb37dc9d68e47947e9381a869d4df777e19248646784ae5c73'
REUSE_SHA='f76c8b5249efc4b1e3a908e2ee19cc94461cd5592f9856518b9073c289404135'
PLAN_SHA='dcb0f4b140d32b23c370331739379b11b32c424e740e2725d8d31e867e7d7ba7'
STATE=ROOT/'.aegis/results/G5-UR0b/resume-r6'
load=resume.load;sha=resume.sha;atomic=resume.atomic
def need(condition,message):
 if not condition:raise RuntimeError(message)
def bound_hash(ref,base=ROOT):
 p=(base/ref['path']).resolve()
 need(p.is_relative_to(ROOT) and p.is_file() and sha(p)==ref['sha256'],'changed/missing reference '+str(p));return p
def bound(ref,base=ROOT):return load(bound_hash(ref,base))
def request():
 return bound({'path':str((CONTROL/'continuation-request.json').relative_to(ROOT)),'sha256':REQUEST_SHA})
def acceptance():return bound({'path':str((CONTROL/'reuse-acceptance.json').relative_to(ROOT)),'sha256':REUSE_SHA})
def plan():return copy.deepcopy(bound({'path':str((CONTROL/'remaining-run-plan.json').relative_to(ROOT)),'sha256':PLAN_SHA})['rows'])
def validate_plan(rows):
 need(rows==plan() and len(rows)==35 and len({r['run_id'] for r in rows})==35 and sum(r['limit_seconds'] for r in rows)==5850,'changed/extra/reordered remaining35 plan')
 need(all(r.get('mode')=='correctness' and r['phase']=='separate' and r['limit_seconds']==90 for r in rows[:5]) and all(r['phase']=='main' and r['limit_seconds']==180 for r in rows[5:]),'wrong correctness/main order or limits')
 need({r['branch'] for r in rows[:5]}==set(repair.CASES)-{'lower_z_translucent_drag'},'lower-z cannot start again; five distinct cases required')
 old=bound(request()['historical_ledger_ref']);need(old['status']=='CLOSED','old window is not closed')
 for row in rows:
  prior=next(r for r in old['rows'] if r['run_id']==row['predecessor_run_id'])
  need(row['window_run_key']=='G5-UR0b-resume-r6/'+row['run_id'] and prior['status']==row['predecessor_status']=='MISSING' and prior['admission_status']==row['predecessor_admission_status']=='NOT_STARTED','predecessor row identity changed')
 return rows
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
def verify_profiles(packet):
 for name,digest in packet['compiler_file_sha256'].items():
  need((HERE/name).is_file() and sha(HERE/name)==digest,'current compiler differs from accepted current tuple: '+name)
 for name,digest in packet['repaired_worker_and_helpers'].items():
  need((ROOT/name).is_file() and sha(ROOT/name)==digest,'worker/helper profile differs: '+name)
 need(packet['repaired_worker_and_helpers']==repair.REPAIRED_INITIALIZATION_PROFILE,'JS/Python profile authority differs')
def admit_record(row,summary,chain,packet):
 trusted=[r for r in packet['records'] if r['run_id']==row['run_id']]
 need(len(trusted)==1 and trusted[0]==row,'foreign recording row/source group')
 a=summary.get('analysis',{})
 need(row['mode']=='OFFLINE_REVERIFICATION' and a.get('mode')==row['mode'] and a.get('input_source_tree_sha256')==row['recording_source_tree_sha256'],'recorded mode/source group differs; never fresh')
 need(summary.get('status')=='OBSERVED' and summary.get('schema_status')=='VALID' and summary.get('source_stable') is True and summary.get('artifacts_stable') is True and summary.get('artifact_identity')==row['accepted_artifacts'],'recorded artifact/source/schema differs')
 need(a.get('compiler_files')==packet['compiler_file_sha256'],'recorded compiler tuple differs')
 for item,key in zip(row['recorded_input_refs'],['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']):need(a.get(key)==item['sha256'],'recorded raw/input hash differs')
 need(summary.get('canonical_chain')==chain and summary.get('canonical_chain_ref')=={'path':'canonical-chain.json','sha256':row['proof_ref']['sha256']},'recorded proof/summary binding differs')
 need(all(chain.get(k)==v for k,v in row['counts'].items()),'recorded population differs')
 repair.require_canonical_chain(summary,require_positive=row['counts']['qualified_committed_count']>0)
 for initial in chain.get('initialization_chains',[]):need(initial['initialization_basis']['input_refs']=={k:a.get(k) for k in ['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']},'recorded init input basis differs')
 for key,name in [('verification_spec_ref','verification-spec.json'),('completion_contract_ref','completion-contract.json')]:
  path='.aegis/verification/G5-UR0b-initialization-v1/'+name;need(a.get(key)=={'path':path,'sha256':sha(ROOT/path)},'recorded Verification binding differs')
 if row['counts']['qualified_committed_count']:need(summary.get('clock')=='QUALIFIED','recorded canonical clock missing')
 return {'run_id':row['run_id'],'mode':row['mode'],'recording_source_tree_sha256':row['recording_source_tree_sha256'],'counts':row['counts'],'summary_ref':row['summary_ref'],'proof_ref':row['proof_ref'],'recorded_input_refs':row['recorded_input_refs']}
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
def correctness_verdict(entries,case):
 need(isinstance(entries,list) and len(entries)==4,'incomplete correctness pair')
 keys=[(e.get('toggle'),e.get('step')) for e in entries]
 need(set(keys)=={('OFF',0),('OFF',1),('ON',0),('ON',1)} and len(set(keys))==4,'duplicate/foreign correctness pair')
 need(all(e.get('case_id')==case and e.get('comparison',{}).get('status') in ['QUALIFIED','FAILED','MISSING'] and all(isinstance(e.get(k),str) and re.fullmatch(r'[0-9a-f]{64}',e[k]) for k in ['candidate_sha256','reference_snapshot_sha256']) for e in entries),'incomplete correctness comparison/hash')
 if all(e['comparison']['status']=='QUALIFIED' for e in entries):return 'QUALIFIED'
 for n in [0,1]:
  off=next(e for e in entries if e['toggle']=='OFF' and e['step']==n);on=next(e for e in entries if e['toggle']=='ON' and e['step']==n)
  if off['comparison']['status']!=on['comparison']['status'] or off['comparison']['status']!='QUALIFIED' and (off['candidate_sha256']!=on['candidate_sha256'] or off['reference_snapshot_sha256']!=on['reference_snapshot_sha256']):return 'BLOCKED_EVIDENCE'
 return 'PRODUCT_RED_IN_BOTH_OFF_ON'
def check_guard(row,outcome):
 g=outcome.get('guard');need(isinstance(g,dict),'guard missing/unknown')
 need(g.get('reason')=='completed' and g.get('returncode')==0 and g.get('cleanup_complete') is True and g.get('remaining_owned_pids')==[],'guard '+str(g.get('reason'))+' returncode='+str(g.get('returncode'))+' cleanup='+str(g.get('cleanup_complete'))+' last_stage='+str(g.get('last_stage')))
 need(g.get('timeout_s')==row['limit_seconds'] and g.get('rss_limit_kib')==3145728 and isinstance(g.get('peak_process_family_rss_kib'),(int,float)) and 0<=g['peak_process_family_rss_kib']<=3145728,'guard bounds/peak differ')
 need(resume.charge(g,row['limit_seconds'])<=row['limit_seconds'],'guard actual elapsed exceeds limit')
def require_fresh(row,outcome,proof):
 check_guard(row,outcome)
 summary=outcome.get('summary') or {};a=summary.get('analysis',{})
 need(a.get('mode')=='WORKER_COMPILE','fresh worker requires WORKER_COMPILE; '+str(outcome.get('compiler_error','no complete summary')))
 need(a.get('input_source_tree_sha256')==proof['source_tree_sha256'],'fresh worker differs from current source')
 resume.check_worker(row,outcome,proof)
 raw=Path(outcome['raw_directory']);run=load(raw/'worker-result.json')
 need(run['source_before']['source_tree_sha256']==proof['source_tree_sha256'] and run['source_after']['source_tree_sha256']==proof['source_tree_sha256'],'actual worker current source differs')
 if row.get('mode')=='correctness':
  verdict=correctness_verdict(run.get('correctness'),row['branch'])
  need(verdict==run.get('correctness_status')==outcome.get('correctness_status') and verdict in ['QUALIFIED','PRODUCT_RED_IN_BOTH_OFF_ON'],'correctness pair incomplete or instrumentation-only RED')
  for e in run['correctness']:
   label=e['toggle'].lower()+'-'+str(e['step']);candidate=raw/(label+'.candidate.rgba');reference=Path(e['comparison']['reference_snapshot']).resolve()
   need(reference.is_relative_to(raw.resolve()) and sha(candidate)==e['candidate_sha256']==e['source_sha256'] and sha(reference)==e['reference_snapshot_sha256'],'fresh correctness candidate/reference binding differs')
 else:
  need(summary.get('clock')=='QUALIFIED' and summary.get('terminal',{}).get('activeContacts')==0,'fresh main clock/terminal incomplete')
def require_phase(phase,rows,reused):
 packet=acceptance();expected=dict(packet['recorded_correctness_cases'][0],correctness_status='QUALIFIED')
 need(reused.get('status')=='ACCEPTED_SCOPED_17_PLUS_ONE_REUSE' and reused.get('acceptance_ref')==request()['accepted_reuse_ref'] and reused.get('prerequisites')==packet['accepted_prerequisites'] and reused.get('recorded_correctness_cases')==[expected],'exact17 plus distinct native-only recorded correctness missing')
 need(phase in ['separate','main'],'unapproved phase')
 if phase=='main':
  need(len(rows[:5])==5 and all(r['status']=='COMPLETED' and r.get('admission_status')=='ADMITTED' and r.get('correctness_status') in ['QUALIFIED','PRODUCT_RED_IN_BOTH_OFF_ON'] for r in rows[:5]),'main requires all five admitted fresh correctness cases')
  need({r.get('branch') for r in rows[:5]}|{expected['case_id']}==set(repair.CASES) and len({r.get('branch') for r in rows[:5]})==5,'recorded1+fresh5 cannot duplicate or omit a case')
class Window:
 def __init__(self,state,out,proof_sha,rows=None,history=None):
  self.state=Path(state);self.out=Path(out).resolve();self.rows=copy.deepcopy(plan() if rows is None else rows);validate_plan(self.rows)
  trusted=verify_history();need(history is None or history==trusted,'historical starts/debit cannot reset');self.history=trusted
  self.proof_sha=proof_sha;self.fd=None;self.data=None
 def __enter__(self):
  self.state.mkdir(parents=True,exist_ok=True);self.fd=os.open(self.state/'window.lock',os.O_CREAT|os.O_RDWR,0o600)
  try:
   try:fcntl.flock(self.fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
   except BlockingIOError:raise RuntimeError('r6 window concurrently owned')
   try:claim=os.open(self.state/'window.json',os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
   except FileExistsError:raise RuntimeError('r6 window already claimed/closed; relocation cannot restart it')
   os.fsync(claim);os.close(claim)
   self.data={'schema':'g5-ur0b-r6-window-v1','window_id':'G5-UR0b-resume-r6','status':'OPEN','package_sha256':PACKAGE_SHA,'proof_sha256':self.proof_sha,'output_directory':str(self.out),'historical_starts':34,'historical_charged_seconds':1264,'historical_rows':self.history['rows'],'historical_window_ref':self.history['window_ref'],'new_workers_max':35,'new_limit_seconds':5850,'original_cap_seconds':8370,'new_starts':0,'new_spent_seconds':0,'reserved_seconds':0,'charged_seconds':1264,'rows':self.rows};self.save();return self
  except BaseException:os.close(self.fd);self.fd=None;raise
 def save(self):atomic(self.state/'window.json',self.data)
 def row(self,key):
  found=[r for r in self.data['rows'] if r['run_id']==key];need(len(found)==1,'unknown/duplicate/unplanned run ID');return found[0]
 def begin(self,key):
  r=self.row(key);pending=next((x for x in self.data['rows'] if x['status']=='PLANNED'),None)
  need(self.data['status']=='OPEN' and pending is r and r['status']=='PLANNED','spent/claimed/out-of-order run cannot start')
  need(not self.data['reserved_seconds'] and not any(x['status']=='RUNNING' for x in self.data['rows']),'concurrency is one')
  need(self.data['new_starts']<35 and self.data['new_spent_seconds']+r['limit_seconds']<=5850 and self.data['charged_seconds']+r['limit_seconds']<=8370,'full reservation exceeds fixed budget')
  prior=self.data['rows'][:self.data['rows'].index(r)];need(all(x.get('admission_status')=='ADMITTED' for x in prior),'previous row not admitted')
  r.update(status='RUNNING',reservation_seconds=r['limit_seconds']);self.data['new_starts']+=1;self.data['reserved_seconds']=r['limit_seconds'];self.save()
 def finish(self,key,outcome):
  r=self.row(key);need(r['status']=='RUNNING','terminal outcome without reservation')
  try:cost=resume.charge(outcome.get('guard'),r['limit_seconds'])
  except RuntimeError as e:cost=r['limit_seconds'];outcome['accounting_error']=str(e)
  r.update(status=(outcome.get('guard') or {}).get('reason','UNKNOWN_GUARD').upper(),charged_seconds=cost,outcome=outcome,admission_status='UNCHECKED');self.data['new_spent_seconds']+=cost;self.data['reserved_seconds']=0;self.data['charged_seconds']=1264+self.data['new_spent_seconds'];self.save()
  if cost>r['limit_seconds'] or self.data['new_spent_seconds']>5850 or self.data['charged_seconds']>8370:
   self.close('actual guard/ledger limit exceeded; charge not clipped','FROZEN_VERIFICATION_FAILURE');raise RuntimeError('actual guard/ledger limit exceeded; charge not clipped')
  return cost
 def admit(self,key):
  r=self.row(key);need(self.data['status']=='OPEN' and r['status']=='COMPLETED','cannot admit a non-completed row');r['admission_status']='ADMITTED';r['correctness_status']=r['outcome'].get('correctness_status');self.save()
 def close(self,reason,status='BLOCKED_EVIDENCE'):
  if not self.data or self.data['status']!='OPEN':return
  for r in self.data['rows']:
   if r['status']=='RUNNING':r.update(status='INTERRUPTED_UNKNOWN_GUARD',charged_seconds=r['limit_seconds'],reason=reason);self.data['new_spent_seconds']+=r['limit_seconds']
   elif r['status']=='PLANNED':r.update(status='MISSING',admission_status='NOT_STARTED',reason=reason)
  self.data.update(status='CLOSED',terminal_status=status,reason=reason,reserved_seconds=0,charged_seconds=1264+self.data['new_spent_seconds']);self.save()
 def __exit__(self,kind,error,trace):
  try:self.close(str(error) if error else 'scope ended; no restart authorization')
  finally:
   if self.fd is not None:fcntl.flock(self.fd,fcntl.LOCK_UN);os.close(self.fd);self.fd=None
def authority():
 need(Path.cwd()==ROOT and subprocess.check_output(['git','remote','get-url','origin'],text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git' and subprocess.check_output(['git','branch','--show-current'],text=True).strip()=='codex/geometry-navigation-integration','wrong controlled repository/worktree/branch')
 need(sha(PACKAGE)==PACKAGE_SHA,'changed r6 package');p=load(PACKAGE)
 need(p['repository']=={'provider':'github','full_name':'Mostorm-Labs/axiom'} and p['status']=='AUTHORIZED_BOUNDED_REVERIFICATION_CONTINUATION','wrong repository/package authority')
 for k in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',p[k]['revision'],'HEAD'],check=True)
 for k in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','evidence_contract_ref','implementation_context_ref','run_plan_ref']:bound_hash(p[k],PACKAGE.parent)
 c=bound(p['execution_contract_ref'],PACKAGE.parent)['EXECUTION_CLOSURE_CONTRACT'];r=request()
 need(c['sampling_limits']==r['sampling_limits'] and c['authorized_source_paths']==r['source_scope'] and (ROOT/r['state_root']).resolve()==STATE.resolve(),'scope/state/limits authority differs')
 review=bound(r['review_result_ref']);need(review['status']=='ACCEPTED_TRACE_LIFECYCLE_REPAIR_AND_RECORDED_CORRECTNESS','independent review missing')
 validate_plan(plan());return p,c,verify_history()
def verify_proof(path,p,c,sdk64,sdk32):
 proof=load(path);current=source_identity.identity(ROOT)
 need(proof.get('status')=='MECHANISMS_VERIFIED' and proof.get('package_sha256')==PACKAGE_SHA and proof.get('source_tree_sha256')==current['source_tree_sha256'],'mechanism proof is not current source/package')
 lock=bound(p['authority_lock_ref'],PACKAGE.parent);baseline=bound(lock['accepted_source_identity_ref'])
 delta={n for n in baseline['files'].keys()|current['files'].keys() if baseline['files'].get(n)!=current['files'].get(n)}
 allowed=set(c['authorized_source_paths'])|{str(q.relative_to(ROOT)) for q in PACKAGE.parent.iterdir() if q.is_file()};need(delta<=allowed,'source delta exceeds new two paths and package')
 for ref in proof['files']:bound_hash(ref)
 packet=acceptance();verify_profiles(packet);need(proof.get('compiler_files')==packet['compiler_file_sha256'] and proof.get('accepted_reuse_ref')==request()['accepted_reuse_ref'],'proof compiler/reuse identity differs')
 old=bound(request()['accepted_artifact_provenance_ref']);need(proof['qualified_sdk_artifacts']==old['qualified_sdk_artifacts'] and proof['qualified_sdk32_artifacts']==old['qualified_sdk32_artifacts'],'SDK artifact authority differs')
 previous.verify_sdk(sdk64,proof['qualified_sdk_artifacts']);previous.verify_sdk(sdk32,proof['qualified_sdk32_artifacts']);return proof
def launch(row,sdk,out):
 target=out/'raw-runs'/row['run_id'];prefix=out/'guards'/row['run_id'];need(not target.exists() and not prefix.with_suffix('.worker.json').exists(),'refuse replacing observed outcome')
 mode=row.get('mode','fixture');branch=row.get('branch','navigation')
 if row['phase']=='main':mode='legacy' if row['case'].startswith('legacy') else 'main';branch=row['case'].removeprefix('immediate_')
 g=repair.observed_worker.run(['node',str(HERE/'worker.mjs'),str(Path(sdk).resolve()),str(target.resolve()),mode,branch,row.get('toggle','ON')],prefix,row['limit_seconds'],3145728)
 outcome={'guard':g,'raw_directory':str(target),'guard_record':str(prefix.with_suffix('.worker.json'))}
 worker_metadata(outcome);return outcome
def worker_metadata(outcome):
 raw=Path(outcome['raw_directory']);f=raw/'worker-result.json'
 if not f.is_file():return
 try:
  r=load(f);outcome.update(worker_status=r['status'],correctness_status=r.get('correctness_status'),worker_error=r.get('error'),correctness_results=[{k:e.get(k) for k in ['case_id','toggle','step','candidate_sha256','reference_snapshot_sha256']}|{'comparison_status':e.get('comparison',{}).get('status'),'mismatches':e.get('comparison',{}).get('interior_mismatched_pixels')} for e in r.get('correctness',[])])
 except (ValueError,KeyError) as e:outcome['worker_result_error']=str(e)
def compile_outcome(outcome):
 raw=Path(outcome['raw_directory'])
 if not (raw/'worker-result.json').is_file():return
 try:
  with (raw/'compiler.log').open('w') as log:subprocess.run(['node',str(HERE/'compile_run.mjs'),str(raw)],cwd=ROOT,check=True,timeout=120,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT)
  outcome['summary']=load(raw/'compiled-summary.json')
 except (subprocess.SubprocessError,OSError,ValueError) as e:outcome['compiler_error']=str(e)
def run(a):
 if (STATE/'window.json').exists() or (STATE/'admission-stop.json').exists():raise RuntimeError('r6 authorization already claimed or closed')
 try:
  p,c,history=authority();reused=accepted_reuse(acceptance());proof=verify_proof(a.proofs,p,c,a.sdk64.resolve(),a.sdk32.resolve());out=a.output.resolve();need(out.is_relative_to(ROOT/'out/ur0b/resume-r6'),'output outside r6 scope')
 except Exception as e:
  atomic(STATE/'admission-stop.json',{'status':'BLOCKED_EVIDENCE','reason':str(e),'new_workers':0,'remaining_ids':[r['run_id'] for r in plan()],'continue_execution':False});raise
 atomic(STATE/'reused-prerequisites.json',reused)
 with Window(STATE,out,sha(a.proofs),history=history) as window:
  window.data['source_tree_sha256']=proof['source_tree_sha256'];window.data['execution_revision']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip();window.save();out.mkdir(parents=True,exist_ok=True);current=None
  try:
   for phase in ['separate','main']:
    require_phase(phase,window.data['rows'],reused)
    for row in window.data['rows']:
     if row['phase']!=phase:continue
     current=row['run_id'];window.begin(current);sdk=a.sdk32 if phase=='main' and row['budget_mib']['navigation']==32 else a.sdk64
     print(json.dumps({'stage':'r6-worker-start','run_id':current,'reserved_seconds':row['limit_seconds'],'starts':window.data['new_starts']}),flush=True)
     outcome=launch(copy.deepcopy(row),sdk,out);cost=window.finish(current,outcome)
     check_guard(row,outcome);compile_outcome(outcome);require_fresh(row,outcome,proof);window.admit(current)
     print(json.dumps({'stage':'r6-worker-admitted','run_id':current,'charged_seconds':cost,'correctness_status':outcome.get('correctness_status')}),flush=True)
    atomic(out/(phase+'-result.json'),{'status':'FROZEN_PREREQUISITES_ADMITTED' if phase=='separate' else 'READY_FOR_CONTROL_REVIEW','product_RED_run_ids':[r['run_id'] for r in window.data['rows'] if r.get('correctness_status')=='PRODUCT_RED_IN_BOTH_OFF_ON'],'official_gate_pass':False})
   window.close('all35 rows accounted and admitted','READY_FOR_CONTROL_REVIEW')
  except BaseException as e:
   if current:window.row(current).update(admission_status='REJECTED',admission_reason=str(e))
   window.data['failed_run_id']=current;window.close(str(e),'FROZEN_VERIFICATION_FAILURE');window.save();atomic(out/'execution-stop.json',{'status':'FROZEN_VERIFICATION_FAILURE','run_id':current,'reason':str(e),'automatic_retry':False});raise
  finally:
   atomic(out/'worker-ledger.json',window.data['rows']);atomic(STATE/'execution-terminal.json',{'status':window.data.get('terminal_status'),'reason':window.data.get('reason'),'source_tree_sha256':proof['source_tree_sha256'],'window_ref':str(STATE/'window.json'),'official_gate_pass':False})
 return 0
def main():
 p=argparse.ArgumentParser();p.add_argument('--sdk64',type=Path,required=True);p.add_argument('--sdk32',type=Path,required=True);p.add_argument('--proofs',type=Path,required=True);p.add_argument('--output',type=Path,required=True);return run(p.parse_args())
if __name__=='__main__':raise SystemExit(main())
