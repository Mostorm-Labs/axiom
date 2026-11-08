"""Cold evidence compilation only; never starts a browser or mutates authority."""
import hashlib,json,re,subprocess,sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'out/ur0b/resume-r3'
RESULT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import resume_runner as runner
import source_identity

def load(path):return json.loads(Path(path).read_text())
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def write(path,value):runner.atomic(path,value)
def ref(path):
 path=Path(path);return {'path':str(path.relative_to(ROOT)),'sha256':sha(path)}
def files(path):return {str(p.relative_to(path)):sha(p) for p in sorted(path.rglob('*')) if p.is_file()}

package,contract,history=runner.authority()
assert not (RESULT/'window.json').exists(),'proof must precede the only sampling window'
source=source_identity.identity(ROOT)
old_result=load(ROOT/'.aegis/results/G5-UR0b/p36-r2/result.json')
accepted=load(Path(old_result['materialized_ref']['path'])/'source/full-source-identity.json')
delta={n:{'before':accepted['files'].get(n),'after':source['files'].get(n)} for n in sorted(set(accepted['files'])|set(source['files'])) if accepted['files'].get(n)!=source['files'].get(n)}
allowed=set(contract['authorized_source_paths'])|{str(p.relative_to(ROOT)) for p in runner.PACKAGE.parent.iterdir() if p.is_file()}
assert set(delta)<=allowed,sorted(set(delta)-allowed)
write(RESULT/'full-source-identity.json',source)
write(RESULT/'source-scope-audit.json',{'status':'AUTHORIZED_SCOPE_VERIFIED','accepted_source_tree_sha256':accepted['source_tree_sha256'],'source_tree_sha256':source['source_tree_sha256'],'changed_paths':list(delta),'files':delta,'runtime_sdk_build_inputs_changed':False,'public_api_impact':'NONE'})

inherited_path=ROOT/'.aegis/results/G5-UR0b/p36-r1/mechanism-proofs.json'
inherited=load(inherited_path)
for entry in inherited['files']:assert sha(ROOT/entry['path'])==entry['sha256'],entry['path']
old_build_source=load(ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/inherited['source_tree_sha256']/'p36-r1/source/full-source-identity.json')
def build_input(name):return name.startswith(('runtime/','sdk/','schema/','cmake/','tools/','assets/')) or name=='CMakeLists.txt' or ('/' not in name and name.endswith('.lock.json'))
projection={n:source['files'].get(n) for n in sorted(set(source['files'])|set(old_build_source['files'])) if build_input(n)}
build_delta=[n for n in projection if source['files'].get(n)!=old_build_source['files'].get(n)]
assert not build_delta,build_delta
native_plan=load(ROOT/'out/ur0b/required-native-plan.json')
ctest=json.loads(subprocess.check_output(['ctest','--test-dir','out/host','--show-only=json-v1'],cwd=ROOT))
selected=[t for t in ctest['tests'] if t['name'] in native_plan['tests']]
assert len(selected)==22
native_files={t['command'][0]:sha(t['command'][0]) for t in selected}
native_log=(OUT/'native-final.log').read_text()
assert '100% tests passed, 0 tests failed out of 22' in native_log
node_log=(OUT/'node-final.log').read_text();node_count=int(re.search(r'# pass (\d+)\n# fail 0',node_log)[1])
python_log=(OUT/'python-final.log').read_text();python_count=int(re.search(r'Ran (\d+) tests',python_log)[1]);assert python_log.rstrip().endswith('OK')
assert node_count>=87 and python_count>=40
assert 'passed (120 contract files)' in (OUT/'boundary-final.log').read_text()
assert subprocess.run(['git','diff','--check'],cwd=ROOT,capture_output=True).returncode==0
tiers={}
for tier,build in [('64-192',ROOT/'out/ur0b/p36-r1/build-qualified-64-192'),('32-96',OUT/'build-qualified-32-96')]:
 sdk=build/'sdk/web';artifacts=files(sdk)
 if tier=='64-192':assert artifacts==inherited['qualified_sdk_artifacts']
 audit=load(OUT/('boundary-artifact-'+tier.split('-')[0]+'.json'));assert audit['status']=='QUALIFIED' and audit['errors']==[]
 definitions=next(line.strip() for line in (build/'build.ninja').read_text().splitlines() if 'AXIOM_BUILD_SOURCE_REVISION' in line)
 tiers[tier]={'mode':'REUSED_EXACT_ACCEPTED_BYTES' if tier=='64-192' else 'FRESH_AXIOM_CONSUMER_BUILD','build_directory':str(build),'sdk_directory':str(sdk),'artifacts':artifacts,'build_identity_definitions':definitions,
  'build_proof_refs':[ref(build/name) for name in ['ur0b-build-request.json','CMakeCache.txt','build.ninja','configure.log','build.log']],
  'boundary_audit_ref':ref(OUT/('boundary-artifact-'+tier.split('-')[0]+'.json'))}
