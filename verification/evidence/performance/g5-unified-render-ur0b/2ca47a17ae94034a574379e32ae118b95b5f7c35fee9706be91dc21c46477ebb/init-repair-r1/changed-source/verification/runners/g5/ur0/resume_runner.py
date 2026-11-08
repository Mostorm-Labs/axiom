"""Exact r3 authority adapter and durable, single-use sampling window."""
import argparse,copy,fcntl,hashlib,json,math,os,subprocess,sys,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
sys.path.insert(0,str(HERE))
import repair_runner as repair
import source_identity
PACKAGE=ROOT/'.aegis/packages/G5-UR0b-resume-r3/package.json'
PACKAGE_SHA='bd6777505dddd474a111c52e1c477001a82ed11b5e445ce293f40368ba27f915'
STATE=ROOT/'.aegis/results/G5-UR0b/resume-r3'
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(path,value):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
 fd,name=tempfile.mkstemp(prefix='.'+path.name+'-',dir=path.parent)
 try:
  with os.fdopen(fd,'w') as stream:json.dump(value,stream,indent=2);stream.write('\n');stream.flush();os.fsync(stream.fileno())
  os.replace(name,path);directory=os.open(path.parent,os.O_RDONLY)
  try:os.fsync(directory)
  finally:os.close(directory)
 finally:
  if os.path.exists(name):os.unlink(name)
def new_plan():
 rows=[r for r in repair.plan() if r['phase']!='original']
 if len(rows)!=53 or sum(r['limit_seconds'] for r in rows)!=7470:raise RuntimeError('changed frozen r3 plan')
 return rows
def charge(guard,limit):
 if guard is None:return limit
 elapsed=guard.get('elapsed_s')
 if isinstance(elapsed,bool) or not isinstance(elapsed,(int,float)) or not math.isfinite(elapsed) or elapsed<0:raise RuntimeError('invalid guarded elapsed')
 return math.ceil(elapsed)
def verify_history(history):
 rows=history.get('rows',[])
 if len(rows)!=15 or len({r['run_key'] for r in rows})!=15:raise RuntimeError('historical preflight/debit rows omitted or duplicated')
 total=0
 for r in rows:
  ref=r.get('guard_ref',r.get('ledger_ref'));path=(ROOT/ref['path']).resolve()
  if not path.is_relative_to(ROOT) or sha(path)!=ref['sha256']:raise RuntimeError('changed historical guard/ledger')
  data=load(path)
  if 'guard_ref' in r:
   if not data['cleanup_complete'] or data['elapsed_s']!=r['elapsed_seconds']:raise RuntimeError('historical cleanup/elapsed mismatch')
   cost=charge(data,r['worker_limit_seconds'])
  else:
   previous=next(x for x in data if x['run_id']=='qualification-p2-A')
   if previous['status']!='CONTROL_STOP' or previous['limit_seconds']!=90:raise RuntimeError('missing guard history changed')
   cost=90
  if cost!=r['charged_seconds']:raise RuntimeError('historical charge differs from its guard')
  total+=cost
 if total!=597 or history.get('historical_charged_seconds')!=597:raise RuntimeError('historical debit reset')
 return total
