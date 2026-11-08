"""Cold r4 source/artifact/reuse binding; no build, worker or browser probe."""
import hashlib,json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent;OUT=ROOT/'out/ur0b/resume-r4'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import remaining_runner as runner
import source_identity
load=runner.load;sha=runner.sha;write=runner.atomic
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p)}
p,c,history,inputs=runner.authority();reused=runner.accepted_reuse(inputs)
assert not (RESULT/'window.json').exists() and not (RESULT/'admission-stop.json').exists()
write(RESULT/'reused-prerequisites.json',reused)
source=source_identity.identity(ROOT)
accepted=runner.bound(runner.bound(p['authority_lock_ref'],runner.PACKAGE.parent)['accepted_source_identity_ref'])
changes={n:{'before':accepted['files'].get(n),'after':source['files'].get(n)} for n in sorted(set(accepted['files'])|set(source['files'])) if accepted['files'].get(n)!=source['files'].get(n)}
allowed=set(c['authorized_source_paths'])|{str(x.relative_to(ROOT)) for x in runner.PACKAGE.parent.iterdir() if x.is_file()}
assert set(changes)<=allowed
write(RESULT/'full-source-identity.json',source)
write(RESULT/'source-scope-audit.json',{'status':'AUTHORIZED_TWO_NEW_SOURCES_PLUS_PACKAGE_ONLY','accepted_source_tree_sha256':accepted['source_tree_sha256'],'source_tree_sha256':source['source_tree_sha256'],'changed_paths':list(changes),'changes':changes,'public_api_impact':'NONE','old_accepted_sources_unchanged':True,'source_identity_exclusions_unchanged':source['excluded_self_outputs']==accepted['excluded_self_outputs']})
node=int(re.search(r'# pass (\d+)\n# fail 0',(OUT/'node-final.log').read_text())[1]);python=int(re.search(r'Ran (\d+) tests',(OUT/'python-final.log').read_text())[1]);assert node==100 and python==56
assert (OUT/'python-final.log').read_text().rstrip().endswith('OK') and 'passed (120 contract files)' in (OUT/'boundary-final.log').read_text()
assert subprocess.run(['git','diff','--check'],cwd=ROOT,capture_output=True).returncode==0
provenance=runner.bound(inputs['existing_artifact_provenance_ref']);native=provenance['native_validation']
assert sha(ROOT/native['log_ref']['path'])==native['log_ref']['sha256']
assert all(sha(x)==v for x,v in native['executable_sha256'].items())
tiers=provenance['tiers'];refs=[ref(RESULT/'reused-prerequisites.json'),ref(RESULT/'full-source-identity.json'),ref(RESULT/'source-scope-audit.json'),ref(Path(__file__))]
for n in ['node-final.log','python-final.log','boundary-final.log','runner-red.log','runner-green.log']:refs.append(ref(OUT/n))
for n in ['remaining_runner.py','resume_runner.py','repair_runner.py','observed_worker.py','source_identity.py','worker.mjs','consumer.mjs','fixture_plan.mjs']+list(inputs['compiler_file_sha256']):refs.append(ref(ROOT/'verification/runners/g5/ur0'/n))
refs.append(ref(ROOT/'verification/runners/g5/bounded_worker.py'));refs.append(ref(ROOT/'verification/tests/test_g5_ur0_remaining_runner.py'))
refs.append(inputs['existing_artifact_provenance_ref']);refs.append(native['log_ref'])
for x in native['executable_sha256']:refs.append(ref(Path(x)))
for tier in tiers.values():
 runner.verify_sdk(Path(tier['sdk_directory']),tier['artifacts'])
 for r in tier['build_proof_refs']:assert sha(ROOT/r['path'])==r['sha256'];refs.append(r)
 refs.append(tier['boundary_audit_ref']);assert sha(ROOT/tier['boundary_audit_ref']['path'])==tier['boundary_audit_ref']['sha256']
normal_binding=ROOT/'out/ur0b/p36-normal-consumer-smoke-binding.json';refs.append(ref(normal_binding))
reuse_proof={'status':'EXACT_EXISTING_ARTIFACT_AND_PRODUCT_EVIDENCE_REUSED','previous_provenance_ref':inputs['existing_artifact_provenance_ref'],'tiers':tiers,'native_test_count':22,'native_test_evidence_ref':native['log_ref'],'native_executable_sha256':native['executable_sha256'],'normal_consumer_smoke_binding_ref':ref(normal_binding),'normal_smoke_rerun':False,'builds_or_probes':0,'source_projection':'Full accepted2ca47 source map is unchanged except the nine r4 package files and two new runner/test files','build_info_not_relabelled':True}
write(RESULT/'artifact-provenance.json',reuse_proof);refs.append(ref(RESULT/'artifact-provenance.json'))
regression={'status':'MECHANISMS_VERIFIED','node_passed':node,'python_passed':python,'new_r4_test_count':12,'boundary_contracts':120,'native_cpp':'Existing hash-bound22CTest/11executables reused without rerun','new_browser_workers':0,'new_artifact_builds':0,'old_r3_status':'CLOSED','historical_starts':30,'historical_debit_seconds':1133,'new_plan_count':38,'new_nominal_limit_seconds':6120,'cumulative_planned_seconds':7253,'original_cap_seconds':8370,'public_api_impact':'NONE','performance_pass':False}
write(RESULT/'regression.json',regression);refs.append(ref(RESULT/'regression.json'))
proof={'schema':'g5-ur0b-r4-mechanism-proof-v1','status':'MECHANISMS_VERIFIED','package_sha256':runner.PACKAGE_SHA,'source_revision':source['revision'],'source_tree_sha256':source['source_tree_sha256'],'files':list({r['path']:r for r in refs}.values()),'qualified_sdk_artifacts':tiers['64-192']['artifacts'],'qualified_sdk32_artifacts':tiers['32-96']['artifacts'],'compiler_files':inputs['compiler_file_sha256'],'accepted_reuse_ref':ref(RESULT/'reused-prerequisites.json'),'artifact_provenance_ref':ref(RESULT/'artifact-provenance.json'),'historical_debit_seconds':1133,'new_browser_workers_before_window':0,'performance_pass':False}
write(RESULT/'mechanism-proofs.json',proof)
runner.verify_proof(RESULT/'mechanism-proofs.json',p,c,Path(tiers['64-192']['sdk_directory']),Path(tiers['32-96']['sdk_directory']))
preflight={'status':'REPOSITORY_PACKAGE_EVIDENCE_REUSE_AND_MECHANISM_BOUND','package_ref':ref(runner.PACKAGE),'source_tree_sha256':source['source_tree_sha256'],'proof_ref':ref(RESULT/'mechanism-proofs.json'),'reuse_ref':ref(RESULT/'reused-prerequisites.json'),'history_ref':ref(runner.PACKAGE.parent/'historical-budget.json'),'plan_ref':ref(runner.PACKAGE.parent/'run-plan.json'),'old_r3_window_status':'CLOSED','new_window_claimed':False,'regression':regression}
write(RESULT/'preflight.json',preflight);print(json.dumps(preflight))
