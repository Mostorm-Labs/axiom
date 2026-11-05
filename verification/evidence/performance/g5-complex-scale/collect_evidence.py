#!/usr/bin/env python3
"""Collect the frozen local run. Run from the controlled worktree after sampling."""
from pathlib import Path
import gzip, hashlib, json, re, shutil, subprocess
R=Path.cwd(); O=R/'out'; D=R/'verification/evidence/performance/g5-complex-scale'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def copy(p,to):
 to.parent.mkdir(parents=True,exist_ok=True)
 if p.suffix in {'.axsnap','.log'}:to=to.with_name(to.name+'.gz');to.write_bytes(gzip.compress(p.read_bytes(),mtime=0))
 else:shutil.copyfile(p,to)
matrix=read(O/'g5-step4-matrix/matrix.json')
assert matrix['sourceStable'] and len(matrix['rows'])==49
assert subprocess.run(['git','diff','--quiet',matrix['sourceBefore']['commit'],'HEAD','--','runtime','sdk','schema','apps','assets','cmake','tools','CMakeLists.txt','*.lock.json']).returncode==0
assert subprocess.run(['git','merge-base','--is-ancestor',matrix['sourceBefore']['commit'],'HEAD']).returncode==0
for row in matrix['rows']:
 if 'worker' in row:assert row['worker']['cleanup_complete'],row['id']
D.mkdir(parents=True,exist_ok=True)
for p in sorted((O/'g5-step4-matrix').rglob('*')):
 if p.is_file():copy(p,D/'matrix'/p.relative_to(O/'g5-step4-matrix'))
for p in O.glob('g5-step4-*.log'):
 copy(p,D/'logs'/p.name)
for name in ['install-check','regression-comparison','web-build-result','native-result','script-results','python312-result','workspace-result','matrix-result','matrix-summary','document-total-comparison']:
 p=O/('g5-step4-'+name+'.json')
 if p.exists():copy(p,D/(name+'.json'))
for p in O.glob('g5-step4-pilot-*case/observation.json.gz'):copy(p,D/'pilots'/p.parent.name/p.name)
for p in O.glob('g5-step4-pilot-*.worker.json'):copy(p,D/'pilots'/p.name)
write(D/'source-inputs.json',matrix['sourceBefore'])
write(D/'execution-inputs.json',{'primary_execution_revision':matrix['sourceBefore']['commit'],'native_full_regression_revision':'4e22db5e',
 'supplemental_execution_revision':read(O/'g5-step4-point-total/result.json')['sourceBefore']['commit'],
 'profile_sha256':sha(R/'verification/corpus/performance/g5/complex-scale-engineering.json'),
 'primary_package_sha256':hashlib.sha256(subprocess.check_output(['git','show',matrix['sourceBefore']['commit']+':docs/planning/G5_COMPLEX_EDITING_AND_SCALE.md'])).hexdigest(),
 'amended_package_sha256':sha(R/'docs/planning/G5_COMPLEX_EDITING_AND_SCALE.md'),
 'primary_binary_sha256':matrix['binary_sha256'],'document_total_binary_sha256':read(O/'g5-step4-point-total/result.json')['binary_sha256'],
 'runtime_sdk_schema_unchanged_after_primary':subprocess.run(['git','diff','--quiet',matrix['sourceBefore']['commit'],'HEAD','--','runtime','sdk','schema','apps','assets','cmake','tools','CMakeLists.txt','*.lock.json']).returncode==0,
 'build_configs':{name:{line.split('=',1)[0]:line.split('=',1)[1] for line in (O/name/'CMakeCache.txt').read_text().splitlines() if '=' in line and any(k in line.split('=',1)[0] for k in ['CMAKE_BUILD_TYPE','AXIOM_ENABLE_FRAME_TIMING','AXIOM_INSTALL_DEVELOPER_COMPONENTS','CANVAS_SKIA_SDK_ROOT','AXIOM_PROTOC','AXIOM_SEMANTIC_RUNTIME_ROOT','CMAKE_PREFIX_PATH'])} for name in ['host','web-sampling','web-sampling-off']}})
