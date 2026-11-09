"""Frozen UR2a r2 two-window executor. No implicit build/browser/smoke/retry."""
import argparse, hashlib, importlib.util, json, math, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
TASK='G5-UR2a-current-scene-handoff-r2'
RESULT=ROOT/'.aegis/results'/TASK
IDS=('UR2A2-Q1','UR2A2-N1')
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb')as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def write(p,value):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(value,indent=2)+'\n')
def observe_owned_processes(table,driver,output,known):
 family={pid for pid,v in table.items()if driver in v['command']and output in v['command']}
 while True:
  children={pid for pid,v in table.items()if v['parent']in family}
  if children.issubset(family):break
  family.update(children)
 for pid in family:
  if pid in table:known[pid]={'pid':pid,**table[pid]}
def source_identity(full=False,root=ROOT):
 m=module('ur2a_source',HERE/'ur0/source_identity.py');m.EXCLUDED+=(f'.aegis/results/{TASK}/','verification/evidence/performance/g5-ur2a-current-scene-handoff-r2/')
 v=m.identity(root)
 if not full:v.pop('files')
 return v
def artifacts(root):return{str(p.relative_to(root)):sha(p)for p in sorted(Path(root).rglob('*'))if p.is_file()}
def protected(catalog):
 v={};pkg=ROOT/'.aegis/packages'/TASK
 for p in pkg.iterdir():
  if p.is_file():v[str(p.relative_to(ROOT))]=sha(p)
 oracle=ROOT/'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'
 assert sha(oracle)=='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105'
 v['oracle']=sha(oracle);v['catalog']=sha(catalog)
 for r in json.loads(Path(catalog).read_text())['resources']:
  assert sha(r['path'])==r['sha256'];v[r['path']]=r['sha256']
 old=json.loads((ROOT/'.aegis/results/G5-UR1c1-prepared-body-index/driver-before-workers.json').read_text())
 for p,h in old['files'].items():
  if p.startswith('/Applications/'):
   assert sha(p)==h;v[p]=h
 for name in ('node','python3.12'):v[name]=sha(Path(shutil.which(name)).resolve(strict=True))
 for base in ((ROOT/'verification/node_modules/playwright').resolve(),(ROOT/'verification/node_modules/playwright-core').resolve()):
  for p in sorted(base.rglob('*')):
   if p.is_file():v[str(p)]=sha(p)
 for p in sorted(HERE.rglob('*')):
  if p.is_file()and p.suffix in('.mjs','.py'):v[str(p.relative_to(ROOT))]=sha(p)
 return v
def new_ledger():return{'schema':'g5-ur2a-two-window-ledger-r2','planned_ids':list(IDS),'historical_starts':60,'historical_charged_seconds':2276,'original_cap_seconds':8370,'attempts':[],'remaining_status':'PENDING'}
def reserve(l,id):
 i=len(l['attempts'])
 if l['remaining_status']!='PENDING'or i>=2 or id!=IDS[i]or any(a['status']=='STARTED'for a in l['attempts']):raise ValueError('unplanned/concurrent/repeated window')
 l['attempts'].append({'id':id,'status':'STARTED','reserved_seconds':360,'charged_seconds':360,'rss_limit_mib':3072})
def complete(l,guard,worker):
 if not l['attempts']or l['attempts'][-1]['status']!='STARTED':raise ValueError('no reserved window')
 ok=guard and guard.get('reason')=='completed'and guard.get('cleanup_complete')and worker and worker.get('status')in('OBSERVED','PRODUCT_FAILURE_OBSERVED')
 row=l['attempts'][-1];row.update(status='COMPLETED'if ok else 'STOPPED',guard=guard,worker_status=worker.get('status')if worker else'MISSING',charged_seconds=math.ceil(guard['elapsed_s'])if guard and guard.get('cleanup_complete')else 360)
 l.update(cumulative_starts=60+len(l['attempts']),cumulative_charged_seconds=2276+sum(r['charged_seconds']for r in l['attempts']))
 if not ok or l['cumulative_charged_seconds']>8370:l['remaining_status']='MISSING'
 elif len(l['attempts'])==2:l['remaining_status']='NONE'
