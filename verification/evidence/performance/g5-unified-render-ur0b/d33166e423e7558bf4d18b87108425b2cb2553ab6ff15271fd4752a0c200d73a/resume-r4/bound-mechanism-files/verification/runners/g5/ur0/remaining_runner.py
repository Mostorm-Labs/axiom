"""One r4 window for the 38 unstarted rows, with explicit accepted-record reuse."""
import argparse,copy,fcntl,hashlib,json,os,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE))
import resume_runner as resume
import repair_runner as repair
import source_identity
PACKAGE=ROOT/'.aegis/packages/G5-UR0b-resume-r4/package.json'
PACKAGE_SHA='2cadd6e12c73dc4006fd20691feca9402974a9f31fe705b2200c4654e020b569'
STATE=ROOT/'.aegis/results/G5-UR0b/resume-r4'
PLAN_SHA='1a2a64525c35403538bb55d458e8af0b7883415392dc710c60fe1a435baa9257'
HISTORY_SHA='d03f6302a48acd47e36e271dc94d4ff23f22f957c529c0beba8ca77e91c9aa6c'
REUSE_SHA='d2bb8fcddd1ec771e771de067a1f697fa27946b3cb949212f38c06613d966dbc'
load=resume.load;sha=resume.sha;atomic=resume.atomic
def bound(ref,base=ROOT):
 p=(base/ref['path']).resolve()
 if not p.is_relative_to(ROOT) or not p.is_file() or sha(p)!=ref['sha256']:raise RuntimeError('changed/missing bound reference '+str(p))
 return load(p) if p.suffix=='.json' else p.read_text()
def references(value):
 count=0
 if isinstance(value,dict):
  if 'path' in value and 'sha256' in value:bound(value);count+=1
  for item in value.values():count+=references(item)
 elif isinstance(value,list):
  for item in value:count+=references(item)
 return count
def control(name,digest):return bound({'path':str((PACKAGE.parent/name).relative_to(ROOT)),'sha256':digest})
def plan():return copy.deepcopy(control('run-plan.json',PLAN_SHA)['rows'])
def validate_plan(rows):
 trusted=plan()
 if rows!=trusted or len(rows)!=38 or sum(r['limit_seconds'] for r in rows)!=6120 or len({r['run_id'] for r in rows})!=38:raise RuntimeError('changed/extra/reordered frozen remaining plan')
 predecessor=bound(control('run-plan.json',PLAN_SHA)['predecessor_closed_window_ref'])
 if predecessor['status']!='CLOSED':raise RuntimeError('old window is not closed')
 for row in rows:
  old=next(r for r in predecessor['rows'] if r['run_id']==row['predecessor_run_id'])
  if old['status']!='MISSING' or 'outcome' in old:raise RuntimeError('attempt to repeat previously started row')
 return rows
def verify_history(history):
 trusted=control('historical-budget.json',HISTORY_SHA)
 if history!=trusted or len(history['rows'])!=30:raise RuntimeError('historical30 starts/debit/preflights omitted or changed')
 old=bound(history['previous_r3_history_ref']);resume.verify_history(old)
 if history['rows'][:15]!=old['rows']:raise RuntimeError('original preflight/missing-guard history reset')
 predecessor=bound(history['closed_r3_window_ref']);started=[r for r in predecessor['rows'] if 'outcome' in r];total=597
 if predecessor['status']!='CLOSED' or len(started)!=15:raise RuntimeError('old r3 execution state changed')
 for row,prior in zip(history['rows'][15:],started):
  guard=bound(row['guard_ref']);cost=resume.charge(guard,row['worker_limit_seconds'])
  if row['run_key']!='resume-r3/'+prior['run_id'] or guard!=prior['outcome']['guard'] or cost!=row['charged_seconds'] or guard['elapsed_s']!=row['elapsed_seconds'] or not guard['cleanup_complete']:raise RuntimeError('historical guard charge/binding differs')
  total+=cost
 if total!=1133 or history['historical_charged_seconds']!=1133:raise RuntimeError('historical debit reset')
 return total
