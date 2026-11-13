"""Frozen CL windows: own processes only, all drift and attempts preserved."""
from pathlib import Path
import hashlib,json,subprocess,importlib.util,time,math,sys
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'out/G5-canonical-convergence-latency-r1';RESULT=ROOT/'.aegis/results/G5-canonical-convergence-latency-r1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def freeze(q):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','runtime','sdk','cmake','verification/runners/g5','verification/tests'],cwd=ROOT).decode().split('\0')
 source={n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and Path(n).suffix in['.cpp','.hpp','.h','.mjs','.mts','.cmake','.json','.py','.txt']}
 sdks={q['sdk']}if'sdk'in q else{c['sdk']for c in q['cases']}
 return {'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'host_tree':source,'SDK':{s:{str(p.relative_to(s)):sha(p)for p in sorted(Path(s).rglob('*'))if p.is_file()}for s in sdks},'driver':sha(ROOT/q['driver']),'fixture':sha(q['fixture']['path']),'image':sha(q['image'])if q.get('image')else None}
def run(p):
 q=read(p);ledger=read(RESULT/'sampling-ledger.json');plan=read(RESULT/'sampling-plan.json')
 if len(ledger['attempts'])>=7 or ledger['charged_seconds']+q['seconds']>2400 or any(a['status']in['STARTED','HARD_STOP']for a in ledger['attempts']):raise ValueError('finite stop')
 if plan['requests'].get(q['id'])!=sha(p)or any(a['id']==q['id']for a in ledger['attempts']):raise ValueError('request identity')
 before=freeze(q);frozen=read(RESULT/(q['freeze']+'.json'))
 if before!=frozen:raise ValueError('before launch source/SDK/input drift')
 attempt={'id':q['id'],'status':'STARTED','limit_seconds':q['seconds'],'started_unix':time.time(),'basis':q['freeze']+'.json','request_sha256':sha(p)};ledger['attempts'].append(attempt);write(RESULT/'sampling-ledger.json',ledger)
 spec=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 guard=m.run(['node',str(ROOT/q['driver']),str(Path(p).resolve()),str(OUT/'web'/q['id'])],OUT/'web'/q['id'],q['seconds'],4096*1024)
 after=freeze(q);drift=[k for k in before if before[k]!=after[k]]
 attempt.update(status='COMPLETED'if guard['reason']=='completed'and guard['cleanup_complete']and not drift else'HARD_STOP',guard=guard,identity_changes=drift,charged_seconds=math.ceil(guard['elapsed_s']))
 ledger['charged_seconds']+=attempt['charged_seconds'];write(RESULT/'sampling-ledger.json',ledger);print(json.dumps(attempt,indent=2));return 0 if attempt['status']=='COMPLETED'else 2
if __name__=='__main__':raise SystemExit(run(sys.argv[1]))