def freeze(q,n,catalog):
 assert not(RESULT/'local-functional/sampling-ledger.json').exists(),'window already reserved'
 identity=source_identity(True);write(RESULT/'source-identity.json',identity)
 write(RESULT/'worker-freeze.json',{'source_tree_sha256':identity['source_tree_sha256'],'protected':protected(catalog),'artifacts':{str(p.resolve()):artifacts(p)for p in(q,n)}})
def execute(q,n,oracle,catalog,output):
 assert output.resolve().is_relative_to(RESULT),'own output required'
 output.mkdir(parents=True,exist_ok=True);lp=output/'sampling-ledger.json';assert not lp.exists(),'no repeat'
 frozen=json.loads((RESULT/'worker-freeze.json').read_text());l=new_ledger();write(lp,l)
 guard=module('ur2a_guard',HERE/'bounded_worker.py')
 for i,id in enumerate(IDS):
  sdk=(q,n)[i]
  stable=source_identity()['source_tree_sha256']==frozen['source_tree_sha256']and protected(catalog)==frozen['protected']and all(artifacts(p)==frozen['artifacts'][str(p.resolve())]for p in(q,n))
  if not stable:l.update(remaining_status='MISSING',identity_failure=True);write(lp,l);break
  reserve(l,id);write(lp,l)
  cmd=['node',str(HERE/'ur2a_handoff.mjs'),str(sdk.resolve()),str((output/id).resolve()),('qualification','normal')[i],str(oracle.resolve()),str(catalog.resolve())]
  owned={};original_processes=guard.processes
  def observed_processes():
   table=original_processes();observe_owned_processes(table,cmd[1],cmd[3],owned);return table
  guard.processes=observed_processes
  try:g=guard.run(cmd,output/id,360,3072*1024)
  except BaseException:complete(l,None,None);write(lp,l);raise
  finally:
   guard.processes=original_processes
   write(output/(id+'-owned-processes.json'),{'owned_processes':list(owned.values()),'observation':'Existing guard process-table opportunities; identity-labelled observed family, no additional worker or altered guard.'})
  rp=output/id/'worker-result.json';w=json.loads(rp.read_text())if rp.exists()else None
  stable=source_identity()['source_tree_sha256']==frozen['source_tree_sha256']and protected(catalog)==frozen['protected']and artifacts(sdk)==frozen['artifacts'][str(sdk.resolve())]
  write(output/(id+'-identity-after.json'),{'stable':stable})
  if not stable:w={**(w or{}),'status':'HARD_FAILURE','identity_failure':True}
  complete(l,g,w);write(lp,l);print(json.dumps({'id':id,'guard':g['reason'],'status':w.get('status')if w else'MISSING','charged':l['attempts'][-1]['charged_seconds'],'remaining':l['remaining_status']}),flush=True)
  if l['remaining_status']=='MISSING':break
 write(output/'summary.json',{'ledger':l,'unrun_ids':list(IDS[len(l['attempts']):]),'source_tree_sha256':frozen['source_tree_sha256']})
def main():
 p=argparse.ArgumentParser();p.add_argument('--source-identity',action='store_true');p.add_argument('--freeze',action='store_true');p.add_argument('--execute',action='store_true')
 for n in('qualification-sdk','normal-sdk','oracle','catalog','output'):p.add_argument('--'+n,type=Path)
 a=p.parse_args()
 if a.source_identity:print(json.dumps(source_identity()))
 elif a.freeze and all((a.qualification_sdk,a.normal_sdk,a.catalog)):freeze(a.qualification_sdk,a.normal_sdk,a.catalog)
 elif a.execute and all((a.qualification_sdk,a.normal_sdk,a.oracle,a.catalog,a.output)):execute(a.qualification_sdk,a.normal_sdk,a.oracle,a.catalog,a.output)
 else:p.error('complete explicit operation required')
if __name__=='__main__':main()
