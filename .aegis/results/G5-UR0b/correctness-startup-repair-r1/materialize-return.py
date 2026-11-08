"""Archive the narrow offline startup fix; never create a sample window."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
P=ROOT/'.aegis/packages/G5-UR0b-correctness-startup-repair-r1';V=ROOT/'.aegis/verification/G5-UR0b-correctness-startup-v1'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'));import source_identity,repair_runner
load=lambda p:json.loads(p.read_text())
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def copy(p,q):q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
assert sha(P/'package.json')=='8add8e7c3d624ace9e09ea032e3e27b008a1bc34c505e6a6ba2da67ab83ebd89'
assert sha(V/'verification-spec.json')=='7bb77a8fe61739267ac23f46d15b4920e9b3331de7c2054932a1b99a9bcac534'
package=load(P/'package.json');work=load(ROOT/'.aegis/results/G5-UR0b/control-review-r6/repair-request.json')
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 r=package[key];assert sha(P/r['path'])==r['sha256']
current=source_identity.identity(ROOT);before=load(D/'accepted-start-source-identity.json')
delta=sorted(n for n in before['files'].keys()|current['files'].keys() if before['files'].get(n)!=current['files'].get(n))
controls={str(q.relative_to(ROOT)) for parent in [P,V] for q in parent.rglob('*') if q.is_file()}
source_delta=sorted(set(delta)-controls);assert set(source_delta)<=set(work['authorized_source_paths']) and len(source_delta)==10,source_delta
assert current['revision']==work['repository']['head'];assert not subprocess.check_output(['git','diff','--cached','--name-only'],cwd=ROOT)
subprocess.run(['git','diff','--check'],cwd=ROOT,check=True);write(D/'full-source-identity.json',current)
baseline=ROOT/'verification/evidence/performance/g5-unified-render-ur0b/a3d022abba7d5909aa6dc1e97f5b0f03af0d34154aae585a0a0db21d2f3162c4/resume-r6'
old_manifest=load(baseline/'manifest.json');assert sha(baseline/'manifest.json')=='64192659caae1c732c426432141eb53e3b5c18d92bec883c85e5ce8a6cf60c95'
seam=load(D/'source-seam.json');baseline_refs=[]
for item in seam['source_refs']:
 q=ROOT/item['path']
 if item['path'] in ['verification/runners/g5/ur0/consumer.mjs','verification/runners/g5/ur0/worker.mjs']:q=baseline/'source'/item['path'];assert old_manifest['files']['source/'+item['path']]['sha256']==item['sha256']
 assert sha(q)==item['sha256'];baseline_refs.append({'logical_source':item,'immutable_materialization':ref(q)})
for name in ['initializationProfile','repairedInitializationProfile']:
 pattern=r'export const '+name+r'=Object.freeze\((\{.*?\})\);'
 old=re.search(pattern,(baseline/'source/verification/runners/g5/ur0/initialization_basis.mjs').read_text(),re.S)[1];new=re.search(pattern,(ROOT/'verification/runners/g5/ur0/initialization_basis.mjs').read_text(),re.S)[1];assert old==new,name
old_py=(baseline/'source/verification/runners/g5/ur0/repair_runner.py').read_text();new_py=(ROOT/'verification/runners/g5/ur0/repair_runner.py').read_text();assert old_py[old_py.index('OLD_INITIALIZATION_PROFILE='):old_py.index('CASES=')]==new_py[new_py.index('OLD_INITIALIZATION_PROFILE='):new_py.index('CASES=')]
profile=load(D/'new-correctness-profile.json');assert profile==repair_runner.STARTUP_INITIALIZATION_PROFILE
for n,value in profile.items():assert sha(ROOT/n)==value,n
rows=load(D/'reverification.json')['rows'];assert len(rows)==19 and all(r['status']=='INVARIANT_EXCEPT_COMPILER_PROVENANCE' for r in rows[:18]) and rows[-1]['status']=='PRESERVED_MISSING'
for row in rows:
 for item in row['original_input_refs']:assert sha(ROOT/item['path'])==item['sha256']
 for key in ['derived_summary_ref','derived_proof_ref']:assert sha(ROOT/row[key]['path'])==row[key]['sha256']
 assert load(ROOT/row['derived_summary_ref']['path'])['analysis']['mode']=='OFFLINE_REVERIFICATION'
r6=load(ROOT/'.aegis/results/G5-UR0b/resume-r6/result.json');assert sha(ROOT/'.aegis/results/G5-UR0b/resume-r6/result.json')=='41650906d25a902f4515b06d46cf757386aad75593d017f89d51c1461809c301';assert sha(ROOT/r6['window_ref']['path'])==r6['window_ref']['sha256'];assert load(ROOT/r6['window_ref']['path'])['status']=='CLOSED'
red=(D/'startup-model-red.log').read_text();assert 'orphaned_completions":1' in red and 'startup boundary must be closed' in red
node=(D/'node.log').read_text();python=(D/'python.log').read_text();assert '# tests 127' in node and '# pass 127' in node and '# fail 0' in node;assert 'Ran 96 tests' in python and python.endswith('OK\n');assert 'passed (120 contract files)' in (D/'boundary.log').read_text()
write(D/'validation.json',{'status':'PASS','root_cause_red_ref':ref(D/'startup-model-red.log'),'node':{'tests':127,'passed':127,'failed':0,'command':'node --test verification/tests/g5_*.test.mjs verification/sdk_web_external/pointer_input_test.mjs verification/sdk_web_external/control_api_test.mjs','log_ref':ref(D/'node.log')},'python':{'tests':96,'passed':96,'failed':0,'command':"python3.12 -m unittest discover -s verification/tests -p 'test_g5_*.py'",'log_ref':ref(D/'python.log')},'boundary':{'contracts':120,'command':'python3.12 tools/check_runtime_boundaries.py --root .','log_ref':ref(D/'boundary.log')},'historical_reader_subset':{'tests':73,'log_ref':ref(D/'historical-readers.log')},'diff_check':{'command':'git diff --check','exit_code':0},'native_unit_invocations':{'count':2,'input':'malformed snapshots only','included_in_full_python_suite':True},'new_browser_starts':0,'new_builds':0,'new_valid_native_qualification':0})
write(D/'preservation.json',{'status':'SCOPE_AND_RECORDED_IDENTITIES_PRESERVED','accepted_source_tree_sha256':before['source_tree_sha256'],'current_source_tree_sha256':current['source_tree_sha256'],'source_delta':source_delta,'approved_control_delta':sorted(controls),'historical_source_seam_materializations':baseline_refs,'historical_profile_literals_unchanged':True,'new_correctness_only_profile':profile,'r4_historical_context':'Already isolated at fixed df214581 Git compiler; preserved unchanged and verified in the73-reader subset.','r5_historical_context':'Fixed a00dc4a2 compiler plus exact source-file namespace for7 helper bytes; bound evidence and real validators unchanged.','r6_historical_context':'Manifest-bound a3d source archive compiler/helper bytes; real validators and actual-current compiler/profile mismatch negatives retained.','r6_result_ref':ref(ROOT/'.aegis/results/G5-UR0b/resume-r6/result.json'),'r6_window_ref':r6['window_ref'],'r6_still_missing':rows[-1]['counts'],'old_17_plus_one_inputs_unchanged':True,'new_browser_starts':0,'historical_starts':35,'historical_seconds':1276})
groups=load(V/'oracle-cases.json')['groups']
write(D/'oracle-coverage.json',{'status':'10_FROZEN_OFFLINE_GROUPS_OBSERVED','groups':[dict(g,status='COVERED_BY_ROOT_RED_MODEL_TARGETED_AND_FULL_TESTS',evidence_refs=[ref(D/'startup-model-red.log')] if g['id']=='STARTUP-RED' else [ref(D/'node.log'),ref(D/'python.log'),ref(D/'reverification.json')] if g['id']=='RECORDING-INVARIANCE' else [ref(D/'node.log'),ref(D/'python.log')]) for g in groups],'official_gate_pass':False})
write(D/'future-applicability.json',{'startup_profile':'Exact new8-file tuple, correctness-only; requires independent review before any sample window.','recorded_evidence':'17 metrics/native plus one prior lower-z retain recorded status; actual r6 remains MISSING and cannot be retroactively qualified.','primary_path':'Source-level measured bodies/default callback and phase-arm positions remain unchanged; full file identities changed and no new primary sampling applicability is self-issued.','independent_performance_work':'Complete UR0b observability is not a blanket prerequisite for a separately scoped production performance task; use its own minimal correctness/local comparison contract.','no_r7_authorized':True,'baseline_status':'BLOCKED_EVIDENCE','remaining':'4 other correctness plus30main remain unstarted; permanent_live_mask record still has unresolved work1.','new_sampling_starts':0,'historical_starts':35,'historical_seconds':1276})
(D/'README.md').write_text('''# Correctness startup repair r1 — READY_FOR_CONTROL_REVIEW

This narrow verification-host fix positively drains startup before collector
reset/enable and fixture work. A page-owned frontier registers onFrame before
createRuntime, retains early callbacks, waits for a positive actual accepted
presentation plus the existing SDK-owned RAF queue to drain, and checks fresh
document/target/error/deadline facts. It does not use generation0, disabled
pending_gpu0, a fixed sleep or ten RAFs as completion proof. OFF and reloaded ON
use the same frontier under the unchanged setup/correctness outer deadlines.

The default onFrame callback and measured bodies remain unchanged; only the
cold correctness init branch gains the helper. Both historical profile literals
remain intact. A third exact8-file correctness-only profile requires two real
host frontier records with exact P20 ref. JS and Python verify and reconstruct
them from bound inputs while preserving every native accepted-chain check.

The controlled17-turn delayed callback model reproduces the old orphan RED.
Full127 Node/96 Python/120 boundary and diff checks pass, including only the2
allowed malformed native unit checks. Three historical runner contexts retain
real validators and actual-current mismatch negatives.17+1 original records
are invariant except compiler provenance; actual r6 remains MISSING7=1+5+1,
with seq22 unresolved. No original raw, pixel, result or closed window changed.

No browser/native qualification, build, commit/push or r7 ran. Sampling stays
35starts/1276s. UR0b remains BLOCKED_EVIDENCE; this does not gate a separately
scoped production performance repair with its own local validation contract.
No performance/platform/P34 PASS or automatic new sampling authority is issued.
''')
bundle=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/current['source_tree_sha256']/'correctness-startup-repair-r1';bundle.mkdir(parents=True,exist_ok=False)
shutil.copytree(P,bundle/'package');shutil.copytree(V,bundle/'verification-contract');shutil.copytree(D,bundle/'execution-and-checks',ignore=shutil.ignore_patterns('result.json','terminal-notification.txt','notification-delivery.json','__pycache__'))
for n in set(source_delta)|set(profile):copy(ROOT/n,bundle/'source'/n)
for r in baseline_refs:copy(ROOT/r['immutable_materialization']['path'],bundle/'source-baseline'/r['logical_source']['path'])
(bundle/'source/authorized-source.patch').write_bytes(subprocess.check_output(['git','diff','--',*source_delta],cwd=ROOT))
files={str(q.relative_to(bundle)):{'sha256':sha(q),'bytes':q.stat().st_size} for q in sorted(bundle.rglob('*')) if q.is_file()}
write(bundle/'manifest.json',{'schema':'g5-ur0b-startup-repair-bundle-v1','source_tree_sha256':current['source_tree_sha256'],'files':files,'excluded_envelopes':['manifest.json','result.json'],'envelope_rule':'Result binds manifest; notification gives both hashes without circular binding.'})
result={'schema_version':1,'task_id':'G5-UR0b-correctness-startup-repair-r1','status':'READY_FOR_CONTROL_REVIEW','continue_execution':False,'return_surface':'CONTROL_REVIEW','result_revision':current['revision'],'dirty':True,'source_tree_sha256':current['source_tree_sha256'],'package_ref':ref(P/'package.json'),'verification_spec_ref':ref(V/'verification-spec.json'),'preflight_ref':ref(D/'preflight.json'),'source_identity_ref':ref(D/'full-source-identity.json'),'validation_ref':ref(D/'validation.json'),'preservation_ref':ref(D/'preservation.json'),'oracle_coverage_ref':ref(D/'oracle-coverage.json'),'reverification_ref':ref(D/'reverification.json'),'future_applicability_ref':ref(D/'future-applicability.json'),'new_profile_ref':ref(D/'new-correctness-profile.json'),'materialized_ref':{'kind':'SHARED_WORKTREE_CONTENT_ADDRESSED_BUNDLE','path':str(bundle),'manifest_sha256':sha(bundle/'manifest.json'),'payload_files':len(files)},'new_browser_starts':0,'historical_starts':35,'historical_charged_seconds':1276,'old_r6_status':'MISSING_REJECTED_CLOSED_UNCHANGED','new_builds':0,'new_commit_push':0,'baseline_UR0b_status':'BLOCKED_EVIDENCE','r7_authorized':False,'independent_performance_package_not_gated_on_full_UR0b':True,'performance_or_official_gate_PASS':False}
write(D/'result.json',result);copy(D/'result.json',bundle/'result.json')
for n,value in files.items():assert sha(bundle/n)==value['sha256'] and (bundle/n).stat().st_size==value['bytes']
assert source_identity.identity(ROOT)['source_tree_sha256']==current['source_tree_sha256'];assert (D/'result.json').read_bytes()==(bundle/'result.json').read_bytes()
print(json.dumps({'status':result['status'],'result_ref':ref(D/'result.json'),'package_ref':ref(P/'package.json'),'source_tree_sha256':current['source_tree_sha256'],'materialized_ref':result['materialized_ref'],'new_browser_starts':0,'history':[35,1276]}))
