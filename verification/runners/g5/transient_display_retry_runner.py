"""Two once-only correctness windows. No performance or additional smoke runs."""
from pathlib import Path
import hashlib,importlib.util,json,math,subprocess,sys,time,shutil
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
TASK='G5-UR2c-web-target-readback-r3';RESULT=ROOT/'.aegis/results'/TASK;OUTPUT=ROOT/'out'/TASK
IDS=('UR2CRB3-S1','UR2CRB3-N1');RESERVES=(120,600);HISTORY={'starts':88,'charged_seconds':5907,'cap_seconds':8370}
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def sha(path):
 with Path(path).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(path,value):
 path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value,indent=2)+'\n')
def identity(full=False):
 m=module('r3_source_identity',HERE/'ur0/source_identity.py')
 m.EXCLUDED=tuple(json.loads((ROOT/'.aegis/results/G5-UR2c-transient-display-handoff-r2/source-final.json').read_text())['excluded_self_outputs'])+(f'.aegis/results/{TASK}/',f'verification/evidence/performance/{TASK}/')
 v=m.identity(ROOT);v['identity_kind']='VERIFICATION_HARNESS_REPOSITORY';v['compiled_subject_source_tree_sha256']='d9bd0a26585a4419ea1a25701f5c42132cbb30e6fb149398a6fe117c1a4154dc'
 if not full:v.pop('files')
 return v
def artifacts(root):return{str(p.relative_to(root)):sha(p)for p in sorted(root.rglob('*'))if p.is_file()}
def protected():
 package=ROOT/'.aegis/packages'/TASK;contract=json.loads((package/'execution-contract.json').read_text())['implementation'];superseded=set(contract['allowed_existing_paths'])
 v={p:h for p,h in json.loads((RESULT/'package-preflight.json').read_text())['bindings'].items()if p not in superseded}
 parent=ROOT/'.aegis/results/G5-UR2c-transient-display-handoff-r2';old=json.loads((parent/'worker-freeze.json').read_text())
 for p,h in old['protected'].items():
  if p not in superseded:v[p]=h
 for row in json.loads((parent/'native-artifacts.json').read_text())['files']:v[row['path']]=row['sha256']
 for p,h in v.items():assert sha(ROOT/p)==h,p
 for p in package.iterdir():
  if p.is_file():v[str(p.relative_to(ROOT))]=sha(p)
 for name in contract['allowed_existing_paths']+contract['allowed_new_paths']:
  p=ROOT/name
  if p.is_file():v[name]=sha(p)
 for p in HERE.glob('webgl_target_readback_*.mjs'):v[str(p.relative_to(ROOT))]=sha(p)
 return v
def new_ledger():return{'schema':'g5-transient-display-two-windows-v1','planned_ids':list(IDS),'historical_starts':88,'historical_charged_seconds':5907,'original_cap_seconds':8370,'attempts':[],'remaining_status':'PENDING','cumulative_starts':88,'cumulative_charged_seconds':5907}
def reserve(ledger,id):
 i=len(ledger['attempts'])
 if ledger['remaining_status']!='PENDING'or i>=2 or id!=IDS[i]or any(r['status']=='STARTED'for r in ledger['attempts']):raise ValueError('unplanned/concurrent/repeated window')
 if ledger['cumulative_charged_seconds']+RESERVES[i]>HISTORY['cap_seconds']:raise ValueError('browser allowance exhausted')
 ledger['attempts'].append({'id':id,'status':'STARTED','reserved_seconds':RESERVES[i],'charged_seconds':RESERVES[i],'rss_limit_mib':3072})
def complete(ledger,guard,worker,stable):
 if not ledger['attempts']or ledger['attempts'][-1]['status']!='STARTED':raise ValueError('no started window')
 row=ledger['attempts'][-1];elapsed=guard.get('elapsed_s')if isinstance(guard,dict)else None
 trusted=isinstance(elapsed,(int,float))and not isinstance(elapsed,bool)and math.isfinite(elapsed)and 0<=elapsed<=row['reserved_seconds']+5 and guard.get('reason')in('completed','timeout','rss_limit','nonzero_exit')
 charged=math.ceil(elapsed)if trusted else row['reserved_seconds']
 good=bool(guard and guard.get('reason')=='completed'and guard.get('cleanup_complete')and stable and worker and worker.get('status')=='OBSERVED')
 row.update(status='COMPLETED'if good else'STOPPED',guard=guard,identity_stable=stable,worker_status=worker.get('status')if worker else'MISSING',charged_seconds=charged,elapsed_trusted=trusted)
 ledger.update(cumulative_starts=88+len(ledger['attempts']),cumulative_charged_seconds=5907+sum(r['charged_seconds']for r in ledger['attempts']))
 if not good or ledger['cumulative_charged_seconds']>8370:ledger['remaining_status']='MISSING'
 elif len(ledger['attempts'])==2:ledger['remaining_status']='NONE'
 ledger['remaining_seconds']=8370-ledger['cumulative_charged_seconds'];return good
