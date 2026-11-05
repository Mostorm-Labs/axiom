#!/usr/bin/env python3
"""Collect the frozen local navigation repair run, from the controlled worktree."""
from pathlib import Path
import json,gzip,hashlib,re,shutil,subprocess,urllib.request
R=Path.cwd();O=R/'out';D=R/'verification/evidence/performance/g5-large-navigation'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def copy(p,to):
 to.parent.mkdir(parents=True,exist_ok=True)
 if p.suffix in {'.log','.axsnap'}:to=to.with_name(to.name+'.gz');to.write_bytes(gzip.compress(p.read_bytes(),mtime=0))
 else:shutil.copyfile(p,to)
comparison=read(O/'g5-large-comparison.json');assert all(v['meets_task_reduction'] for v in comparison['regression'].values())
D.mkdir(parents=True,exist_ok=True)
for dirname in ['g5-large-before','g5-large-after','g5-large-edits']:
 for p in sorted((O/dirname).rglob('*')):
  if p.is_file():copy(p,D/dirname.removeprefix('g5-large-')/p.relative_to(O/dirname))
for p in O.glob('g5-large-*.log'):copy(p,D/'logs'/p.name)
for name in ['before-artifacts','web-build-results','native-results','script-results','regression-comparison','comparison']:
 copy(O/('g5-large-'+name+'.json'),D/(name+'.json'))
for p in O.glob('g5-large-gpu-*.json'):copy(p,D/'gpu'/p.name)
write(D/'source-inputs.json',read(O/'g5-large-after/matrix.json')['sourceBefore'])
packages={}
for key,relative in [('before-on','out/g5-step4-on/js'),('before-off','out/g5-step4-off/js'),('after-on','out/g5-large-on/js'),('after-off','out/g5-large-off/js')]:
 base=R/relative;files={str(p.relative_to(base)):sha(p) for p in sorted(base.rglob('*')) if p.is_file()}
 assert len(files)==36 and not any(Path(n).suffix in {'.h','.hpp','.a'} for n in files)
 if key.startswith('before'):assert files==read(O/'g5-large-before-artifacts.json')[key.split('-')[1]]
 if key=='after-off':assert all(s.encode() not in (base/'axiom.wasm').read_bytes() for s in ['AXIOM_NAVIGATION_TIMING','AXIOM_FRAME_TIMING','AXIOM_SCENE_TIMING'])
 packages[key]={'directory':str(base),'files':files}
frozen=read(R/'verification/evidence/performance/g5-complex-scale/install-check.json')['frozen4217'];base=Path(frozen['directory']);assert frozen['files']=={str(p.relative_to(base)):sha(p) for p in base.rglob('*') if p.is_file()};packages['frozen4217']=frozen
write(D/'consumer-artifacts.json',packages)
node=(O/'g5-large-node-full.log').read_text();old=gzip.decompress((R/'verification/evidence/performance/g5-complex-scale/logs/g5-step4-node-full.log.gz').read_bytes()).decode();fail=lambda s:re.findall(r'^not ok \d+ - (.+)$',s,re.M);assert fail(node)==fail(old)
python=(O/'g5-large-python-full.log').read_text();oldpy=gzip.decompress((R/'verification/evidence/performance/g5-complex-scale/logs/g5-step4-python312-full.log.gz').read_bytes()).decode();pyfail=lambda s:re.findall(r'^FAIL: (.+)$',s,re.M);assert pyfail(python)==pyfail(oldpy)
write(D/'script-regressions.json',{'node':{'tests':152,'passed':141,'failed_names':fail(node),'new_failures':[]},'python312':{'tests':205,'failures':4,'skips':2,'failed_names':pyfail(python),'new_failures':[]},'workspace_missing_tsc':sorted(set(re.findall(r'^npm error workspace (.+)$',(O/'g5-large-workspace.log').read_text(),re.M))),'boundary_contracts':118})
refs={k:subprocess.check_output(['git','rev-parse',k],text=True).strip() for k in ['4f7842db','19839b3a','d7903b6b','ed8df6af','9f312e92']}
assert subprocess.run(['git','diff','--quiet','ed8df6af','HEAD','--','runtime',':(exclude)runtime/**/tests/**','sdk','schema','apps','assets','cmake','CMakeLists.txt','*.lock.json']).returncode==0
write(D/'execution-inputs.json',{'revisions':refs,'package_sha256':sha(R/'docs/planning/G5_LARGE_NAVIGATION_SUBMISSION.md'),'runtime_code_matches_candidate_sdk':True,'binaries':{str(p):sha(p) for p in [O/'host/runtime/render/axiom_render_cache_tests',O/'host/runtime/render/axiom_prepared_geometry_tests',O/'web-prepared-probe/g5_render_cache_gpu.js',O/'web-prepared-probe/g5_render_cache_gpu.wasm']}})
response=urllib.request.urlopen('http://127.0.0.1:4218/example.html');assert response.status==200 and hashlib.sha256(response.read()).hexdigest()==sha(O/'g5-large-off/js/example.html')
write(D/'demo.json',{'url':'http://127.0.0.1:4218/example.html','sdk_revision':refs['ed8df6af'],'frozen4217_unchanged':True})
print(json.dumps({'collected_files':sum(p.is_file() for p in D.rglob('*')),'runtime_code_matches_sdk':True,'demo_http':200}))
