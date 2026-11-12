"""Frozen CPUW3 finite browser windows; old ledgers and evidence stay immutable."""
from pathlib import Path
import hashlib,importlib.util,json,math,subprocess,sys
ROOT=Path(__file__).resolve().parents[3]
RESULT=ROOT/'.aegis/results/G5-CPUW3-complete-interaction-r1'
PACKAGE=ROOT/'.aegis/packages/G5-CPUW3-complete-interaction-r1/package.json'
PACKAGE_SHA='4e3fcf565784c6bea5eb44f3cdb3e2550273003644118895fbaf1f938a798fbf'
LIMITS={'G5CWC-D6':180,'G5CWC-PAIRS':300,'G5CWC-MIXED':240,'G5CWC-RECOVERY':180}
ROSTERS={'G5CWC-D6':['dpr4-focused-fault'],'G5CWC-PAIRS':['normalOFF1600','candidate1600','candidate7200burst','candidate7200paced'],'G5CWC-MIXED':['mixed-all-dpr2'],'G5CWC-RECOVERY':['mixed-dpr1','mixed-fault-dpr4']}
def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def write(path,value):Path(path).write_text(json.dumps(value,indent=2)+'\n')
def read(path):return json.loads(Path(path).read_text())
def inventory(root):return {str(p.relative_to(root)):sha(p) for p in sorted(Path(root).rglob('*')) if p.is_file()}
def freeze(request,path):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','runtime','sdk','cmake','verification/runners/g5','verification/tests'],cwd=ROOT).decode().split('\0')
 source={n:sha(ROOT/n) for n in sorted(set(names)) if n and (ROOT/n).is_file() and Path(n).suffix in ['.cpp','.hpp','.h','.mjs','.mts','.cmake','.json','.py','.txt']}
 return {'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'source':source,'SDKs':{c['sdk']:inventory(c['sdk']) for c in request['cases']},'request':sha(path),'driver':sha(ROOT/'verification/runners/g5/cpu_complete_web.mjs'),'fixture':sha(request['fixture']['path']),'image':sha(request['image'])}
def admit(ledger,request):
 name=request['id'];bucket=ledger['diagnostics' if name=='G5CWC-D6' else 'final']
 if name not in LIMITS or [c['id']for c in request['cases']]!=ROSTERS[name]:raise ValueError('unfrozen window roster')
 if len(bucket['attempts'])>=bucket['max_starts'] or any(a['id']==name or a['status']=='STARTED' for a in bucket['attempts']):raise ValueError('repeated/concurrent/exhausted window')
 if any(a.get('hard_stop')for a in ledger['final']['attempts']):raise ValueError('prior blocking final result')
 if bucket['charged_seconds']+LIMITS[name]>bucket['cap_seconds']:raise ValueError('finite seconds exhausted')
 if name!='G5CWC-D6' and name!=list(LIMITS)[1+len(bucket['attempts'])]:raise ValueError('final window order')
 bucket['attempts'].append({'id':name,'status':'STARTED','limit_seconds':LIMITS[name]});return bucket,bucket['attempts'][-1]
def execute(path):
 if sha(PACKAGE)!=PACKAGE_SHA:raise ValueError('package changed')
 request=read(path);ledgerPath=RESULT/'sampling-ledger.json';ledger=read(ledgerPath)
 if request['id']!='G5CWC-D6':
  plan=read(RESULT/'sampling-plan.json')
  if plan['requests'].get(request['id'])!=sha(path):raise ValueError('final request not frozen')
  expected=read(RESULT/'final-source-freeze.json')
  current=freeze(request,path)
  if expected['source']!=current['source'] or any(expected['SDKs'].get(k)!=v for k,v in current['SDKs'].items()):raise ValueError('source/SDK mismatch')
 before=freeze(request,path);bucket,attempt=admit(ledger,request);write(ledgerPath,ledger)
 label=request['id'];write(RESULT/(label+'-freeze.json'),before)
 spec=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');bounded=importlib.util.module_from_spec(spec);spec.loader.exec_module(bounded)
 prefix=RESULT/'local-web'/label
 guard=bounded.run(['node',str(ROOT/'verification/runners/g5/cpu_complete_web.mjs'),str(Path(path).resolve()),str(prefix)],prefix,LIMITS[label],4096*1024)
 after=freeze(request,path);changes=[k for k in before if before[k]!=after[k]]
 attempt.update(status='COMPLETED' if guard['reason']=='completed' and not changes else 'STOPPED',guard=guard,charged_seconds=math.ceil(guard['elapsed_s']),source_changed=changes)
 bucket['charged_seconds']+=attempt['charged_seconds'];attempt['hard_stop']=label!='G5CWC-D6' and attempt['status']!='COMPLETED';write(ledgerPath,ledger)
 if attempt['status']!='COMPLETED':raise RuntimeError('browser window failed: '+guard['reason']+' changed '+str(changes))
 return attempt
if __name__=='__main__':print(json.dumps(execute(sys.argv[1]),indent=2))
