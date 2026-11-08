"""Archive completed repair and exact unresolved mandatory-suite failure."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
P=ROOT/'.aegis/packages/G5-UR0b-correctness-repair-r2';HERE=ROOT/'verification/runners/g5/ur0'
sys.path.insert(0,str(HERE));import source_identity
load=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
package=load(P/'package.json');assert sha(P/'package.json')=='769a4f59f45e87d23e58fb7b89b95d735c729b8657122e0d8a4bc73623ee3736'
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 binding=package[key];assert sha(P/binding['path'])==binding['sha256']
node=(D/'node.log').read_text();python=(D/'python.log').read_text();boundary=(D/'boundary.log').read_text()
assert '# tests 114' in node and '# pass 114' in node and '# fail 0' in node
assert 'Ran 60 tests' in python and 'FAILED (errors=1)' in python
assert 'test_explicit_accepted_reuse_keeps_old_modes_and_sources' in python
assert 'current compiler differs from accepted reuse compiler' in python
assert 'passed (120 contract files)' in boundary
verify=load(D/'reverification.json');assert verify['status']=='ALL_17_IMMUTABLE_RECORDINGS_INVARIANT' and len(verify['rows'])==17
scope=load(D/'source-scope-audit.json');assert scope['status']=='PASS'
now=source_identity.identity(ROOT);assert now['source_tree_sha256']==verify['source_sha256']==scope['source_after_sha256']
assert now['revision']=='df214581c1c80c2d546939271f3db2dc0e77a38b'
subprocess.run(['git','diff','--check'],cwd=ROOT,check=True)
write(D/'validation.json',{
 'status':'FROZEN_VERIFICATION_FAILURE',
 'executions':[
  {'command':'node --test verification/tests/g5_*.test.mjs verification/sdk_web_external/pointer_input_test.mjs verification/sdk_web_external/control_api_test.mjs','exit_code':0,'tests':114,'passed':114,'failed':0,'log_ref':ref(D/'node.log')},
  {'command':"python3.12 -m unittest discover -s verification/tests -p 'test_g5_*.py'",'exit_code':1,'tests':60,'passed':59,'errors':1,'log_ref':ref(D/'python.log'),'failure_ref':ref(D/'frozen-failure.json')},
  {'command':'python3.12 tools/check_runtime_boundaries.py --root .','exit_code':0,'contracts':120,'log_ref':ref(D/'boundary.log')},
  {'command':'git diff --check','exit_code':0},
 ],
 'recording_reverification_ref':ref(D/'reverification.json'),
 'new_sampling_workers':0,'new_builds':0,'new_native_qualification_probes':0,
 'native_unit_invocations':{'count':2,'allowed':True,'test_module':'verification/tests/test_g5_ur0_fixture_oracle.py','cases':['test_pixel_oracle_decodes_the_public_snapshot_and_rejects_invalid_bytes','test_malformed_snapshot_is_rejected'],'input':'malformed snapshot only','included_in_full_python_suite':True},
 'no_claim_of_complete_mandatory_suite_PASS':True,
})
passed_names=re.findall(r'^ok \d+ - (.*)$',node,re.M)
groups=[]
spec=load(ROOT/'.aegis/verification/G5-UR0b-correctness-harness-v1/oracle-cases.json')
special={'ACTUAL-OFF0-DIAGNOSTIC':D/'pixel-diagnostic.json','OLD17-INVARIANCE':D/'reverification.json','FROZEN-SCOPE':D/'source-scope-audit.json'}
for case in spec['cases']:
 key=case['id']
 if key in special:
  groups.append({'id':key,'status':'SATISFIED_OFFLINE','evidence_ref':ref(special[key])})
 else:
  names=[n for n in passed_names if n.startswith(key+':')]
  assert names,('missing passing oracle case',key)
  row={'id':key,'status':'SATISFIED_OFFLINE','passed_tests':names,'test_log_ref':ref(D/'node.log')}
  if key=='REAL-OLD-HUD-RED':row['actual_old_RED_ref']=ref(D/'red-precondition.json')
  if key=='CREATION-PROFILES':row['exact_profile_ref']=ref(D/'repaired-profile.json');row['python_coherence_ref']=ref(D/'profile-coherence.log')
  if key=='STAGE-TERMINALS':row['original_timers_and_primary_path_ref']=ref(D/'source-scope-audit.json');row['actual_new_browser_timeout_diagnosis']='NOT_OBSERVED_NO_NEW_BROWSER_RUN'
  groups.append(row)
assert len(groups)==12
write(D/'oracle-coverage.json',{'status':'FROZEN_ORACLE_GROUPS_COVERED_OFFLINE_MANDATORY_SUITE_STILL_BLOCKED','groups':groups,'mandatory_suite_failure_ref':ref(D/'frozen-failure.json'),'no_performance_or_platform_qualification':True})
failure=load(D/'frozen-failure.json')
write(D/'ownership-handoff.json',{
 'type':'ownership_handoff','from_owner':'aegis-implementation','to':'aegis',
 'reason':'earlier_untrusted_layer','requested_stage':'P31',
 'earliest_untrusted_layer':'Required regression test contract outside the frozen source allowance',
 'status':'FROZEN_VERIFICATION_FAILURE','suggested_next_stage':'Classify test contract drift and authorize its narrow correction',
 'evidence_ref':ref(D/'frozen-failure.json'),
 'requested_action':failure['requested_control_action'],
 'preserve_implementation':True,'repeat_already_satisfied_repair_oracles':False,
})
(D/'README.md').write_text('''# Correctness harness repair r2 — FROZEN_VERIFICATION_FAILURE

The authorized repair is implemented in the shared worktree. Runtime/SDK,
consumer/input schedule, native oracle, limits, old packages and old windows
remain unchanged. The source-scope audit identifies all 12 changed code/test
paths, within the 13-path allowance.

The driver retains hand selection HUD, uses the independently fixed snapped
transform, captures before/live/after state, and requires matching public/native
snapshot identity, order and transform before live-vs-canonical comparison. Cold
transport uses <=32 KiB units and <=8 units / 256 KiB per encoded batch, source /
received SHA256 and finish/cancel ownership. Coarse stages bracket render/settle,
readback/hash, transfer, snapshot, native compare and reload/close. The single
30s correctness phase and 90s outer guard remain unchanged. One exact repaired
worker/helper tuple is pinned in JS and Python; paired-init matching is strict,
unique and correctness-only. Other modes retain init-only admission.

114 Node tests and 120 boundary contracts pass. Python ran 60 tests: 59 pass,
one unchanged r4 accepted-reuse positive test errors because it requires the
current compiler to match the old frozen initialization_basis.mjs hash. The
production refusal is correct. The test is outside this task's source scope;
neither the reader nor old reuse packet was weakened. See frozen-failure.json.

All 17 immutable recordings were recompiled independently into new output.
canonical-chain.json, decoded.json, metrics.json and external-metrics.json are
byte identical to accepted baselines; every summary field is identical except
explicit compiler provenance. OFFLINE output is rejected as fresh admission.
The original cohort/120/240 remain recorded evidence; the new compiler requires
independent reuse acceptance before another sampling window.

Actual archived OFF1 reproduces the old 16112-pixel RED. With frozen public HUD,
file-only OFF0/OFF1 recomparisons both have 0 residual mismatches. Original FAILED
and timeout124 records remain unchanged; original live public capture is missing,
so these diagnostics do not retrospectively qualify the old partial worker.

This repair ran no browser worker, no build and no valid-snapshot native probe.
The full Python suite executed the 2 explicitly permitted malformed-snapshot
native checks. Historical sampling stays 33 starts / 1251 seconds. No old window
was reopened. Six correctness plus 30 main remain unauthorized and unexecuted.
No commit/push, UR0c/UR1, platform/performance or P34 PASS is issued.

Return the exact mandatory-suite failure to control for its narrow test-contract
correction and refreshed compiler/reuse acceptance. Preserve the implementation
and all invariant recompilation evidence instead of repeating completed work.
''')
bundle=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/now['source_tree_sha256']/'correctness-repair-r2'
bundle.mkdir(parents=True,exist_ok=False)
shutil.copytree(P,bundle/'package')
shutil.copytree(D,bundle/'execution-and-checks',ignore=shutil.ignore_patterns('__pycache__'))
shutil.copytree(ROOT/'out/ur0b/correctness-repair-r2/recompiled',bundle/'recompiled-recordings')
source_dir=bundle/'source';source_dir.mkdir()
shutil.copyfile(D/'full-source-identity.json',source_dir/'full-source-identity.json')
paths=set(scope['changed_source_paths'])|{'verification/runners/g5/ur0/'+n for n in ['compile_run.mjs','decode_trace.mjs','canonical_chain.mjs','metrics_compiler.mjs','external_metrics.mjs','schedule.mjs','validate_metrics.py','source_identity.py','consumer.mjs','fixture_oracle.cpp']}
for name in sorted(paths):
 target=source_dir/'files'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/name,target)
patch=subprocess.check_output(['git','diff','--binary'],cwd=ROOT);(source_dir/'tracked-source.patch').write_bytes(patch)
files={str(p.relative_to(bundle)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(bundle.rglob('*')) if p.is_file()}
write(bundle/'manifest.json',{'schema':'ur0b-correctness-repair-bundle-v1','source_tree_sha256':now['source_tree_sha256'],'files':files,'excluded_envelopes':['manifest.json','result.json'],'envelope_rule':'result binds this manifest; result and manifest hashes are carried in the terminal notice, without circular binding.'})
result={
 'schema_version':1,'task_id':'G5-UR0b-correctness-repair-r2','stage':'P32_OFFLINE_HARNESS_REPAIR',
 'status':'FROZEN_VERIFICATION_FAILURE','root_cause':'TEST_CONTRACT_DRIFT_OUTSIDE_ALLOWED_SOURCE_SCOPE',
 'continue_execution':False,'return_surface':'CONTROL_REVIEW',
 'result_revision':now['revision'],'dirty':True,'source_tree_sha256':now['source_tree_sha256'],
 'package_ref':ref(P/'package.json'),'verification_spec_ref':ref(ROOT/'.aegis/verification/G5-UR0b-correctness-harness-v1/verification-spec.json'),
 'validation_ref':ref(D/'validation.json'),'oracle_coverage_ref':ref(D/'oracle-coverage.json'),
 'reverification_ref':ref(D/'reverification.json'),'repaired_profile_ref':ref(D/'repaired-profile.json'),
 'scope_audit_ref':ref(D/'source-scope-audit.json'),'reuse_assessment_ref':ref(D/'reuse-assessment.json'),
 'failure_ref':ref(D/'frozen-failure.json'),'handoff_ref':ref(D/'ownership-handoff.json'),
 'pixel_diagnostic_ref':ref(D/'pixel-diagnostic.json'),'future_position_ref':ref(D/'future-continuation-position.json'),
 'materialized_ref':{'kind':'SHARED_WORKTREE_CONTENT_ADDRESSED_BUNDLE','path':str(bundle),'manifest_sha256':sha(bundle/'manifest.json')},
 'completed_scope':'Authorized harness/JS-Python profile implementation, offline oracle coverage and 17 invariant recompilations; mandatory suite has one unresolved test contract drift.',
 'remaining':['Control classification and narrow correction of the one unchanged r4 positive reuse test','Independent control review and current compiler/recording-reuse acceptance','Separately authorized six correctness then 30 main workers, not run here'],
 'performance_or_official_gate_PASS':False,
}
write(D/'result.json',result);shutil.copyfile(D/'result.json',bundle/'result.json')
for name,value in files.items():assert sha(bundle/name)==value['sha256']
assert (D/'result.json').read_bytes()==(bundle/'result.json').read_bytes()
assert source_identity.identity(ROOT)['source_tree_sha256']==now['source_tree_sha256']
print(json.dumps({'status':result['status'],'result_ref':ref(D/'result.json'),'package_ref':ref(P/'package.json'),'source_tree_sha256':now['source_tree_sha256'],'bundle':result['materialized_ref'],'payload_files':len(files)}))
