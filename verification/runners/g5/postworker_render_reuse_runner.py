"""Frozen finite Render reuse windows. All identities checked before/after."""
from pathlib import Path
import hashlib,importlib.util,json,math,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[3]
RESULT=ROOT/'.aegis/results/G5-postworker-render-reuse-r1'
OUT=ROOT/'out/G5-postworker-render-reuse-r1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def freeze(q):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','runtime','sdk','cmake','verification/runners/g5','verification/tests'],cwd=ROOT).decode().split('\0')
 sources={n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and Path(n).suffix in ['.cpp','.hpp','.h','.mjs','.mts','.cmake','.json','.py','.txt']}
 roots={q['sdk']}if 'sdk'in q else{c['sdk']for c in q['cases']}
 return {'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'sources':sources,'SDKs':{r:{str(p.relative_to(r)):sha(p)for p in sorted(Path(r).rglob('*'))if p.is_file()}for r in roots},'fixture':sha(q['fixture']['path']),'image':sha(q['image'])if q.get('image')else None,'driver':sha(ROOT/q['driver'])}
def execute(path):
 q=read(path);plan=read(RESULT/'sampling-plan.json');ledger=read(RESULT/'sampling-ledger.json')
 if sha(ROOT/'.aegis/packages/G5-postworker-render-reuse-r1/package.json')!='ef56b7c84e7f3f5cb526b43554dc765fd819ae4ab78567680733fb8929ccf987':raise ValueError('package')
 if len(ledger['attempts'])>=7 or ledger['charged_seconds']+q['seconds']>2400 or any(a['status']in['STARTED','HARD_STOP']for a in ledger['attempts']):raise ValueError('finite guard')
 if q['id']not in plan['requests']or plan['requests'][q['id']]!=sha(path)or any(a['id']==q['id']for a in ledger['attempts']):raise ValueError('request identity')
 before=freeze(q)
 if before!=read(RESULT/'source-SDK-freeze.json')[q['role']]:raise ValueError('source SDK input drift before launch')
 write(RESULT/(q['id']+'-freeze.json'),{'request':sha(path),'basis':before})
 attempt={'id':q['id'],'status':'STARTED','started_unix':time.time(),'limit_seconds':q['seconds']};ledger['attempts'].append(attempt);write(RESULT/'sampling-ledger.json',ledger)
 spec=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 guard=module.run(['node',str(ROOT/q['driver']),str(Path(path).resolve()),str(OUT/'web'/q['id'])],OUT/'web'/q['id'],q['seconds'],4096*1024)
 after=freeze(q);drift=[k for k in before if before[k]!=after[k]]
 attempt.update(status='COMPLETED'if guard['reason']=='completed'and guard['cleanup_complete']and not drift else'HARD_STOP',guard=guard,identity_changes=drift,charged_seconds=math.ceil(guard['elapsed_s']))
 ledger['charged_seconds']+=attempt['charged_seconds'];write(RESULT/'sampling-ledger.json',ledger);print(json.dumps(attempt,indent=2));return 0 if attempt['status']=='COMPLETED'else 2
if __name__=='__main__':raise SystemExit(execute(sys.argv[1]))