def admit_record(row,summary,chain,inputs,artifacts):
 analysis=summary.get('analysis',{})
 if analysis.get('mode')!='OFFLINE_REVERIFICATION' or analysis.get('input_source_tree_sha256')!=inputs['recording_source_tree_sha256']:raise RuntimeError('recorded reuse mode/source changed; not a fresh output')
 if summary.get('artifact_identity')!=artifacts or not summary.get('source_stable') or not summary.get('artifacts_stable') or summary.get('schema_status')!='VALID':raise RuntimeError('recorded artifact/source/schema differs')
 if analysis.get('compiler_files')!=inputs['compiler_file_sha256']:raise RuntimeError('recorded proof compiler differs')
 for item,key in zip(row['recorded_input_refs'],['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']):
  if analysis.get(key)!=item['sha256']:raise RuntimeError('recorded input binding differs')
 if summary.get('canonical_chain')!=chain or summary.get('canonical_chain_ref')!={'path':'canonical-chain.json','sha256':row['accepted_v2_proof_ref']['sha256']}:raise RuntimeError('accepted v2 proof/summary differs')
 if any(chain.get(k)!=v for k,v in row['expected_counts'].items()) or chain.get('status')!=row['status']:raise RuntimeError('accepted proof population changed')
 repair.require_canonical_chain(summary,require_positive=row['expected_counts']['qualified_committed_count']>0)
 for key,name in [('verification_spec_ref','verification-spec.json'),('completion_contract_ref','completion-contract.json')]:
  path='.aegis/verification/G5-UR0b-initialization-v1/'+name
  if analysis.get(key)!={'path':path,'sha256':sha(ROOT/path)}:raise RuntimeError('recorded Verification binding differs')
 if chain['qualified_committed_count'] and summary.get('clock')!='QUALIFIED':raise RuntimeError('recorded canonical clock is unqualified')
 return {'run_id':row['run_id'],'mode':analysis['mode'],'status':'ACCEPTED_RECORDED_REUSE','proof_ref':row['accepted_v2_proof_ref'],'summary_ref':row['accepted_derived_summary_ref'],'recorded_input_refs':row['recorded_input_refs'],'counts':row['expected_counts']}
def accepted_reuse(inputs):
 trusted=control('reuse-inputs.json',REUSE_SHA)
 if inputs!=trusted or inputs.get('status')!='APPROVED_EXACT_RECORDED_REUSE':raise RuntimeError('changed/foreign/unaccepted recorded reuse packet')
 references(inputs)
 review=bound(inputs['independent_review_ref']);accept=bound(inputs['reuse_acceptance_ref'])
 if review.get('status')!='ACCEPTED_OFFLINE_INITIALIZATION_REPAIR_AND_RECORDED_COHORT_REUSE' or accept.get('status')!='ACCEPTED_EXACT_RECORDED_DIAGNOSTIC_AND_OVERHEAD_REUSE':raise RuntimeError('recorded prerequisite lacks independent acceptance')
 if any(sha(HERE/name)!=digest for name,digest in inputs['compiler_file_sha256'].items()):raise RuntimeError('current compiler differs from accepted reuse compiler')
 artifacts=bound(inputs['existing_artifact_provenance_ref'])['tiers']['64-192']['artifacts']
 records=[admit_record(r,bound(r['accepted_derived_summary_ref']),bound(r['accepted_v2_proof_ref']),inputs,artifacts) for r in inputs['records']]
 if len(records)!=15 or len({r['run_id'] for r in records})!=15:raise RuntimeError('recorded prerequisite population changed')
 overhead=bound(inputs['original_overhead_decision_ref'])
 if overhead.get('status')!='QUALIFIED':raise RuntimeError('accepted original overhead is not qualified')
 return {'status':'ACCEPTED_RECORDED_REUSE','reuse_packet_sha256':REUSE_SHA,'review_ref':inputs['independent_review_ref'],'acceptance_ref':inputs['reuse_acceptance_ref'],'recording_source_tree_sha256':inputs['recording_source_tree_sha256'],'accepted_compiler_source_tree_sha256':inputs['accepted_compiler_source_tree_sha256'],'records':records,'diagnostic_qualified':True,'overhead_qualified':True,'realtime60_qualified':True,'scope':'Exact accepted recordings; never a new WORKER_COMPILE or new cohort'}
