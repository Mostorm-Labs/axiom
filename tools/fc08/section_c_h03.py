"""Bounded accepted H03 amendment: exact preflights, verification and preservation."""
import argparse,fnmatch,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
GIT='D:/Softwares/Git/cmd/git.exe'
PACKAGE='.aegis/packages/FC-08-SECTION-C-H03-v0.1'
PACKAGE_REV='6bab7b7ffde876317c11a8e69201da9505d6a476'
PACKAGE_SHA='afe81967b3032612920d2c4b9752ba2ced3c203a555f57149e2a131269ace4b0'
CURSOR='a68f37da7b261eade60e06209ac6ca8a1ac5faa8'
RESULTS='.aegis/results/FC-08-SECTION-C-H03-v0.1'
def git(*args):return subprocess.check_output([GIT,*args],cwd=ROOT).decode().strip()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def save(path,value):
 assert not path.exists(),str(path)
 path.write_bytes((json.dumps(value,indent=2,ensure_ascii=False)+'\n').encode('utf-8'))
def validate():
 assert git('remote','get-url','origin')=='https://github.com/Mostorm-Labs/axiom.git'
 assert git('symbolic-ref','HEAD')=='refs/heads/codex/fc08-section-history'
 subprocess.run([GIT,'merge-base','--is-ancestor',CURSOR,'HEAD'],cwd=ROOT,check=True)
 assert sha(ROOT/PACKAGE/'package.json')==PACKAGE_SHA
 package=read(ROOT/PACKAGE/'package.json');refs=[]
 def check(v):
  if isinstance(v,dict):
   if 'path' in v and 'sha256' in v:
    p=ROOT/v['path'];assert p.resolve().is_relative_to(ROOT)
    assert sha(p)==v['sha256'],str(p);refs.append(v)
   for value in v.values():check(value)
  elif isinstance(v,list):
   for value in v:check(value)
 for p in sorted((ROOT/PACKAGE).glob('*.json')):
  assert p.read_bytes()==subprocess.check_output([GIT,'show',PACKAGE_REV+':'+p.relative_to(ROOT).as_posix()],cwd=ROOT)
  check(read(p))
 assert package['product_behavior_changes_authorized'] is False
 assert read(ROOT/PACKAGE/'authority.lock.json')['status']=='Current_for_this_bounded_amendment'
 contract=read(ROOT/PACKAGE/'execution-contract.json')
 assert contract['continue_until_terminal_state'] and contract['required'] and contract['terminal_success']
 for obligation in read(ROOT/PACKAGE/'verification.lock.json')['obligations']:
  assert all(obligation.get(k) for k in ['requirement','oracle','acceptance','evidence_required'])
 overlay=read(ROOT/PACKAGE/'input-overlay.json')
 assert overlay['status']=='ACCEPTED_BY_USER_FOR_C_VERIFICATION' and len(overlay['variants'])==2
 for v in overlay['variants']:
  assert v['recipe']['N'] in [65534,65535] and v['recipe']['section_id']==90000
  assert v['expected_command_admitted']==(v['recipe']['N']==65534)
 # Preserve all product source and the original C scope, packages and results.
 assert not git('diff',CURSOR,'--','runtime/semantic/src','runtime/semantic/include','runtime/interaction','runtime/foundation','schema','semantic-sdk.lock.json','r1-full-skia-sdk.lock.json','.aegis/results/FC-08-SECTION-C','.aegis/packages/FC-08-SECTION-C-v0.1','tests/fc08/reference','.aegis/materialized')
 return package,refs,overlay

def audit():
 _,refs,_=validate();allowed=read(ROOT/PACKAGE/'execution-contract.json')['allowed_files']
 changed=set(git('diff','--name-only',CURSOR).splitlines()+git('ls-files','--others','--exclude-standard').splitlines())
 bad=[p for p in changed if not any(fnmatch.fnmatchcase(p,rule) for rule in allowed)];assert not bad,bad
 corpus=read(ROOT/'.aegis/materialized/FC-08/attempts/v0.3-control-remediation-02/corpus-manifest.json')
 assert len(corpus['variants'])==302 and len({r['case_id'] for r in corpus['variants']})==60
 for row in corpus['variants']:
  for v in [row['input'],row['expected'],*row.get('wire',[])]:
   if 'path' in v:assert sha(ROOT/v['path'])==v['sha256'],v['path']
 return refs

def ensure_historical_inputs():
 old=read(ROOT/'.aegis/results/FC-08-SECTION-C/qualified-001/budget-input-receipts.json')
 missing=False
 for r in old:
  name=r['variant_id'].split('/')[1]
  for v in r['wire_hashes']:
   if v['label'] not in ['F','independent-before-seed']:continue
   p=ROOT/'out/fc08-section-c/frozen-budgets'/(name+('.pb' if v['label']=='F' else '.seed.pb'))
   if not p.exists():missing=True
   else:assert sha(p)==v['sha256'],str(p)
 if missing:
  from tools.fc08.section_c_fixtures import materialize
  rebuilt=materialize()
  assert rebuilt==old
 return {'role':'original inputs retained solely as historical/context material and unchanged H04 inputs',
     'source_receipts':'.aegis/results/FC-08-SECTION-C/qualified-001/budget-input-receipts.json',
     'receipts_sha256':sha(ROOT/'.aegis/results/FC-08-SECTION-C/qualified-001/budget-input-receipts.json'),
     'reconstructed_from_frozen_writer':missing}

