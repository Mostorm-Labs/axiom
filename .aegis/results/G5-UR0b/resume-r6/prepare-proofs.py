"""Cold source/artifact/reuse binding only; no worker, build or browser probe."""
import json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import scoped_remaining_runner as runner
import source_identity
sha=runner.sha;load=runner.load;write=runner.atomic
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p)}
p,c,h=runner.authority();packet=runner.acceptance();reused=runner.accepted_reuse(packet)
assert not (D/'window.json').exists() and not (D/'admission-stop.json').exists()
node=(D/'node-final.log').read_text();python=(D/'python-final.log').read_text()
assert '# pass 119' in node and '# fail 0' in node and 'Ran 92 tests' in python and python.endswith('OK\n')
assert 'passed (120 contract files)' in (D/'boundary-final.log').read_text()
assert 'Ran 16 tests' in (D/'runner-green.log').read_text() and (D/'runner-green.log').read_text().endswith('OK\n')
source=source_identity.identity(ROOT)
baseline=runner.bound(runner.bound(p['authority_lock_ref'],runner.PACKAGE.parent)['accepted_source_identity_ref'])
delta=sorted(n for n in baseline['files'].keys()|source['files'].keys() if baseline['files'].get(n)!=source['files'].get(n))
allowed=set(c['authorized_source_paths'])|{str(q.relative_to(ROOT)) for q in runner.PACKAGE.parent.iterdir() if q.is_file()}
assert set(delta)<=allowed
write(D/'full-source-identity.json',source)
write(D/'source-scope-audit.json',{'status':'AUTHORIZED_TWO_NEW_ORCHESTRATION_PATHS_PLUS_PACKAGE_ONLY','accepted_source_sha256':baseline['source_tree_sha256'],'current_source_sha256':source['source_tree_sha256'],'changed_paths':delta,'all_harness_compiler_profile_product_bytes_unchanged':True,'public_api_impact':'NONE'})
old=runner.bound(runner.request()['accepted_artifact_provenance_ref']);provenance=runner.bound(old['artifact_provenance_ref'])
refs=[ref(D/'source-scope-audit.json'),ref(D/'full-source-identity.json'),ref(Path(__file__)),ref(runner.PACKAGE),runner.request()['review_result_ref'],runner.request()['accepted_reuse_ref'],runner.request()['historical_ledger_ref'],runner.request()['run_plan_ref']]
for name in ['node-final.log','python-final.log','boundary-final.log','runner-red.log','runner-green.log','package-preflight.json']:refs.append(ref(D/name))
for name in ['scoped_remaining_runner.py','reverified_remaining_runner.py','remaining_runner.py','resume_runner.py','repair_runner.py','observed_worker.py','source_identity.py','fixture_plan.mjs']+list(packet['compiler_file_sha256']):refs.append(ref(ROOT/'verification/runners/g5/ur0'/name))
for name in packet['repaired_worker_and_helpers']:refs.append(ref(ROOT/name))
refs.extend([ref(ROOT/'verification/runners/g5/bounded_worker.py'),ref(ROOT/'verification/tests/test_g5_ur0_scoped_remaining_runner.py')])
for tier in provenance['tiers'].values():
 runner.previous.verify_sdk(Path(tier['sdk_directory']),tier['artifacts'])
 for binding in tier['build_proof_refs']+[tier['boundary_audit_ref']]:runner.bound_hash(binding);refs.append(binding)
for path,digest in provenance['native_executable_sha256'].items():assert sha(Path(path))==digest;refs.append(ref(Path(path)))
runner.bound_hash(provenance['native_test_evidence_ref']);refs.append(provenance['native_test_evidence_ref'])
refs.append(runner.request()['accepted_artifact_provenance_ref'])
write(D/'cold-preflight.json',{'status':'COLD_AUTHORITY_REUSE_HISTORY_ARTIFACT_PREFLIGHT_PASS','recorded_prerequisites':len(reused['records']),'recorded_correctness':len(reused['recorded_correctness_cases']),'history':{'starts':h['starts'],'seconds':h['seconds']},'sdk_artifact_counts':{k:len(v['artifacts']) for k,v in provenance['tiers'].items()},'new_browser_workers':0});refs.append(ref(D/'cold-preflight.json'))
artifact={'status':'EXACT_EXISTING_SDK_AND_NATIVE_EVIDENCE_REUSED','previous_provenance_ref':old['artifact_provenance_ref'],'tiers':provenance['tiers'],'native_test_evidence_ref':provenance['native_test_evidence_ref'],'native_executable_sha256':provenance['native_executable_sha256'],'build_info_not_relabelled':True,'new_builds':0,'extra_qualification_probes':0}
write(D/'artifact-provenance.json',artifact);refs.append(ref(D/'artifact-provenance.json'))
regression={'status':'PASS','node_tests':119,'python_tests':92,'new_runner_offline_tests':16,'boundary_contracts':120,'native_unit_invocations':{'count':2,'input':'malformed snapshots only','authorized':True,'included_in_full_python_suite':True},'new_browser_workers':0,'new_builds':0,'history':{'starts':34,'seconds':1264},'new_plan':{'workers':35,'nominal_seconds':5850,'concurrency':1},'performance_pass':False}
write(D/'regression.json',regression);refs.append(ref(D/'regression.json'))
proof={'schema':'g5-ur0b-r6-mechanism-proof-v1','status':'MECHANISMS_VERIFIED','package_sha256':runner.PACKAGE_SHA,'source_revision':source['revision'],'source_tree_sha256':source['source_tree_sha256'],'files':list({v['path']:v for v in refs}.values()),'qualified_sdk_artifacts':old['qualified_sdk_artifacts'],'qualified_sdk32_artifacts':old['qualified_sdk32_artifacts'],'compiler_files':packet['compiler_file_sha256'],'accepted_reuse_ref':runner.request()['accepted_reuse_ref'],'artifact_provenance_ref':ref(D/'artifact-provenance.json'),'historical_starts':34,'historical_debit_seconds':1264,'new_browser_workers_before_window':0,'performance_pass':False}
write(D/'mechanism-proofs.json',proof)
sdk64=Path(provenance['tiers']['64-192']['sdk_directory']);sdk32=Path(provenance['tiers']['32-96']['sdk_directory'])
runner.verify_proof(D/'mechanism-proofs.json',p,c,sdk64,sdk32)
command=['python3.12','verification/runners/g5/ur0/scoped_remaining_runner.py','--sdk64',str(sdk64),'--sdk32',str(sdk32),'--proofs',str(D/'mechanism-proofs.json'),'--output',str(ROOT/'out/ur0b/resume-r6/execution')]
write(D/'execution-command.json',{'command':command,'single_window':True,'new_worker_plan_count':35,'no_hidden_smoke':True})
write(D/'preflight.json',{'status':'READY_FOR_SINGLE_AUTHORIZED_WINDOW','package_ref':ref(runner.PACKAGE),'source_tree_sha256':source['source_tree_sha256'],'proof_ref':ref(D/'mechanism-proofs.json'),'accepted_reuse_ref':runner.request()['accepted_reuse_ref'],'artifact_provenance_ref':ref(D/'artifact-provenance.json'),'regression_ref':ref(D/'regression.json'),'old_window':'CLOSED','new_window_claimed':False,'new_starts':0})
print(json.dumps({'status':'READY_FOR_SINGLE_AUTHORIZED_WINDOW','source_tree_sha256':source['source_tree_sha256'],'proof_ref':ref(D/'mechanism-proofs.json'),'command':command}))