def verify_sdk(directory,expected):
 actual={str(p.relative_to(directory)):sha(p) for p in directory.rglob('*') if p.is_file()}
 if not expected or actual!=expected:raise RuntimeError('exact SDK artifact bytes differ')
def authority():
 if sha(PACKAGE)!=PACKAGE_SHA:raise RuntimeError('changed r4 package')
 p=load(PACKAGE)
 if p.get('status')!='AUTHORIZED_REMAINING_BASELINE_CONTINUATION' or p['repository']!={'provider':'github','full_name':'Mostorm-Labs/axiom'}:raise RuntimeError('wrong authority/repository')
 if Path.cwd()!=ROOT or subprocess.check_output(['git','remote','get-url','origin'],text=True).strip()!='git@github.com:Mostorm-Labs/axiom.git' or subprocess.check_output(['git','branch','--show-current'],text=True).strip()!='codex/geometry-navigation-integration':raise RuntimeError('wrong controlled repository/worktree/branch')
 for key in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',p[key]['revision'],'HEAD'],check=True)
 values={k:bound(p[k],PACKAGE.parent) for k in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','evidence_contract_ref']};bound(p['implementation_context_ref'],PACKAGE.parent)
 for v in values.values():references(v)
 c=values['execution_contract_ref']['EXECUTION_CLOSURE_CONTRACT'];limits=c['execution_limits']
 if any(limits[k]!=v for k,v in {'historical_started_workers':30,'historical_charged_seconds':1133,'new_workers_max':38,'new_worker_limit_seconds_sum':6120,'cumulative_planned_seconds':7253,'original_all_workers_cap_seconds':8370,'concurrency':1,'process_family_rss_bytes':3221225472}.items()):raise RuntimeError('frozen execution limits changed')
 history=bound(c['historical_budget_ref']);verify_history(history);validate_plan(plan())
 return p,c,history,bound(c['reuse_inputs_ref'])
def verify_proof(path,p,c,sdk64,sdk32):
 proof=load(path);current=source_identity.identity(ROOT)
 if proof.get('status')!='MECHANISMS_VERIFIED' or proof.get('package_sha256')!=PACKAGE_SHA or proof.get('source_tree_sha256')!=current['source_tree_sha256']:raise RuntimeError('mechanism proof is not current source/package')
 lock=bound(p['authority_lock_ref'],PACKAGE.parent);accepted=bound(lock['accepted_source_identity_ref'])
 delta={n for n in set(accepted['files'])|set(current['files']) if accepted['files'].get(n)!=current['files'].get(n)}
 allowed=set(c['authorized_source_paths'])|{str(n.relative_to(ROOT)) for n in PACKAGE.parent.iterdir() if n.is_file()}
 if not delta<=allowed:raise RuntimeError('source delta exceeds two new runner/test plus package files')
 for item in proof['files']:bound_hash(item)
 verify_sdk(sdk64,proof['qualified_sdk_artifacts']);verify_sdk(sdk32,proof['qualified_sdk32_artifacts'])
 return proof
def bound_hash(ref):
 p=(ROOT/ref['path']).resolve()
 if not p.is_relative_to(ROOT) or not p.is_file() or sha(p)!=ref['sha256']:raise RuntimeError('changed mechanism proof file '+str(p))
def require_phase(phase,rows,reused):
 if reused.get('status')!='ACCEPTED_RECORDED_REUSE' or any(reused.get(k) is not True for k in ['diagnostic_qualified','overhead_qualified','realtime60_qualified']):raise RuntimeError('explicit accepted reuse prerequisite missing')
 if phase not in ['separate','main']:raise RuntimeError('unapproved phase; no new diagnostic/cohort')
 separate=[r for r in rows if r['phase']=='separate']
 if phase=='main' and (len(separate)!=8 or any(r['status']!='COMPLETED' or r.get('admission_status')!='QUALIFIED' for r in separate)):raise RuntimeError('main requires all eight separate prerequisites')
def require_fresh(row,outcome,proof):
 summary=outcome.get('summary') or {};analysis=summary.get('analysis',{})
 if analysis.get('mode')!='WORKER_COMPILE':raise RuntimeError('new worker requires fresh WORKER_COMPILE, never recorded reuse')
 if analysis.get('input_source_tree_sha256')!=proof['source_tree_sha256']:raise RuntimeError('fresh worker source differs from current mechanism proof')
 resume.check_worker(row,outcome,proof)
class Window:
 def __init__(self,state,out,proof_sha,rows=None,history=None):
  self.state=Path(state);self.out=Path(out).resolve();self.rows=copy.deepcopy(plan() if rows is None else rows);validate_plan(self.rows)
  trusted=control('historical-budget.json',HISTORY_SHA)['rows'];self.history=copy.deepcopy(trusted if history is None else history)
  if self.history!=trusted:raise RuntimeError('historical rows cannot reset in new window')
  self.proof_sha=proof_sha;self.fd=None;self.data=None
 def __enter__(self):
  self.state.mkdir(parents=True,exist_ok=True);self.fd=os.open(self.state/'window.lock',os.O_CREAT|os.O_RDWR,0o600)
  try:
   try:fcntl.flock(self.fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
   except BlockingIOError:raise RuntimeError('r4 window concurrently owned')
   try:claim=os.open(self.state/'window.json',os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
   except FileExistsError:raise RuntimeError('r4 window already claimed/closed; relocation cannot restart it')
   os.close(claim);self.data={'schema':'g5-ur0b-r4-window-v1','window_id':'G5-UR0b-resume-r4','status':'OPEN','package_sha256':PACKAGE_SHA,'proof_sha256':self.proof_sha,'output_directory':str(self.out),'historical_rows':self.history,'historical_charged_seconds':1133,'new_limit_seconds':6120,'original_cap_seconds':8370,'new_spent_seconds':0,'reserved_seconds':0,'charged_seconds':1133,'rows':self.rows};self.save();return self
  except BaseException:os.close(self.fd);self.fd=None;raise
 def save(self):atomic(self.state/'window.json',self.data)
 def row(self,key):
  found=[r for r in self.data['rows'] if r['run_id']==key]
  if len(found)!=1:raise RuntimeError('unknown/duplicate/unapproved run ID')
  return found[0]
 def begin(self,key):
  r=self.row(key)
  if self.data['status']!='OPEN' or r['status']!='PLANNED':raise RuntimeError('claimed/spent run cannot restart')
  if self.data['reserved_seconds'] or any(x['status']=='RUNNING' for x in self.data['rows']):raise RuntimeError('concurrency is one')
  if self.data['new_spent_seconds']+r['limit_seconds']>6120 or 1133+self.data['new_spent_seconds']+r['limit_seconds']>8370:raise RuntimeError('overbudget full reservation before spawn')
  r.update(status='RUNNING',reservation_seconds=r['limit_seconds']);self.data['reserved_seconds']=r['limit_seconds'];self.save()
 def finish(self,key,outcome):
  r=self.row(key)
  if r['status']!='RUNNING':raise RuntimeError('terminal outcome without reserved run')
  cost=resume.charge(outcome.get('guard'),r['limit_seconds']);r.update(status=(outcome.get('guard') or {}).get('reason','UNKNOWN_GUARD').upper(),charged_seconds=cost,outcome=outcome,admission_status='UNCHECKED');self.data['new_spent_seconds']+=cost;self.data['charged_seconds']=1133+self.data['new_spent_seconds'];self.data['reserved_seconds']=0;self.save();return cost
 def admit(self,key):self.row(key)['admission_status']='QUALIFIED';self.save()
 def close(self,reason,status='BLOCKED_EVIDENCE'):
  if not self.data or self.data['status']!='OPEN':return
  for r in self.data['rows']:
   if r['status']=='RUNNING':r.update(status='INTERRUPTED_UNKNOWN_GUARD',charged_seconds=r['limit_seconds'],reason=reason);self.data['new_spent_seconds']+=r['limit_seconds']
   elif r['status']=='PLANNED':r.update(status='MISSING',reason=reason)
  self.data.update(status='CLOSED',terminal_status=status,reason=reason,reserved_seconds=0,charged_seconds=1133+self.data['new_spent_seconds']);self.save()
 def __exit__(self,kind,error,trace):
  try:self.close(str(error) if error else 'scope ended; no restart authorization')
  finally:
   if self.fd is not None:fcntl.flock(self.fd,fcntl.LOCK_UN);os.close(self.fd);self.fd=None
def run(a):
 if (STATE/'window.json').exists() or (STATE/'admission-stop.json').exists():raise RuntimeError('r4 authorization already claimed or closed')
 try:
  p,c,history,inputs=authority();reused=accepted_reuse(inputs);proof=verify_proof(a.proofs,p,c,a.sdk64.resolve(),a.sdk32.resolve());out=a.output.resolve()
  if not out.is_relative_to(ROOT/'out/ur0b/resume-r4'):raise RuntimeError('output outside frozen r4 output scope')
 except Exception as error:
  atomic(STATE/'admission-stop.json',{'status':'BLOCKED_EVIDENCE','reason':str(error),'new_workers':0,'remaining_ids':[r['run_id'] for r in plan()],'continue_execution':False});raise
 atomic(STATE/'reused-prerequisites.json',reused)
 with Window(STATE,out,sha(a.proofs),history=history['rows']) as window:
  window.data['source_tree_sha256']=proof['source_tree_sha256'];window.data['accepted_reuse_ref']={'path':str((STATE/'reused-prerequisites.json').relative_to(ROOT)),'sha256':sha(STATE/'reused-prerequisites.json')};window.save();out.mkdir(parents=True,exist_ok=True);current=None
  try:
   for phase in ['separate','main']:
    require_phase(phase,window.data['rows'],reused)
    for row in window.data['rows']:
     if row['phase']!=phase:continue
     current=row['run_id'];window.begin(current);sdk=a.sdk32 if phase=='main' and row['budget_mib']['navigation']==32 else a.sdk64
     outcome=repair.execute(copy.deepcopy(row),sdk,out);cost=window.finish(current,outcome)
     if cost>row['limit_seconds'] or window.data['new_spent_seconds']>6120 or window.data['charged_seconds']>8370:raise RuntimeError('actual guard/ledger limit exceeded; charge is not clipped')
     require_fresh(row,outcome,proof);window.admit(current)
    resume.phase_result(out,window.data['rows'],phase)
   window.close('all remaining rows accounted and admitted','READY_FOR_CONTROL_REVIEW')
  except BaseException as error:
   if current:
    window.row(current)['admission_status']='REJECTED';window.row(current)['admission_reason']=str(error)
   window.data['failed_run_id']=current;window.close(str(error),'FROZEN_VERIFICATION_FAILURE');atomic(out/'execution-stop.json',{'status':'FROZEN_VERIFICATION_FAILURE','run_id':current,'reason':str(error),'automatic_retry':False});raise
  finally:
   atomic(out/'worker-ledger.json',window.data['rows']);atomic(STATE/'execution-terminal.json',{'status':window.data.get('terminal_status'),'reason':window.data.get('reason'),'source_tree_sha256':proof['source_tree_sha256'],'window_ref':str(STATE/'window.json'),'official_gate_pass':False})
 return 0
def main():
 p=argparse.ArgumentParser();p.add_argument('--sdk64',type=Path,required=True);p.add_argument('--sdk32',type=Path,required=True);p.add_argument('--proofs',type=Path,required=True);p.add_argument('--output',type=Path,required=True);return run(p.parse_args())
if __name__=='__main__':raise SystemExit(main())