def observe_inputs(receipts):
 from tools.fc08.section_c_h03_inputs import OUTPUT
 observed=[]
 for r in receipts:
  name=r['variant_id'].split('/')[1]
  for v in r['wire_hashes']:
   suffix='.pb' if v['label']=='F' else '.reference-u.pb' if v['label']=='U' else '.seed.pb'
   p=OUTPUT/(name+suffix)
   assert sha(p)==v['sha256'] and p.stat().st_size==v['size'],str(p)
   observed.append({'variant_id':r['variant_id'],'role':v['label'],'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p),'bytes':p.stat().st_size})
 old=read(ROOT/'.aegis/results/FC-08-SECTION-C/qualified-001/budget-input-receipts.json')
 for r in old:
  if not r['variant_id'].startswith('FC08-H03/'):continue
  name=r['variant_id'].split('/')[1]
  for v in r['wire_hashes']:
   if v['label'] not in ['F','independent-before-seed']:continue
   p=ROOT/'out/fc08-section-c/frozen-budgets'/(name+('.pb' if v['label']=='F' else '.seed.pb'))
   assert sha(p)==v['sha256'],str(p)
   observed.append({'variant_id':r['variant_id'],'role':'historical-original-'+v['label'],'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p),'bytes':p.stat().st_size})
 return observed

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['preflight','audit','build']);p.add_argument('--repo',required=True);p.add_argument('--phase',required=True);p.add_argument('--targets',nargs='+',default=['axiom_fc08_section_history_tests','axiom_fc08_section_history_allocation_tests']);p.add_argument('--filter');a=p.parse_args()
 assert Path(a.repo).resolve()==ROOT
 out=ROOT/RESULTS/a.phase;out.mkdir(parents=True,exist_ok=False)
 if a.action in ['preflight','audit']:
  errors=[];refs=[]
  try:refs=audit() if a.action=='audit' else validate()[1]
  except (AssertionError,OSError,ValueError,subprocess.CalledProcessError) as e:errors.append(str(e))
  for name in (['PackageBindingPreflight','EvidenceContractPreflight'] if a.action=='preflight' else ['ScopePreservationAudit']):
   save(out/(name+'.json'),{'name':name,'status':'BLOCKED' if errors else 'PASS_H03_AMENDMENT','package_revision':PACKAGE_REV,'package_sha256':PACKAGE_SHA,'source_head':git('rev-parse','HEAD'),'cwd':str(ROOT),'cursor_reconciliation':'DESCENDANT_CURSOR','verified_refs':refs,'errors':errors})
  print(json.dumps({'action':a.action,'errors':errors}));return int(bool(errors))
 validate()
 sys.path.insert(0,str(ROOT))
 from tools.fc08.section_c_h03_inputs import materialize
 save(out/'historical-input-reuse.json',ensure_historical_inputs())
 receipts=materialize()
 save(out/'input-receipts.json',receipts)
 facts=read(ROOT/'out/fc08-section-c-h03/environment.json');env={**os.environ,**facts['environment'],'PYTHONUTF8':'1'}
 from tools.semantic.cmake_consumer import consumer_cmake_arguments
 build=ROOT/'out/fc08-section-c/build';journal=[]
 source_names=sorted(set(git('ls-files','runtime/semantic','runtime/interaction','tools/fc08').splitlines()+git('ls-files','--others','--exclude-standard','runtime/semantic','runtime/interaction','tools/fc08').splitlines()))
 sources=[{'path':n,'sha256':sha(ROOT/n)} for n in source_names if (ROOT/n).is_file()]
 def run(name,args):
  args=list(map(str,args));r=subprocess.run(args,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
  for suffix,value in [('stdout',r.stdout),('stderr',r.stderr)]: (out/(name+'.'+suffix+'.log')).write_bytes(value.encode('utf-8'))
  journal.append({'argv':args,'exit_code':r.returncode});print(name+': '+str(r.returncode),flush=True);return r.returncode
 code=run('configure',['cmake','-S',ROOT,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DBUILD_TESTING=ON','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_INK_PLAYGROUND=OFF','-DCANVAS_BUILD_RENDER=OFF','-DCANVAS_BUILD_RF01=OFF','-DCANVAS_SKIA_SDK_VARIANT=release','-DAXIOM_FC08_QUALIFICATION_HOOKS=OFF',*consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(env['AXIOM_PROTOC']))])
 if code==0:code=run('build',['cmake','--build',build,'--target',*a.targets,'--parallel','6'])
 if code==0:code=run('discover',['ctest','--test-dir',build,'--show-only=json-v1'])
 if code==0:
  import re
  tests=read(out/'discover.stdout.log')['tests'];names=[t['name'] for t in tests if Path(t.get('command',[''])[0]).stem in a.targets and (not a.filter or re.search(a.filter,t['name']))];assert names
  (out/'ctest-names.txt').write_bytes(('\n'.join(names)+'\n').encode())
  input_observations=observe_inputs(receipts)
  save(out/'input-observations-before.json',input_observations)
  code=run('ctest',['ctest','--test-dir',build,'--tests-from-file',out/'ctest-names.txt','--output-on-failure','--timeout','120','--output-junit',out/'ctest.xml'])
  assert observe_inputs(receipts)==input_observations
  save(out/'input-observations-after.json',observe_inputs(receipts))
 save(out/'invocations.json',{'source_head':git('rev-parse','HEAD'),'package_revision':PACKAGE_REV,'package_sha256':PACKAGE_SHA,'cwd':str(ROOT),'commands':journal,'exit_code':code,'environment':facts['environment'],'source_manifest':sources,'dirty_status':git('status','--porcelain')})
 return code
if __name__=='__main__':raise SystemExit(main())
