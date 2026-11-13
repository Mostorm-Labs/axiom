"""Finite CPUW5 windows. Own-process containment and immutable input freeze."""
from pathlib import Path
import hashlib,importlib.util,json,math,subprocess,sys
ROOT=Path(__file__).resolve().parents[3]
RESULT=ROOT/'.aegis/results/G5-CPUW5-zoomout-frontier-r3'
PACKAGE=ROOT/'.aegis/packages/G5-CPUW5-zoomout-frontier-r3/package.json'
SHA='e0ab78b6106a8864ad0b5e9e6822d02f4fea02fb58d13bcbe42980f219ca14f8'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def freeze(q,path):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','runtime','sdk','cmake','verification/runners/g5','verification/tests'],cwd=ROOT).decode().split('\0')
 sources={n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and Path(n).suffix in ['.cpp','.hpp','.h','.mjs','.mts','.cmake','.json','.py','.txt']}
 return {'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'sources':sources,'SDKs':{c['sdk']:{str(p.relative_to(c['sdk'])):sha(p)for p in sorted(Path(c['sdk']).rglob('*'))if p.is_file()}for c in q['cases']},'request':sha(path),'driver':sha(ROOT/'verification/runners/g5/cpu_zoomout_frontier_web.mjs'),'input':sha(ROOT/'verification/runners/g5/cpu_zoomout_frontier_contract.mjs'),'fixture':sha(q['fixture']['path']),'image':sha(q['image'])}
def execute(path):
 if sha(PACKAGE)!=SHA:raise ValueError('package mismatch')
 q=read(path);ledger=read(RESULT/'sampling-ledger.json');bucket=ledger[q['bucket']]
 limit=q['seconds'];maximum=240 if q['bucket']=='development'else 300
 if limit>maximum or len(bucket['attempts'])>=bucket['max_starts']or bucket['charged_seconds']+limit>bucket['cap_seconds']:raise ValueError('finite allowance exhausted')
 if any(a['id']==q['id']or a['status']=='STARTED'for b in ledger.values()for a in b['attempts']):raise ValueError('duplicate or concurrent window')
 if any(a.get('hard_stop') and a.get('formal_set')=='CPUW5-r3'for a in ledger['final']['attempts']):raise ValueError('sealed r3 final stopped')
 plan=read(RESULT/'sampling-plan.json')
 if plan.get('requests',{}).get(q['id'])!=sha(path):raise ValueError('unfrozen request')
 before=freeze(q,path);write(RESULT/(q['id']+'-freeze.json'),before)
 attempt={'id':q['id'],'status':'STARTED','seconds':limit,'formal_set':'CPUW5-r3','rows':[c['id']for c in q['cases']]};bucket['attempts'].append(attempt);write(RESULT/'sampling-ledger.json',ledger)
 spec=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 prefix=RESULT/'local-web'/q['id']
 guard=m.run(['node',str(ROOT/'verification/runners/g5/cpu_zoomout_frontier_web.mjs'),str(Path(path).resolve()),str(prefix)],prefix,limit,4096*1024)
 after=freeze(q,path);changes=[k for k in before if before[k]!=after[k]]
 attempt.update(status='COMPLETED'if guard['reason']=='completed'and not changes and guard['cleanup_complete']else'STOPPED',guard=guard,charged_seconds=math.ceil(guard['elapsed_s']),identity_changes=changes)
 attempt['hard_stop']=q['bucket']=='final'and attempt['status']!='COMPLETED';bucket['charged_seconds']+=attempt['charged_seconds'];write(RESULT/'sampling-ledger.json',ledger)
 print(json.dumps(attempt,indent=2))
 return 0 if attempt['status']=='COMPLETED'else 2
if __name__=='__main__':raise SystemExit(execute(sys.argv[1]))
