"""Bind completed P36 mechanism evidence before the sole diagnostic pair."""
import hashlib,importlib.util,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];OUT=ROOT/'out/ur0b';RESULT=Path(__file__).resolve().parent
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
spec=importlib.util.spec_from_file_location('p36_identity',ROOT/'verification/runners/g5/ur0/source_identity.py');identity=importlib.util.module_from_spec(spec);spec.loader.exec_module(identity)
source=identity.identity(ROOT);write(RESULT/'source-before-sampling.json',source)
def git(*args):return subprocess.check_output(['git','-C',str(ROOT),*args]).decode()
scope=load(ROOT/'.aegis/packages/G5-UR0b/scope-contract.json')
tracked=[s for s in git('diff','--name-only','-z','HEAD').split('\0') if s]
untracked=[s for s in git('ls-files','--others','--exclude-standard','-z').split('\0') if s]
violations=[p for p in tracked if p not in scope['allowed_existing_source_files']]
violations += [p for p in untracked if p not in scope['allowed_new_source_files'] and not any(p.startswith(prefix) for prefix in scope['allowed_new_roots'])]
assert not violations,violations
assert subprocess.run(['git','diff','--check'],cwd=ROOT,capture_output=True).returncode==0
write(RESULT/'source-scope-audit.json',{'status':'QUALIFIED_SCOPE_ONLY','tracked_changed':tracked,'untracked_source':[p for p in untracked if not p.startswith(identity.EXCLUDED)],'violations':violations,'public_api_change':False,'product_optimization':False})
for name,pattern in [('p36-ctest-required.log',r'0 tests failed out of (22)'),('p36-node-required-final.log',r'# pass (81)\n# fail 0'),('p36-python-required.log',r'Ran (26) tests[^\0]*\nOK'),('p36-runtime-boundaries-r2.log',r'passed \((120) contract files\)')]:
 assert re.search(pattern,(OUT/name).read_text()),name
assert load(OUT/'p36-normal-qualification-boundary.json')['status']=='QUALIFIED'
assert load(OUT/'p36-fixture-semantic-oracle.json')['status']=='QUALIFIED'
assert load(OUT/'p36-normal-consumer-smoke-binding.json')['status']=='IDENTICAL_CONSUMER_PAYLOAD'
assert load(OUT/'p36-r1/pixel-mechanism/final-result.json')['status']=='REFERENCE_RENDERED'
preserved=load(ROOT/'.aegis/results/G5-UR0b/output-sha256.json')
for name,value in preserved.items():
 if isinstance(value,str):assert sha(ROOT/'.aegis/results/G5-UR0b'/name)==value,name
 else:assert sha(value['path'])==value['sha256'],name
def artifacts(root):return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file()}
qual=OUT/'p36-r1/build-qualified-64-192/sdk/web'
files=[p for p in OUT.glob('p36-*') if p.is_file()]
files += [OUT/'p36-r1/pixel-mechanism/final-result.json',ROOT/'out/host/runtime/render/axiom_ur0_fixture_oracle']
for variant in ['qualified','normal']:
 build=OUT/f'p36-r1/build-{variant}-64-192'
 files += [build/name for name in ['CMakeCache.txt','ur0b-build-request.json','configure.log','build.log']]
 files += [p for p in (build/'sdk/web').rglob('*') if p.is_file()]
write(RESULT/'mechanism-proofs.json',{'schema':'g5-ur0b-p36-mechanism-proof-v1','status':'MECHANISMS_VERIFIED',
 'scope':'fresh affected native/Node/Python/boundary, actual build/install and cache-free offline oracles; no browser overhead qualification claimed',
 'source_revision':source['revision'],'source_tree_sha256':source['source_tree_sha256'],
 'files':[{'path':str(p.relative_to(ROOT)),'sha256':sha(p)} for p in sorted(set(files))],
 'qualified_sdk_artifacts':artifacts(qual),'fixture_snapshot_sha256':sha(OUT/'fixture-mechanism-data/initial.axsnap'),
 'original_result_preserved':True,'combined_Web_correctness':'NOT_RUN; separate workers remain gated by qualification','performance_pass':False})
print(json.dumps({'status':'MECHANISMS_VERIFIED','source_tree_sha256':source['source_tree_sha256'],'proof':str(RESULT/'mechanism-proofs.json')}))
