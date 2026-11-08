"""Materialize only the authorized historical-test-context correction."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
P=ROOT/'.aegis/packages/G5-UR0b-correctness-test-reverify-r1';R2=ROOT/'.aegis/results/G5-UR0b/correctness-repair-r2'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'));import source_identity
load=lambda p:json.loads(p.read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
assert sha(P/'package.json')=='1198fb988e286eda097f4016258291ac5d4e667eaf0e172f7d7482ce285660af'
package=load(P/'package.json')
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 value=package[key];assert sha(P/value['path'])==value['sha256']
current=source_identity.identity(ROOT);before=load(D/'starting-source-identity.json')
delta=sorted(n for n in before['files'].keys()|current['files'].keys() if before['files'].get(n)!=current['files'].get(n))
assert delta==['verification/tests/test_g5_ur0_remaining_runner.py'],delta
assert current['revision']==package['resume_cursor']['revision']
assert not subprocess.check_output(['git','diff','--cached','--name-only'],cwd=ROOT)
subprocess.run(['git','diff','--check'],cwd=ROOT,check=True)
r2=load(R2/'result.json');assert sha(R2/'result.json')=='926ab4219a0aa1181432281fb3f26a871ea6850757ace6f67459de4572d870e4'
assert r2['status']=='FROZEN_VERIFICATION_FAILURE'
r2bundle=Path(r2['materialized_ref']['path']);manifest=r2bundle/'manifest.json'
assert sha(manifest)==r2['materialized_ref']['manifest_sha256']
archived=load(manifest)['files']
for name,value in archived.items():assert sha(r2bundle/name)==value['sha256'] and (r2bundle/name).stat().st_size==value['bytes'],name
assert (r2bundle/'result.json').read_bytes()==(R2/'result.json').read_bytes()
assert sha(R2/'reverification.json')=='918b3cdd3e081e034cfa0cd7e9651bf9fcc1043388e14e2ff1d37c6e458d86a0'
verified=load(R2/'reverification.json');assert len(verified['rows'])==17
compiler=load(ROOT/verified['rows'][0]['derived_summary_ref']['path'])['analysis']['compiler_files']
for name,digest in compiler.items():assert sha(ROOT/'verification/runners/g5/ur0'/name)==digest,name
write(D/'preservation.json',{
 'status':'PASS','source_delta':delta,'source_before_sha256':before['source_tree_sha256'],
 'source_after_sha256':current['source_tree_sha256'],
 'unchanged_harness_compiler_profile_and_product_bytes':True,
 'r2_result_ref':ref(R2/'result.json'),'r2_manifest_ref':ref(manifest),
 'r2_payload_files_verified':len(archived),'r2_result_status_preserved':r2['status'],
 'reused_r2_oracle_ref':ref(R2/'oracle-coverage.json'),
 'reused_r2_17_recording_ref':ref(R2/'reverification.json'),
 'unchanged_compiler_file_sha256':compiler,
 'rerun_17_compilations':False,'rerun_12_oracle_groups':False,
 'no_new_reuse_acceptance':True,
})
write(D/'full-source-identity.json',current)
revision='df214581c1c80c2d546939271f3db2dc0e77a38b'
packet=ROOT/'.aegis/packages/G5-UR0b-resume-r4/reuse-inputs.json';inputs=load(packet)
fixture=D/'historical-compiler-fixture';fixture.mkdir(exist_ok=False)
fixture_files={}
for name,expected in inputs['compiler_file_sha256'].items():
 value=subprocess.check_output(['git','show',revision+':verification/runners/g5/ur0/'+name],cwd=ROOT)
 assert hashlib.sha256(value).hexdigest()==expected,name
 (fixture/name).write_bytes(value)
 fixture_files[name]={'sha256':expected,'git_blob_oid':subprocess.check_output(['git','rev-parse',revision+':verification/runners/g5/ur0/'+name],cwd=ROOT,text=True).strip()}
write(D/'fixture-provenance.json',{
 'historical_git_revision':revision,'compiler_file_tuple':fixture_files,
 'matches_frozen_packet_ref':ref(packet),
 'actual_behavior':'accepted_reuse/admit_record/canonical checks execute unmocked; only the module HERE path is temporarily isolated and restored in finally.',
 'positive':'15 original OFFLINE records, original source, artifacts and receipts checked by the real reader.',
 'negative':'actual current compiler rejects the unchanged r4 packet with the existing compiler-mismatch error.',
 'current_compiler_reuse_permission':'NOT_GRANTED; independent control acceptance remains required.',
})
def count_py(name):
 text=(D/name).read_text();match=re.search(r'Ran (\d+) tests? in',text);assert match and text.endswith('OK\n'),name
 return int(match.group(1))
node=(D/'node.log').read_text();assert '# tests 114' in node and '# pass 114' in node and '# fail 0' in node
assert count_py('focused-green.log')==4 and count_py('module.log')==13 and count_py('python.log')==61
assert 'passed (120 contract files)' in (D/'boundary.log').read_text()
write(D/'validation.json',{
 'status':'PASS',
 'red_precondition_ref':ref(D/'red-precondition.json'),
 'executions':[
  {'command':'python3.12 -m unittest discover -s verification/tests -p test_g5_ur0_remaining_runner.py -k reuse','exit_code':0,'tests':4,'log_ref':ref(D/'focused-green.log')},
  {'command':'python3.12 -m unittest discover -s verification/tests -p test_g5_ur0_remaining_runner.py','exit_code':0,'tests':13,'log_ref':ref(D/'module.log')},
  {'command':'node --test verification/tests/g5_*.test.mjs verification/sdk_web_external/pointer_input_test.mjs verification/sdk_web_external/control_api_test.mjs','exit_code':0,'tests':114,'passed':114,'failed':0,'log_ref':ref(D/'node.log')},
  {'command':"python3.12 -m unittest discover -s verification/tests -p 'test_g5_*.py'",'exit_code':0,'tests':61,'passed':61,'failed':0,'log_ref':ref(D/'python.log')},
  {'command':'python3.12 tools/check_runtime_boundaries.py --root .','exit_code':0,'contracts':120,'log_ref':ref(D/'boundary.log')},
  {'command':'git diff --check','exit_code':0},
 ],
 'new_browser_sampling_workers':0,'new_builds':0,'new_native_qualification_probes':0,
 'native_unit_invocations':{'count':2,'included_in':'Full mandatory Python suite','input':'malformed snapshots only','allowed_by_package':True},
 'historical_sampling_starts':33,'historical_sampling_seconds':1251,
 'old_windows':'CLOSED_UNCHANGED','new_window_opened':False,
})
(D/'README.md').write_text('''# Correctness test reverification r1 — READY_FOR_CONTROL_REVIEW

Only verification/tests/test_g5_ur0_remaining_runner.py changed relative to this
P36 package start. The historical reuse positive now builds an exact temporary
compiler fixture from pinned df214581 Git blobs and checks every file against
the original frozen packet SHA256. accepted_reuse, admit_record, canonical,
mode/source/receipt/artifact checks remain real. Only HERE is isolated, then
restored in finally. No result mock, skip, xfail or expectedFailure is used.

A separate test uses the actual current compiler and requires the original r4
packet to be rejected with the existing compiler-mismatch error. No reader or
packet was modified, and no current compiler reuse permission is issued.

The original focused RED is preserved. Focused tests 4/4, module 13/13, complete
Node 114/114, complete Python 61/61 and 120 boundary contracts pass. The Python
suite includes exactly 2 allowed malformed-snapshot native checks. No browser
worker, build, valid native qualification probe, commit/push or new window ran.
Historical sampling remains 33 starts / 1251 seconds.

All 188 r2 payload files and the exact r2 result/manifest were rehashed. Harness,
compiler and profiles are unchanged; all 12 oracle groups and 17 invariant
recording recompilations are referenced by their original precise hashes rather
than rerun. The r2 FAILED result stays unchanged. Independent control review and
current compiler/recording-reuse acceptance are still required before separately
authorizing 6 correctness plus 30 main workers. No performance/platform/P34 PASS.
''')
bundle=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/current['source_tree_sha256']/'correctness-test-reverify-r1'
bundle.mkdir(parents=True,exist_ok=False)
shutil.copytree(P,bundle/'package');shutil.copytree(D,bundle/'execution-and-checks',ignore=shutil.ignore_patterns('__pycache__'))
target=bundle/'source'/delta[0];target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/delta[0],target)
(bundle/'source'/'test-only.patch').write_bytes(subprocess.check_output(['git','diff','--',delta[0]],cwd=ROOT))
files={str(p.relative_to(bundle)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(bundle.rglob('*')) if p.is_file()}
write(bundle/'manifest.json',{'schema':'ur0b-test-fixture-reverify-bundle-v1','source_tree_sha256':current['source_tree_sha256'],'files':files,'excluded_envelopes':['manifest.json','result.json'],'envelope_rule':'Result binds manifest; terminal notice provides both hashes without circular binding.'})
result={
 'schema_version':1,'task_id':'G5-UR0b-correctness-test-reverify-r1','stage':'P36_CODE_REVERIFY',
 'status':'READY_FOR_CONTROL_REVIEW','continue_execution':False,'return_surface':'CONTROL_REVIEW',
 'result_revision':current['revision'],'dirty':True,'source_tree_sha256':current['source_tree_sha256'],
 'package_ref':ref(P/'package.json'),'repair_authorization_ref':ref(P/'repair-authorization.json'),
 'preflight_ref':ref(D/'preflight.json'),'red_precondition_ref':ref(D/'red-precondition.json'),
 'validation_ref':ref(D/'validation.json'),'preservation_ref':ref(D/'preservation.json'),
 'fixture_provenance_ref':ref(D/'fixture-provenance.json'),
 'preserved_r2_result_ref':ref(R2/'result.json'),
 'preserved_17_recording_reverification_ref':ref(R2/'reverification.json'),
 'preserved_12_oracle_ref':ref(R2/'oracle-coverage.json'),
 'materialized_ref':{'kind':'SHARED_WORKTREE_CONTENT_ADDRESSED_BUNDLE','path':str(bundle),'manifest_sha256':sha(bundle/'manifest.json')},
 'next_action':'Independent control review and exact current compiler/recording reuse acceptance; separate future sampling authorization only afterwards.',
 'remaining_sampling':'6 correctness plus 30 main remain unexecuted and unauthorized here.',
 'current_compiler_reuse_accepted_by_producer':False,'performance_or_official_gate_PASS':False,
}
write(D/'result.json',result);shutil.copyfile(D/'result.json',bundle/'result.json')
for name,value in files.items():assert sha(bundle/name)==value['sha256']
assert (D/'result.json').read_bytes()==(bundle/'result.json').read_bytes()
assert source_identity.identity(ROOT)['source_tree_sha256']==current['source_tree_sha256']
print(json.dumps({'status':result['status'],'result_ref':ref(D/'result.json'),'package_ref':ref(P/'package.json'),'source_tree_sha256':current['source_tree_sha256'],'materialized_ref':result['materialized_ref'],'payload_files':len(files)}))
