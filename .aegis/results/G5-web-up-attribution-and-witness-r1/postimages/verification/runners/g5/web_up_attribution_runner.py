"""Four frozen windows; only new namespace, source/SDK guarded during sampling."""
from pathlib import Path
import hashlib, importlib.util, json, math, subprocess, sys
ROOT=Path(__file__).resolve().parents[3]
RESULT=ROOT/'.aegis/results/G5-web-up-attribution-and-witness-r1'
LIMITS={'G5WA1-BASELINE-WITNESS':240,'G5WA1-BASELINE-TAIL':300,'G5WA1-DIAGNOSTIC':360,'G5WA1-OVERHEAD':180,'G5WA1-DIAGNOSTIC-UP-ONLY-R1':236}
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb')as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def read(p):return json.loads(Path(p).read_text())
def write(p,v):Path(p).parent.mkdir(parents=True,exist_ok=True);Path(p).write_text(json.dumps(v,indent=2)+'\n')
def inventory(p):
 p=Path(p);return{str(q.relative_to(p)):sha(q)for q in sorted(p.rglob('*'))if q.is_file()}
def reserve(ledger,name):
 if name not in LIMITS or any(x['id']==name or x['status']=='STARTED'for x in ledger['attempts']):raise ValueError('foreign/repeated/concurrent window')
 if ledger['charged_seconds']+LIMITS[name]>1080:raise ValueError('allowance exhausted')
 ledger['attempts'].append(dict(id=name,status='STARTED',limit_seconds=LIMITS[name],rss_mib=4096))
def inputs(request):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=ROOT).decode().split('\0')
 source={n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and n.startswith(('runtime/','sdk/','cmake/','verification/runners/g5/','verification/tests/'))and Path(n).suffix in('.cpp','.h','.hpp','.mjs','.cmake','.json','.py','.txt')}
 if sha(request['fixture']['path'])!=request['fixture']['sha256']:raise ValueError('fixture bytes changed')
 baseline=read(ROOT/'.aegis/results/G5-erase-exact-canonical-r1/final-normal-artifacts.json')
 if inventory(baseline['root'])!=baseline['files']:raise ValueError('normal4319 SDK changed')
 return dict(source=source,SDK=inventory(request['sdk']),normal=baseline,fixture=request['fixture'])
def execute(request_path):
 req=read(request_path);ledger_path=RESULT/'sampling-ledger.json';ledger=read(ledger_path)if ledger_path.exists()else dict(schema='g5-wa-four-window-ledger-v1',charged_seconds=0,cap_seconds=1080,attempts=[])
 if req['id']=='G5WA1-DIAGNOSTIC-UP-ONLY-R1':
  authority=ROOT/'.aegis/results/G5-web-up-attribution-and-witness-control-r1/diagnostic-up-only-continuation-r1.json'
  if sha(authority)!='5e64af311124b40eab574f024edaf048587e8d2a071bca8d3763b5ca0dad6336':raise ValueError('diagnostic continuation identity')
 reserve(ledger,req['id']);before=inputs(req);out=RESULT/'local-web'/req['id'];out.mkdir(parents=True,exist_ok=False)
 write(out/'freeze.json',dict(request=req,request_sha256=sha(request_path),inputs=before));write(ledger_path,ledger)
 spec=importlib.util.spec_from_file_location('guard',Path(__file__).with_name('bounded_worker.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 guard=m.run(['node',str(Path(__file__).with_name('web_up_attribution.mjs')),str(request_path),str(out)],out,LIMITS[req['id']],4096*1024)
 after=inputs(req);write(out/'inputs-after.json',after);worker=read(out/'worker-result.json')if(out/'worker-result.json').exists()else{}
 elapsed=guard.get('elapsed_s');charged=math.ceil(elapsed)if isinstance(elapsed,(int,float))and math.isfinite(elapsed)and 0<=elapsed<=LIMITS[req['id']]+5 else LIMITS[req['id']]
 completed=guard['reason']=='completed'and guard['cleanup_complete']and not guard['remaining_owned_pids']and before==after and worker.get('status')=='OBSERVED'and len(worker.get('rows',[]))==len(req['cases'])
 ledger['attempts'][-1].update(status='COMPLETED'if completed else'STOPPED',guard=guard,source_stable=before==after,charged_seconds=charged,worker_status=worker.get('status'))
 ledger['charged_seconds']+=charged;write(ledger_path,ledger);print(json.dumps(ledger),flush=True);return 0 if completed else 1
if __name__=='__main__':raise SystemExit(execute(Path(sys.argv[1]).resolve()))