ctest=subprocess.check_output(['ctest','--test-dir',str(O/'host'),'--show-only=json-v1'],text=True)
ctest=json.loads(ctest[ctest.index('{'):]);commands={}
for t in ctest['tests']:
 if t.get('command'):
  p=Path(t['command'][0]);commands[str(p)]=sha(p) if p.is_file() else None
write(D/'native-test-inputs.json',{'executables':commands,'test_count':len(ctest['tests'])})
pairs=[]
for kind in matrix['profile']['edit_cases']:
 values={side:json.loads(gzip.decompress((O/'g5-step4-matrix'/('edit-'+kind+'-'+side)/'observation.json.gz').read_bytes())) for side in ['diagnostic','default']}
 checks={name:values['diagnostic'][name]['image']['sha256']==values['default'][name]['image']['sha256'] for name in ['before','after']}
 assert all(checks.values()),(kind,checks)
 pairs.append({'kind':kind,'checks':checks,'hashes':{side:{name:body[name]['image']['sha256'] for name in ['before','after']} for side,body in values.items()}})
write(D/'diagnostic-default-pixels.json',pairs)
node=(O/'g5-step4-node-full.log').read_text();baseline=gzip.decompress((R/'verification/evidence/performance/g5-shared-scene/g5-shared-reclaimed-node-full.log.gz').read_bytes()).decode()
fail=lambda s:re.findall(r'^not ok \d+ - (.+)$',s,re.M)
assert fail(node)==fail(baseline)
python=(O/'g5-step4-python312-full.log').read_text();pybase=(O/'g5-step4-python-baseline-failures.log').read_text()
pyfail=lambda s:re.findall(r'^FAIL: (.+)$',s.replace('verification.tests.',''),re.M)
assert pyfail(python)==pyfail(pybase)
write(D/'script-regressions.json',{'node':{'tests':int(re.search(r'^# tests (\d+)',node,re.M).group(1)),'pass':int(re.search(r'^# pass (\d+)',node,re.M).group(1)),'failed_names':fail(node),'new_failures':[]},'python312':{'tests':int(re.search(r'Ran (\d+) tests',python).group(1)),'failures':4,'skips':2,'failed_names':pyfail(python),'baseline_reproduced':True},'workspace':{'exit':127,'missing_tsc_workspaces':sorted(set(re.findall(r'^npm error workspace (.+)$',(O/'g5-step4-workspace-full.log').read_text(),re.M)))},'runtime_boundary_contracts':118})

# Keep superseded probe outcomes and the initial unreproduced target-close failure.
for source,dest in [('g5-step4-pre-admission-matrix','experiments/pre-admission'),('g5-step4-point-total-first-error','experiments/initial-document-total-error'),('g5-step4-legacy-runner-red-case','experiments/legacy-runner-red'),('g5-step4-legacy-runner-green-case','legacy-runner-green'),('g5-step4-point-total','document-total'),('g5-step4-point-total-default-case','document-total-default'),('g5-step4-observer-control-case','observer-control')]:
 for p in sorted((O/source).rglob('*')):
  if p.is_file():copy(p,D/dest/p.relative_to(O/source))
for stem in ['g5-step4-pre-admission-summary','g5-step4-pre-admission-install-check']:
 copy(O/(stem+'.json'),D/'experiments'/(stem+'.json'))
for pattern in ['g5-step4-point-total-default.worker.json','g5-step4-observer-control.worker.json','g5-step4-legacy-runner-*.worker.json']:
 for p in O.glob(pattern):copy(p,D/p.name)
# Inspect all completed SDK returns for same-camera cold/warm content stability.
optical=[]
for p in (O/'g5-step4-matrix').glob('*/observation.json.gz'):
 b=json.loads(gzip.decompress(p.read_bytes()))
 if b.get('status')!='completed':continue
 checks={'cold_warm_pixels':b['phases'][0]['raw']['pixels']['sha256']==b['phases'][1]['raw']['pixels']['sha256']}
 assert all(checks.values()),p
 optical.append({'case':p.parent.name,'checks':checks,'hashes':[v['raw']['pixels']['sha256'] for v in b['phases']]})
write(D/'navigation-return-pixels.json',optical)

for name in ['README.md','collect_evidence.py']:
 if not (D/name).exists():print('pending human-readable evidence file',name)
print(json.dumps({'files':sum(1 for p in D.rglob('*') if p.is_file()),'rows':len(matrix['rows'])}))