class Window:
 def __init__(self,state,out,package_sha,proof_sha,rows):
  self.state=Path(state);self.out=Path(out).resolve();self.package_sha=package_sha;self.proof_sha=proof_sha;self.rows=copy.deepcopy(rows);self.fd=None;self.data=None
 def __enter__(self):
  self.state.mkdir(parents=True,exist_ok=True);self.fd=os.open(self.state/'window.lock',os.O_CREAT|os.O_RDWR,0o600)
  try:
   try:fcntl.flock(self.fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
   except BlockingIOError:raise RuntimeError('r3 window concurrently owned')
   try:claim=os.open(self.state/'window.json',os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
   except FileExistsError:raise RuntimeError('r3 window already claimed or closed; output relocation cannot restart it')
   os.close(claim)
   self.data={'schema':'g5-ur0b-r3-window-v1','window_id':'G5-UR0b-resume-r3','status':'OPEN','package_sha256':self.package_sha,'proof_sha256':self.proof_sha,'output_directory':str(self.out),
    'historical_charged_seconds':597,'new_limit_seconds':7470,'cumulative_limit_seconds':8067,'original_cap_seconds':8370,'new_spent_seconds':0,'reserved_seconds':0,'charged_seconds':597,'rows':self.rows}
   self.save();return self
  except BaseException:
   os.close(self.fd);self.fd=None;raise
 def save(self):atomic(self.state/'window.json',self.data)
 def row(self,key):
  found=[r for r in self.data['rows'] if r['run_id']==key]
  if len(found)!=1:raise RuntimeError('unknown/duplicate frozen run ID')
  return found[0]
 def begin(self,key):
  r=self.row(key)
  if self.data['status']!='OPEN' or r['status']!='PLANNED':raise RuntimeError('spent/claimed run cannot restart')
  if self.data['reserved_seconds'] or any(x['status']=='RUNNING' for x in self.data['rows']):raise RuntimeError('concurrency is one')
  if self.data['new_spent_seconds']+r['limit_seconds']>7470 or 597+self.data['new_spent_seconds']+r['limit_seconds']>8067:raise RuntimeError('overbudget reservation before spawn')
  r.update(status='RUNNING',reservation_seconds=r['limit_seconds']);self.data['reserved_seconds']=r['limit_seconds'];self.save()
 def finish(self,key,outcome):
  r=self.row(key)
  if r['status']!='RUNNING':raise RuntimeError('completion without reserved run')
  cost=charge(outcome.get('guard'),r['limit_seconds']);r.update(status=(outcome.get('guard') or {}).get('reason','UNKNOWN_GUARD').upper(),charged_seconds=cost,outcome=outcome)
  self.data['new_spent_seconds']+=cost;self.data['reserved_seconds']=0;self.data['charged_seconds']=597+self.data['new_spent_seconds'];self.save()
 def close(self,reason,status='BLOCKED_EVIDENCE'):
  if not self.data or self.data['status']!='OPEN':return
  for r in self.data['rows']:
   if r['status']=='RUNNING':
    r.update(status='INTERRUPTED_UNKNOWN_GUARD',charged_seconds=r['limit_seconds'],reason=reason);self.data['new_spent_seconds']+=r['limit_seconds']
   elif r['status']=='PLANNED':r.update(status='MISSING',reason=reason)
  self.data.update(status='CLOSED',terminal_status=status,reason=reason,reserved_seconds=0,charged_seconds=597+self.data['new_spent_seconds']);self.save()
 def __exit__(self,kind,error,trace):
  try:self.close(str(error) if error else 'window scope ended; no retry authorization')
  finally:
   if self.fd is not None:fcntl.flock(self.fd,fcntl.LOCK_UN);os.close(self.fd);self.fd=None
def bound(ref,base=ROOT):
 path=(base/ref['path']).resolve()
 if not path.is_relative_to(ROOT) or sha(path)!=ref['sha256']:raise RuntimeError('changed bound reference '+str(path))
 return load(path) if path.suffix=='.json' else path.read_text()
def authority():
 if sha(PACKAGE)!=PACKAGE_SHA:raise RuntimeError('changed r3 package')
 p=load(PACKAGE)
 if p['status']!='AUTHORIZED_BOUNDED_CONTINUATION' or p['repository']!={'provider':'github','full_name':'Mostorm-Labs/axiom'}:raise RuntimeError('wrong authorization/repository')
 if Path.cwd().resolve()!=ROOT or subprocess.check_output(['git','remote','get-url','origin'],text=True).strip()!='git@github.com:Mostorm-Labs/axiom.git' or subprocess.check_output(['git','branch','--show-current'],text=True).strip()!='codex/geometry-navigation-integration':raise RuntimeError('wrong controlled repository/worktree/branch')
 for key in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',p[key]['revision'],'HEAD'],check=True)
 values={key:bound(p[key],PACKAGE.parent) for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']}
 a=values['authority_lock_ref']
 for key,value in a.items():
  if key.endswith('_ref'):bound(value)
 for value in a['preserved_returns']:bound(value)
 history=bound(a['historical_budget_ref']);verify_history(history)
 c=values['execution_contract_ref']['EXECUTION_CLOSURE_CONTRACT'];limits=c['execution_limits']
 if limits['historical_debit_seconds']!=597 or limits['new_worker_seconds_max']!=7470 or limits['cumulative_seconds_max']!=8067 or limits['all_workers_cap_seconds']!=8370 or limits['new_worker_count_max']!=53 or limits['concurrency']!=1 or limits['process_family_rss_bytes']!=3221225472:raise RuntimeError('r3 limits changed')
 return p,c,history
def verify_proof(path,p,c,sdk64,sdk32):
 proof=load(path);current=source_identity.identity(ROOT)
 if proof.get('status')!='MECHANISMS_VERIFIED' or proof.get('source_tree_sha256')!=current['source_tree_sha256'] or proof.get('package_sha256')!=PACKAGE_SHA:raise RuntimeError('changed/missing exact mechanism proof')
 old=load(ROOT/'.aegis/results/G5-UR0b/p36-r2/result.json');accepted=load(Path(old['materialized_ref']['path'])/'source/full-source-identity.json')
 changed=[n for n in set(accepted['files'])|set(current['files']) if accepted['files'].get(n)!=current['files'].get(n)]
 package_files={str(x.relative_to(ROOT)) for x in PACKAGE.parent.iterdir() if x.is_file()}
 allowed=set(c['authorized_source_paths'])|package_files
 if any(n not in allowed for n in changed):raise RuntimeError('source delta exceeds authorized runner/package scope')
 if any(sha(ROOT/r['path'])!=r['sha256'] for r in proof['files']):raise RuntimeError('mechanism evidence changed')
 for sdk,key in [(sdk64,'qualified_sdk_artifacts'),(sdk32,'qualified_sdk32_artifacts')]:
  expected=proof.get(key)
  if not expected:raise RuntimeError('missing artifact tier proof '+key)
  actual={str(x.relative_to(sdk)):sha(x) for x in sdk.rglob('*') if x.is_file()}
  if actual!=expected:raise RuntimeError('artifact bytes differ from exact tier proof')
 return proof
def check_worker(row,outcome,proof):
 guard=outcome['guard'];s=outcome.get('summary')
 if guard.get('reason')!='completed' or guard.get('returncode')!=0 or repair.hard_stop(guard):raise RuntimeError('frozen guard/cleanup stop at '+row['run_id'])
 if outcome.get('worker_status')!='OBSERVED' or not s or not s['source_stable'] or not s['artifacts_stable']:raise RuntimeError('worker/source/artifact mechanism failure at '+row['run_id'])
 expected=proof['qualified_sdk32_artifacts'] if row['phase']=='main' and row['budget_mib']['navigation']==32 else proof['qualified_sdk_artifacts']
 if s['artifact_identity']!=expected:raise RuntimeError('worker artifact identity differs from its budget tier')
 if row.get('mode')!='correctness' and s['schema_status']!='VALID':raise RuntimeError('frozen metrics schema not valid')
 if row.get('toggle','ON')=='ON':
  if any(not s['actual_observations'][key] for key in ['raw_records','owners','leaves']):raise RuntimeError('actual owner/target links missing')
  repair.require_canonical_chain(s,require_positive=row['phase']=='diagnostic',require_metrics=row.get('mode')!='correctness');repair.require_bound_compilation(Path(outcome['raw_directory']),s,proof)
 if row.get('mode')=='correctness' and outcome.get('correctness_status') not in ['QUALIFIED','PRODUCT_RED_IN_BOTH_OFF_ON']:raise RuntimeError('independent correctness proof incomplete or instrumentation-only RED')
def phase_result(out,rows,which):
 if which in ['diagnostic','qualification']:
  pairs=repair.paired(rows,which);atomic(out/(which+'-pairs.json'),pairs)
  if any(not p.get(k) for p in pairs for k in ['same_artifact','same_document','same_terminal','same_product_error_codes']):raise RuntimeError('paired artifact/document/terminal/product outcome differs')
  decision={'status':'QUALIFIED','scope':'fresh diagnostic mechanism only; not overhead acceptance'}
  if which=='qualification':
   script="import{qualifyOverhead}from'./verification/runners/g5/ur0/fixture_plan.mjs';import fs from'node:fs';console.log(JSON.stringify(qualifyOverhead(JSON.parse(fs.readFileSync(process.argv[1])))))"
   decision=json.loads(subprocess.check_output(['node','--input-type=module','-e',script,str(out/(which+'-pairs.json'))],cwd=ROOT,text=True))
  atomic(out/('diagnostic-result.json' if which=='diagnostic' else 'overhead-qualification.json'),decision)
  if decision['status']!='QUALIFIED':raise RuntimeError('fresh overhead qualification '+decision['status']+'; no retry authorized')
 elif which=='separate':atomic(out/'separate-result.json',{'status':'QUALIFIED','product_RED_preserved':True})
 else:atomic(out/'main-result.json',{'status':'READY_FOR_CONTROL_REVIEW','performance_pass':False})
def run(a):
 if (STATE/'window.json').exists():raise RuntimeError('r3 window already claimed or closed; output relocation cannot restart it')
 p,c,history=authority();proof=verify_proof(a.proofs,p,c,a.sdk64.resolve(),a.sdk32.resolve());out=a.output.resolve()
 with Window(STATE,out,PACKAGE_SHA,sha(a.proofs),new_plan()) as window:
  window.data['historical_rows']=history['rows'];window.data['source_tree_sha256']=proof['source_tree_sha256'];window.save()
  out.mkdir(parents=True,exist_ok=True)
  try:
   for which in ['diagnostic','qualification','separate','main']:
    if which!='diagnostic':repair.require_qualified_diagnostic(out,proof)
    if which in ['separate','main'] and load(out/'overhead-qualification.json')['status']!='QUALIFIED':raise RuntimeError('overhead prerequisite not qualified')
    if which=='main' and load(out/'separate-result.json')['status']!='QUALIFIED':raise RuntimeError('separate prerequisite not qualified')
    for row in window.data['rows']:
     if row['phase']!=which:continue
     window.begin(row['run_id']);sdk=a.sdk32 if which=='main' and row['budget_mib']['navigation']==32 else a.sdk64
     outcome=repair.execute(copy.deepcopy(row),sdk,out);window.finish(row['run_id'],outcome);check_worker(row,outcome,proof)
    phase_result(out,window.data['rows'],which)
   window.close('all frozen rows accounted','READY_FOR_CONTROL_REVIEW')
  except BaseException as error:
   window.close(str(error),'FROZEN_VERIFICATION_FAILURE');atomic(out/'execution-stop.json',{'status':'FROZEN_VERIFICATION_FAILURE','reason':str(error),'primary_timing_adopted':False,'automatic_retry':False});raise
  finally:
   atomic(out/'worker-ledger.json',window.data['rows']);atomic(STATE/'execution-terminal.json',{'status':window.data.get('terminal_status'),'reason':window.data.get('reason'),'window_ref':str(STATE/'window.json'),'source_tree_sha256':proof['source_tree_sha256'],'official_gate_pass':False})
 return 0
def main():
 p=argparse.ArgumentParser();p.add_argument('--sdk64',type=Path,required=True);p.add_argument('--sdk32',type=Path,required=True);p.add_argument('--proofs',type=Path,required=True);p.add_argument('--output',type=Path,required=True);return run(p.parse_args())
if __name__=='__main__':raise SystemExit(main())
