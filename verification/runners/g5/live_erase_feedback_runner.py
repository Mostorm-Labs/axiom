"""Freeze sources, own bounded browser processes and charge actual ceil seconds."""
from pathlib import Path
import hashlib,json,subprocess,sys,time,math,importlib.util
ROOT=Path(__file__).resolve().parents[3];RES=ROOT/'.aegis/results/G5-live-erase-input-feedback-r1';OUT=ROOT/'out/G5-live-erase-input-feedback-r1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def freeze(q):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','runtime','sdk','cmake','verification/runners/g5','verification/tests'],cwd=ROOT).decode().split('\0')
 source={n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and Path(n).suffix in['.cpp','.hpp','.h','.mjs','.mts','.cmake','.json','.py','.txt']}
 return{'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'host_tree':source,'SDK':{str(p.relative_to(q['sdk'])):sha(p)for p in sorted(Path(q['sdk']).rglob('*'))if p.is_file()},'fixture':sha(q['fixture']['path']),'driver':sha(ROOT/'verification/runners/g5/live_erase_feedback_web.mjs'),'request':sha(OUT/'requests'/f"{q['id']}.json")}
def run(p):
 q=read(p);ledger=read(RES/'sampling-ledger.json')
 if len(ledger['attempts'])>=3 or ledger['charged_seconds']+q['seconds']>720 or any(a['status']=='STARTED'for a in ledger['attempts']):raise ValueError('finite cap')
 before=freeze(q);write(RES/(q['id']+'-freeze.json'),before)
 attempt={'id':q['id'],'status':'STARTED','limit_seconds':q['seconds'],'started_unix':time.time(),'request_sha256':sha(p)};ledger['attempts'].append(attempt);write(RES/'sampling-ledger.json',ledger)
 s=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
 guard=m.run(['node',str(ROOT/'verification/runners/g5/live_erase_feedback_web.mjs'),str(Path(p).resolve()),str(OUT/'web'/q['id'])],OUT/'web'/q['id'],q['seconds'],4096*1024)
 after=freeze(q);write(RES/(q['id']+'-after-freeze.json'),after);drift=[k for k in before if before[k]!=after[k]]
 attempt.update(status='COMPLETED'if guard['reason']=='completed'and guard['cleanup_complete']and not drift else'FAILED',guard=guard,identity_changes=drift,charged_seconds=math.ceil(guard['elapsed_s']))
 ledger['charged_seconds']+=attempt['charged_seconds'];write(RES/'sampling-ledger.json',ledger);print(json.dumps(attempt,indent=2));return 0 if attempt['status']=='COMPLETED'else 2
if __name__=='__main__':raise SystemExit(run(sys.argv[1]))
