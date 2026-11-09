"""Two once-only correctness windows. No performance or additional smoke runs."""
from pathlib import Path
import hashlib,importlib.util,json,math,subprocess,sys,time,shutil
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
TASK='G5-UR2c-transient-display-retry-r1';RESULT=ROOT/'.aegis/results'/TASK;OUTPUT=ROOT/'out'/TASK
IDS=('UR2CDR-1-Q1','UR2CDR-1-N1');RESERVES=(360,600);HISTORY={'starts':84,'charged_seconds':5139,'cap_seconds':8370}
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def sha(path):
 with Path(path).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(path,value):
 path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value,indent=2)+'\n')
def identity(full=False):
 m=module('display_preflight',RESULT/'preflight.py');v=m.identity()
 if not full:v.pop('files')
 return v
def artifacts(root):return{str(p.relative_to(root)):sha(p)for p in sorted(root.rglob('*'))if p.is_file()}
def protected():
 package=ROOT/'.aegis/packages'/TASK;contract=json.loads((package/'execution-contract.json').read_text())['implementation']
 superseded=set(contract['allowed_existing_product_paths']+contract['allowed_tests'])
 bindings=json.loads((RESULT/'package-preflight.json').read_text())['bindings'];v={p:h for p,h in bindings.items()if p not in superseded}
 prior=module('display_frozen_oracles',HERE/'ur2a_handoff_runner.py')
 aliases={'node':Path(shutil.which('node')),'python3.12':Path(shutil.which('python3.12')),'oracle':ROOT/'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle','catalog':ROOT/'.aegis/results/G5-UR1d-physical-loss-repair-r1/reference-catalog.json'}
 for path,digest in prior.protected(aliases['catalog']).items():
  p=aliases.get(path,Path(path));p=p if p.is_absolute()else ROOT/p
  assert p.is_file(),str(p)
  v[str(p.relative_to(ROOT))if p.is_relative_to(ROOT)else str(p)]=digest
 for p,h in v.items():assert sha(ROOT/p)==h,p
 for p in package.iterdir():
  if p.is_file():v[str(p.relative_to(ROOT))]=sha(p)
 for name in ['verification/runners/g5/transient_display_retry.mjs','verification/runners/g5/transient_display_retry_contract.mjs','verification/runners/g5/transient_display_retry_runner.py']:
  v[name]=sha(ROOT/name)
 return v
def new_ledger():return{'schema':'g5-transient-display-two-windows-v1','planned_ids':list(IDS),'historical_starts':84,'historical_charged_seconds':5139,'original_cap_seconds':8370,'attempts':[],'remaining_status':'PENDING','cumulative_starts':84,'cumulative_charged_seconds':5139}
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
 ledger.update(cumulative_starts=84+len(ledger['attempts']),cumulative_charged_seconds=5139+sum(r['charged_seconds']for r in ledger['attempts']))
 if not good or ledger['cumulative_charged_seconds']>8370:ledger['remaining_status']='MISSING'
 elif len(ledger['attempts'])==2:ledger['remaining_status']='NONE'
 ledger['remaining_seconds']=8370-ledger['cumulative_charged_seconds'];return good
def freeze():
 assert not(RESULT/'local-web/sampling-ledger.json').exists();source=identity(True);write(RESULT/'source-final.json',source)
 roots=[OUTPUT/'normal-install/js',OUTPUT/'qualification-sdk'];sdks={str(p):artifacts(p)for p in roots};assert all(len(v)==36 for v in sdks.values())
 native=json.loads((RESULT/'native-summary.json').read_text());assert native['passed']==116 and native['registered']==117
 extras=[RESULT/n for n in ['native-artifacts.json','native-summary.json','other-checks.json','design-preflight.json','red-result.json']]+[OUTPUT/'identity-adapter.mjs']
 bound=protected()
 for p in extras:bound[str(p.relative_to(ROOT))]=sha(p)
 for root in ['build-web-normal','build-web-qualification']:
  p=OUTPUT/root/'CMakeCache.txt';bound[str(p.relative_to(ROOT))]=sha(p)
 write(RESULT/'worker-freeze.json',{'schema':'transient-display-workers-freeze-v1','source_tree_sha256':source['source_tree_sha256'],'protected':bound,'SDK':sdks,'ids':list(IDS),'history':HISTORY})
def execute():
 frozen=json.loads((RESULT/'worker-freeze.json').read_text());directory=RESULT/'local-web';directory.mkdir(exist_ok=True);ledger_path=directory/'sampling-ledger.json';assert not ledger_path.exists();ledger=new_ledger();write(ledger_path,ledger)
 guard=module('display_guard',HERE/'bounded_worker.py');prior=module('display_process_identity',HERE/'ur2a_handoff_runner.py')
 def stable():return identity()['source_tree_sha256']==frozen['source_tree_sha256']and all(sha(ROOT/p)==h for p,h in frozen['protected'].items())and all(artifacts(Path(p))==v for p,v in frozen['SDK'].items())
 for i,id in enumerate(IDS):
  if not stable():ledger.update(remaining_status='MISSING',identity_failure=True);write(ledger_path,ledger);break
  out=directory/id;assert not out.exists();reserve(ledger,id);write(ledger_path,ledger)
  sdk=OUTPUT/('qualification-sdk'if i==0 else'normal-install/js');cmd=['node',str(OUTPUT/'identity-adapter.mjs'),str(sdk),str(out),'qualification'if i==0 else'normal',str(ROOT/'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'),str(ROOT/'.aegis/results/G5-UR1d-physical-loss-repair-r1/reference-catalog.json')]
  original=guard.processes;owned={}
  def observed():
   table=original();prior.observe_owned_processes(table,cmd[1],str(out),owned);return table
  guard.processes=observed;g=None;started=time.monotonic()
  try:g=guard.run(cmd,out,RESERVES[i],3072*1024)
  except BaseException as error:g={'reason':'guard-exception','cleanup_complete':False,'error':repr(error)}
  finally:guard.processes=original;write(directory/(id+'-owned-processes.json'),{'owned_processes':list(owned.values())})
  wp=out/'worker-result.json';worker=json.loads(wp.read_text())if wp.is_file()else None
  good=complete(ledger,g,worker,stable());write(ledger_path,ledger);print(json.dumps(ledger['attempts'][-1]),flush=True)
  if not good:break
 write(directory/'summary.json',{'ledger':ledger,'unrun_ids':list(IDS[len(ledger['attempts']):]),'source_tree_sha256':frozen['source_tree_sha256']})
if __name__=='__main__':
 if sys.argv[1]=='--source-identity':print(json.dumps(identity()))
 elif sys.argv[1]=='--freeze':freeze()
 elif sys.argv[1]=='--execute':execute()
 else:raise ValueError('explicit operation required')