def freeze():
 assert not(RESULT/'local-web/sampling-ledger.json').exists();source=identity(True);write(RESULT/'harness-source-final.json',source)
 parent=ROOT/'.aegis/results/G5-UR2c-transient-display-handoff-r2';subject=json.loads((ROOT/'.aegis/packages'/TASK/'authority.lock.json').read_text())['subject']
 for row in subject['protected_runtime_postimages']:assert sha(ROOT/row['path'])==row['sha256'],row['path']
 for row in subject['protected72SDKmembers']:assert sha(ROOT/row['path'])==row['sha256'],row['path']
 roots=[ROOT/'out/G5-UR2c-transient-display-handoff-r2/normal-install/js',ROOT/'out/G5-UR2c-transient-display-handoff-r2/qualification-sdk'];sdks={str(p):artifacts(p)for p in roots};assert all(len(v)==36 for v in sdks.values())
 bound=protected()
 for n in ['old-reader-lines.json','old-reader-red.log','other-checks.json','design-preflight.json','scope-integrity.json']:
  p=RESULT/n;bound[str(p.relative_to(ROOT))]=sha(p)
 p=OUTPUT/'identity-adapter.mjs';bound[str(p.relative_to(ROOT))]=sha(p)
 write(RESULT/'worker-freeze.json',{'schema':'web-target-readback-workers-freeze-v1','harness_source_tree_sha256':source['source_tree_sha256'],'compiled_subject':subject,'protected':bound,'SDK':sdks,'ids':list(IDS),'history':HISTORY,'native_rebuilt':False,'SDK_rebuilt':False})
def execute():
 frozen=json.loads((RESULT/'worker-freeze.json').read_text());directory=RESULT/'local-web';directory.mkdir(exist_ok=True);ledger_path=directory/'sampling-ledger.json';assert not ledger_path.exists();ledger=new_ledger();write(ledger_path,ledger)
 guard=module('display_guard',HERE/'bounded_worker.py');prior=module('display_process_identity',HERE/'ur2a_handoff_runner.py')
 def stable():return identity()['source_tree_sha256']==frozen['harness_source_tree_sha256']and all(sha(ROOT/p)==h for p,h in frozen['protected'].items())and all(artifacts(Path(p))==v for p,v in frozen['SDK'].items())
 for i,id in enumerate(IDS):
  if not stable():ledger.update(remaining_status='MISSING',identity_failure=True);write(ledger_path,ledger);break
  out=directory/id;assert not out.exists();reserve(ledger,id);write(ledger_path,ledger)
  if i==0:cmd=['node',str(HERE/'webgl_target_readback_selftest.mjs'),str(out),str(RESULT/'old-reader-lines.json')]
  else:cmd=['node',str(OUTPUT/'identity-adapter.mjs'),str(ROOT/'out/G5-UR2c-transient-display-handoff-r2/normal-install/js'),str(out),'normal',str(ROOT/'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'),str(ROOT/'.aegis/results/G5-UR1d-physical-loss-repair-r1/reference-catalog.json'),str(ROOT/'.aegis/results/G5-UR2c-transient-display-handoff-r2/trajectory-plan.json')]
  original=guard.processes;owned={}
  def observed():
   table=original();prior.observe_owned_processes(table,cmd[1],str(out),owned);return table
  guard.processes=observed;g=None;started=time.monotonic()
  try:g=guard.run(cmd,out,RESERVES[i],3072*1024)
  except BaseException as error:g={'reason':'guard-exception','cleanup_complete':False,'error':repr(error)}
  finally:guard.processes=original;write(directory/(id+'-owned-processes.json'),{'owned_processes':list(owned.values())})
  wp=out/'worker-result.json';worker=json.loads(wp.read_text())if wp.is_file()else None
  if i==0 and worker and worker.get('status')=='OBSERVED':
   if len(worker.get('cases',[]))!=8 or any(c.get('status')!='QUALIFIED'for c in worker['cases']):worker=dict(worker,status='SELFTEST_CLOSURE_FAILURE')
  good=complete(ledger,g,worker,stable());write(ledger_path,ledger);print(json.dumps(ledger['attempts'][-1]),flush=True)
  if not good:break
 write(directory/'summary.json',{'ledger':ledger,'unrun_ids':list(IDS[len(ledger['attempts']):]),'source_tree_sha256':frozen['harness_source_tree_sha256']})
if __name__=='__main__':
 if sys.argv[1]=='--source-identity':print(json.dumps(identity()))
 elif sys.argv[1]=='--freeze':freeze()
 elif sys.argv[1]=='--execute':execute()
 else:raise ValueError('explicit operation required')