write(RESULT/'artifact-provenance.json',{'status':'EXACT_ARTIFACT_PROVENANCE_VERIFIED','orchestration_revision':source['revision'],'orchestration_source_tree_sha256':source['source_tree_sha256'],
 'inherited_mechanism_proof_ref':ref(inherited_path),'accepted_build_source_tree_sha256':old_build_source['source_tree_sha256'],'unchanged_build_input_paths':len(projection),'build_input_projection_sha256':hashlib.sha256(json.dumps(projection,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'changed_build_input_paths':build_delta,
 'native_validation':{'mode':'FRESH_RUN_OF_EXISTING_EXECUTABLES_NO_REBUILD','tests':native_plan['tests'],'executable_sha256':native_files,'log_ref':ref(OUT/'native-final.log')},
 'tiers':tiers,'normal_smoke':{'mode':'INHERITED_EXACT_ACCEPTED_64_192_PAYLOAD_NO_NEW_BROWSER','binding_ref':ref(ROOT/'out/ur0b/p36-normal-consumer-smoke-binding.json'),'all_inherited_proof_files_rehashed':len(inherited['files'])},'dependency_rebuild':False,'no_relabeling_of_build_info':True})
write(RESULT/'regression.json',{'status':'REQUIRED_MECHANISMS_VERIFIED','node_passed':node_count,'python_passed':python_count,'native_passed':22,'boundary_contracts':120,'new_browser_workers':0,
 'negative_tests':['closed old authority/new directory','concurrent/relocated/single-use claim','unknown interruption full charge','all historical rows retained','overbudget rejected before spawn','changed compiler/proof/product/two artifact tiers rejected','offline chain cannot admit fresh sampling','nonzero worker exit rejected'],
 'development_findings':[{'finding':'explicit absent guard caused terminal accounting exception','resolution':'full-limit UNKNOWN_GUARD recorded durably','red_ref':ref(OUT/'admission-red-checked.log'),'green_ref':ref(OUT/'admission-green.log')},{'finding':'worker could retain OBSERVED summary before nonzero exit','resolution':'completed zero-exit guard is required','red_ref':ref(OUT/'admission-red-checked.log'),'green_ref':ref(OUT/'admission-green.log')}],
 'full_ctest':'NOT_RERUN; only frozen affected 22 required','performance_qualification':'NOT_STARTED','official_gate_pass':False})

compiler_names=['compile_run.mjs','decode_trace.mjs','metrics_compiler.mjs','canonical_chain.mjs','external_metrics.mjs','schedule.mjs','validate_metrics.py']
new_files=[RESULT/'artifact-provenance.json',RESULT/'full-source-identity.json',RESULT/'source-scope-audit.json',RESULT/'regression.json',Path(__file__)]
new_files += [OUT/name for name in ['node-final.log','python-final.log','native-final.log','native-test-command.json','boundary-final.log','admission-red-checked.log','admission-green.log','boundary-artifact-64.json','boundary-artifact-64.log','boundary-artifact-32.json','boundary-artifact-32.log']]
new_files += [ROOT/'verification/runners/g5/ur0'/name for name in compiler_names+['resume_runner.py','repair_runner.py','observed_worker.py','source_identity.py','build_variants.py']]
new_files += [ROOT/'verification/tests/test_g5_ur0_resume_runner.py']
for tier in tiers.values():new_files += [ROOT/entry['path'] for entry in tier['build_proof_refs']]
references={entry['path']:entry for entry in inherited['files']}
for path in new_files+[inherited_path]:references[str(path.relative_to(ROOT))]=ref(path)
proof={'schema':'g5-ur0b-r3-mechanism-proof-v1','status':'MECHANISMS_VERIFIED','scope':'fresh runner/compiler regressions, exact unchanged product/build projection and both budget artifacts; no browser qualification claimed',
 'package_sha256':runner.PACKAGE_SHA,'source_revision':source['revision'],'source_tree_sha256':source['source_tree_sha256'],'files':list(references.values()),'compiler_files':{name:sha(ROOT/'verification/runners/g5/ur0'/name) for name in compiler_names},
 'qualified_sdk_artifacts':tiers['64-192']['artifacts'],'qualified_sdk32_artifacts':tiers['32-96']['artifacts'],'historical_charged_seconds':runner.verify_history(history),'artifact_provenance_ref':ref(RESULT/'artifact-provenance.json'),
 'starting_revision':source['revision'],'build_identities_retained':True,'performance_pass':False}
write(RESULT/'mechanism-proofs.json',proof)
runner.verify_proof(RESULT/'mechanism-proofs.json',package,contract,ROOT/'out/ur0b/p36-r1/build-qualified-64-192/sdk/web',OUT/'build-qualified-32-96/sdk/web')
print(json.dumps({'status':proof['status'],'source_tree_sha256':source['source_tree_sha256'],'proof_sha256':sha(RESULT/'mechanism-proofs.json'),'regression':{'node':node_count,'python':python_count,'native':22,'boundaries':120},'artifact_tiers':list(tiers),'window_claimed':False}))
