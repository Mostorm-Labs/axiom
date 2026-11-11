"""Exact four-window convergence guard; historical evidence is read-only."""
from pathlib import Path
import hashlib, importlib.util, json, math, subprocess, sys
ROOT=Path(__file__).resolve().parents[3]
PACKAGE=ROOT/'.aegis/packages/G5-convergence-idle-up-r1'
RESULT=ROOT/'.aegis/results/G5-convergence-idle-up-r1'
LIMITS={'G5CV1-IDLE-CONTEXT':240,'G5CV1-LIFECYCLE-VERIFY':240,'G5CV1-UP-NORMAL':300,'G5CV1-UP-DIAGNOSTIC':360}
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb')as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def read(p):return json.loads(Path(p).read_text())
def write(p,v):Path(p).parent.mkdir(parents=True,exist_ok=True);Path(p).write_text(json.dumps(v,indent=2)+'\n')
def inventory(p):return {str(q.relative_to(p)):sha(q)for q in sorted(Path(p).rglob('*'))if q.is_file()}
def reserve(ledger,name):
 if sha(PACKAGE/'package.json')!='6504d8daadd9821ca264641db932bf59825472f0d5652c4a5e211071a57aa45c':raise ValueError('authority changed')
 if name not in LIMITS or len(ledger['attempts'])>=4 or any(x['id']==name or x['status']=='STARTED'for x in ledger['attempts']):raise ValueError('foreign/repeated/concurrent window')
 if ledger['charged_seconds']+LIMITS[name]>1140:raise ValueError('allowance exhausted')
 ledger['attempts'].append(dict(id=name,status='STARTED',limit_seconds=LIMITS[name],rss_mib=4096))
def inputs(req):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=ROOT).decode().split('\0')
 source={n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and n.startswith(('runtime/','sdk/','cmake/','verification/runners/g5/','verification/tests/'))and Path(n).suffix in('.cpp','.h','.hpp','.mjs','.cmake','.json','.py','.txt')}
 if sha(req['fixture']['path'])!=req['fixture']['sha256']:raise ValueError('fixture changed')
 return dict(source=source,SDKs={p:inventory(Path(p))for p in sorted({req['sdk']}|{x.get('sdk',req['sdk'])for x in req['cases']})},fixture=req['fixture'],driver=sha(ROOT/req['driver']),HEAD=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip())
def execute(request_path):
 req=read(request_path);lp=RESULT/'sampling-ledger.json';ledger=read(lp)if lp.exists()else dict(schema='g5-convergence-four-windows-v1',cap_seconds=1140,charged_seconds=0,attempts=[])
 reserve(ledger,req['id']);before=inputs(req);out=RESULT/'local-web'/req['id'];out.mkdir(parents=True,exist_ok=False)
 write(out/'freeze.json',dict(request=req,request_sha256=sha(request_path),inputs=before));write(lp,ledger)
 spec=importlib.util.spec_from_file_location('guard',Path(__file__).with_name('bounded_worker.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 guard=m.run(['node',str(ROOT/req['driver']),str(request_path),str(out)],out,LIMITS[req['id']],4096*1024)
 after=inputs(req);write(out/'inputs-after.json',after);worker=read(out/'worker-result.json')if(out/'worker-result.json').exists()else{}
 elapsed=guard.get('elapsed_s');charged=math.ceil(elapsed)if isinstance(elapsed,(int,float))and math.isfinite(elapsed)and 0<=elapsed<=LIMITS[req['id']]+5 else LIMITS[req['id']]
 ok=guard['reason']=='completed'and guard['cleanup_complete']and not guard['remaining_owned_pids']and before==after and worker.get('status')=='OBSERVED'and len(worker.get('rows',[]))==len(req['cases'])
 ledger['attempts'][-1].update(status='COMPLETED'if ok else'STOPPED',guard=guard,source_stable=before==after,charged_seconds=charged,worker_status=worker.get('status'));ledger['charged_seconds']+=charged;write(lp,ledger);print(json.dumps(ledger),flush=True);return 0 if ok else 1
if __name__=='__main__':raise SystemExit(execute(Path(sys.argv[1]).resolve()))
