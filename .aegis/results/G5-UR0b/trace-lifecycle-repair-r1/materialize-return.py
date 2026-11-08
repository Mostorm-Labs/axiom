"""Materialize the frozen offline lifecycle repair and prospective reuse report."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
P=ROOT/'.aegis/packages/G5-UR0b-trace-lifecycle-repair-r1'
V=ROOT/'.aegis/verification/G5-UR0b-trace-lifecycle-v1'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'));import source_identity,repair_runner
load=lambda p:json.loads(p.read_text())
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
assert sha(P/'package.json')=='f187fa6ead75bc6eddca92f7bbc1e76862be7060b89cbc279816c577b3ec3d71'
package=load(P/'package.json');work=load(V/'implementation-request.json');inputs=load(V/'evidence-inputs.json')
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 r=package[key];assert sha(P/r['path'])==r['sha256']
source=source_identity.identity(ROOT);before=load(D/'starting-source-identity.json')
delta=sorted(n for n in before['files'].keys()|source['files'].keys() if before['files'].get(n)!=source['files'].get(n))
assert delta==sorted(work['source_scope']),delta
assert source['revision']==work['resume_cursor']['revision']
assert not subprocess.check_output(['git','diff','--cached','--name-only'],cwd=ROOT)
subprocess.run(['git','diff','--check'],cwd=ROOT,check=True)
write(D/'full-source-identity.json',source)
refs={}
def audit(x):
 if isinstance(x,dict):
  if 'path' in x and 'sha256' in x:
   p=ROOT/x['path'];assert p.is_file() and sha(p)==x['sha256'],str(p);refs[x['path']]=x['sha256']
  for c in x.values():audit(c)
 elif isinstance(x,list):
  for c in x:audit(c)
for p in V.glob('*.json'):audit(load(p))
original=load(ROOT/inputs['original_r5_result_ref']['path'])
old_bundle=Path(original['materialized_ref']['path']);old_manifest=load(old_bundle/'manifest.json')
assert sha(old_bundle/'manifest.json')==inputs['original_r5_manifest_ref']['sha256']
for name,value in old_manifest['files'].items():assert sha(old_bundle/name)==value['sha256'],name
assert sha(ROOT/original['window_ref']['path'])==original['window_ref']['sha256']
assert load(ROOT/original['window_ref']['path'])['status']=='CLOSED'
assert load(ROOT/inputs['original_r5_result_ref']['path'])['status']=='FROZEN_VERIFICATION_FAILURE'
red=load(D/'red-precondition.json');assert red['status']=='EXPECTED_ACTUAL_RECORD_RED'
assert red['counts']=={'accepted_raw_count':6,'qualified_initialization_count':0,'qualified_committed_count':5,'unresolved_accepted_count':1}
derived=load(D/'reverification.json');rows=derived['rows'];assert len(rows)==18
assert all(r['result']=='INVARIANT_EXCEPT_COMPILER_PROVENANCE' for r in rows[:17])
for row in rows:
 audit(row['original_input_refs'])
 for key in ['derived_summary_ref','derived_proof_ref']:audit(row[key])
 summary=load(ROOT/row['derived_summary_ref']['path'])
 assert summary['analysis']['mode']=='OFFLINE_REVERIFICATION'
 assert summary['analysis']['input_source_tree_sha256']==row['recording_source_tree_sha256']
 for n,value in summary['analysis']['compiler_files'].items():assert sha(ROOT/'verification/runners/g5/ur0'/n)==value
 repair_runner.require_canonical_chain(summary,require_metrics=row is not rows[-1])
assert rows[-1]['counts']=={'accepted_raw_count':6,'qualified_initialization_count':1,'qualified_committed_count':5,'qualified_decoded_count':5,'metric_receipt_count':None,'unresolved_accepted_count':0}
profiles={n:v for n,v in repair_runner.REPAIRED_INITIALIZATION_PROFILE.items()}
for n,value in profiles.items():assert sha(ROOT/n)==value
initial_js='verification/runners/g5/ur0/initialization_basis.mjs'
assert (ROOT/initial_js).read_text().split('const hash=')[0]==subprocess.check_output(['git','show','a00dc4a2:'+initial_js],cwd=ROOT,text=True).split('const hash=')[0]
fixture=D/'historical-compiler-fixture';fixture.mkdir(exist_ok=False)
accepted=load(ROOT/inputs['old17_reuse_ref']['path']);historical_files={}
for n,value in accepted['compiler_file_sha256'].items():
 data=subprocess.check_output(['git','show','a00dc4a2d4b4ad3a5ea1353d44794228a8355ca7:verification/runners/g5/ur0/'+n],cwd=ROOT)
 assert hashlib.sha256(data).hexdigest()==value;(fixture/n).write_bytes(data);historical_files[n]=value
write(D/'preservation.json',{'status':'EXACT_INPUTS_AND_HISTORY_PRESERVED','source_delta':delta,'starting_source_tree_sha256':before['source_tree_sha256'],'final_source_tree_sha256':source['source_tree_sha256'],'work_order_ref':ref(V/'implementation-request.json'),'p20_exact_refs_verified':refs,'pinned_creation_profile_files':profiles,'creation_profile_literals_unchanged':True,'original_r5_result_ref':inputs['original_r5_result_ref'],'original_r5_manifest_ref':inputs['original_r5_manifest_ref'],'original_r5_payload_files_verified':len(old_manifest['files']),'original_r5_window_ref':original['window_ref'],'original_r5_admission':'REJECTED_UNCHANGED','original_r5_window':'CLOSED_UNCHANGED','recompiled_original_input_sets':18,'sampling_starts':34,'sampling_charged_seconds':1264,'new_sampling_workers':0,'new_builds':0,'new_commit_push':0})
write(D/'historical-compiler-context.json',{'revision':'a00dc4a2d4b4ad3a5ea1353d44794228a8355ca7','historical_compiler_file_sha256':historical_files,'current_compiler_file_sha256':load(ROOT/rows[-1]['derived_summary_ref']['path'])['analysis']['compiler_files'],'test_behavior':'Exact Git compiler bytes form isolated temporary HERE fixtures, restored by unittest cleanup; accepted_reuse/admit_record/canonical/guard/ledger checks are real, never mocked. Actual new compiler must reject the unchanged r5 acceptance.','new_reuse_permission_issued':False})
def py_count(name):
 text=(D/name).read_text();m=re.search(r'Ran (\d+) tests? in',text);assert m and text.endswith('OK\n'),name;return int(m[1])
node=(D/'node.log').read_text();assert '# tests 119' in node and '# pass 119' in node and '# fail 0' in node
assert py_count('python.log')==76 and py_count('reader-green.log')==16 and py_count('historical-context.log')==14
assert '# tests 21' in (D/'lifecycle-green-node.log').read_text() and '# pass 21' in (D/'lifecycle-green-node.log').read_text()
assert 'passed (120 contract files)' in (D/'boundary.log').read_text()
write(D/'validation.json',{'status':'PASS','actual_record_red_ref':ref(D/'red-precondition.json'),'node_full':{'tests':119,'passed':119,'failed':0,'log_ref':ref(D/'node.log')},'python_full':{'tests':76,'passed':76,'failed':0,'log_ref':ref(D/'python.log')},'boundary':{'contracts':120,'log_ref':ref(D/'boundary.log')},'targeted':{'node_initialization':21,'python_reader':16,'python_historical_context':14},'diff_check':{'command':'git diff --check','exit_code':0},'native_unit_invocations':{'count':2,'input':'malformed snapshots only','included_in_full_python_suite':True},'new_browser_workers':0,'new_builds':0,'new_valid_native_qualification_probes':0,'old_sampling_starts':34,'old_sampling_charged_seconds':1264,'offline_derivation_attempt_note_ref':ref(D/'offline-compile-attempts.json')})
oracles=load(V/'oracle-cases.json')['cases']
evidence={
 'ACTUAL-R5-RED':[ref(D/'red-precondition.json')],
 'ACTUAL-SCOPED-POSITIVE':[rows[-1]['derived_summary_ref'],rows[-1]['derived_proof_ref']],
 'CROSS-PAGE-ALIAS':[ref(D/'lifecycle-green-node.log')],
 'UNKNOWN-SCOPE':[ref(D/'lifecycle-green-node.log'),ref(D/'reader-green.log')],
 'WITHIN-SCOPE-IDENTITY':[ref(D/'lifecycle-green-node.log'),ref(D/'reader-green.log')],
 'OUTCOME-INDEPENDENCE':[ref(D/'lifecycle-green-node.log'),ref(D/'historical-context.log')],
 'PRODUCER-READER-BINDING':[ref(D/'reader-red.log'),ref(D/'reader-green.log')],
 'LEGACY17-INVARIANCE':[ref(D/'reverification.json')],
 'HISTORICAL-TEST-CONTEXT':[ref(D/'historical-context.log'),ref(D/'historical-compiler-context.json')]
}
assert set(evidence)=={c['id'] for c in oracles}
write(D/'oracle-coverage.json',{'status':'9_FROZEN_ORACLE_GROUPS_OBSERVED','verification_spec_ref':work['verification_spec_ref'],'groups':[{'id':c['id'],'status':'SATISFIED_BY_RECORDED_OR_TEST_EVIDENCE','frozen_rule':c['rule'],'evidence_refs':evidence[c['id']]} for c in oracles],'official_gate_pass':False})
(D/'README.md').write_text('''# Trace lifecycle repair r1 — READY_FOR_CONTROL_REVIEW

The exact existing repaired worker closes OFF, reloads, creates ON paired_init,
then drains the current ON page. The proof now establishes that fixed complete
protocol from literal source profiles, four OFF/ON steps, stable/bound source and
artifacts, actual raw count, and distinct ordered page time origins before it
matches the native initialization inside the ON lifecycle. Both public states
remain separate provenance, even when all their numeric identities are equal.
Missing/foreign/ambiguous scope or native identity stays unresolved; OFF cannot
replace missing or foreign ON. Pixel outcomes never decide trace provenance.

JS emits CORRECTNESS_FINAL_ON_PAGE_V1 and the exact P20 reference. Python checks
the same rule and reconstructs expected scope/states from original bound bytes.
Legacy and non-correctness behavior are unchanged. The7-file worker/consumer/
helper creation tuple is unchanged. Only the5 work-order source paths changed.

Actual old-compiler RED was raw6/init0/committed5/unresolved1 with the original
ambiguous initializer reason. Eighteen new OFFLINE derivations are complete:
17 prior records retain byte-identical canonical/decoded/metrics/external files
and identical non-compiler-provenance summaries. Actual r5 becomes native-only
raw6/init1/committed5/unresolved0, preserving all5 committed chains and all4
QUALIFIED pixels plus2 subject proofs. No metric document/receipt is invented.
Original r5 WORKER_COMPILE result, rejected admission and closed ledger remain.

All9 frozen oracle groups are covered. Full119 Node/76 Python/120 boundary and
diff checks pass. Python includes only the2 allowed malformed native unit checks.
One offline compiler subprocess timed out; its logs/partial outputs are preserved.
The unchanged compiler completed the same input directly, and remaining offline
derivation resumed with closed outer stdin. No browser retry or new sample ran.

Sampling remains34 starts/1264 seconds. No build, valid standalone native probe,
commit/push, UR0c/UR1 or performance/P34 PASS. Exact recorded-correctness reuse is
eligible pending independent control review, not accepted by the producer. If
accepted,5 other correctness cases and30 main runs remain for separate authority.
''')
bundle=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/source['source_tree_sha256']/'trace-lifecycle-repair-r1'
bundle.mkdir(parents=True,exist_ok=False)
shutil.copytree(P,bundle/'package');shutil.copytree(V,bundle/'verification-contract')
shutil.copytree(D,bundle/'execution-and-checks',ignore=shutil.ignore_patterns('result.json','notification-delivery.json','terminal-notification.txt','__pycache__'))
paths=set(delta)|set(profiles)|{'verification/runners/g5/ur0/'+n for n in historical_files}
for name in sorted(paths):
 target=bundle/'source'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/name,target)
(bundle/'source/authorized-source.patch').write_bytes(subprocess.check_output(['git','diff','--',*delta],cwd=ROOT))
files={str(p.relative_to(bundle)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(bundle.rglob('*')) if p.is_file()}
write(bundle/'manifest.json',{'schema':'g5-ur0b-trace-lifecycle-repair-bundle-v1','source_tree_sha256':source['source_tree_sha256'],'files':files,'excluded_envelopes':['manifest.json','result.json'],'envelope_rule':'Result binds manifest; terminal notification supplies both hashes without circular binding.'})
result={'schema_version':1,'task_id':'G5-UR0b-trace-lifecycle-repair-r1','stage':'P32_OFFLINE_VERIFICATION_REPAIR','status':'READY_FOR_CONTROL_REVIEW','continue_execution':False,'return_surface':'CONTROL_REVIEW','result_revision':source['revision'],'dirty':True,'source_tree_sha256':source['source_tree_sha256'],'package_ref':ref(P/'package.json'),'work_order_ref':ref(V/'implementation-request.json'),'verification_spec_ref':work['verification_spec_ref'],'preflight_ref':ref(D/'preflight.json'),'source_identity_ref':ref(D/'full-source-identity.json'),'validation_ref':ref(D/'validation.json'),'oracle_coverage_ref':ref(D/'oracle-coverage.json'),'reverification_ref':ref(D/'reverification.json'),'reuse_assessment_ref':ref(D/'reuse-assessment.json'),'preservation_ref':ref(D/'preservation.json'),'historical_context_ref':ref(D/'historical-compiler-context.json'),'materialized_ref':{'kind':'SHARED_WORKTREE_CONTENT_ADDRESSED_BUNDLE','path':str(bundle),'manifest_sha256':sha(bundle/'manifest.json'),'payload_files':len(files)},'actual_r5_native_only_counts':rows[-1]['counts'],'new_sampling_workers':0,'historical_starts':34,'historical_charged_seconds':1264,'new_builds':0,'new_commit_push':0,'producer_accepts_reuse':False,'old_r5_status':'FROZEN_VERIFICATION_FAILURE_UNCHANGED','old_r5_admission':'REJECTED_UNCHANGED','old_r5_window':'CLOSED_UNCHANGED','next_action':'Independent control review and exact new-compiler recorded-correctness reuse acceptance. Only then authorize5 remaining correctness plus30 main.','performance_or_official_gate_PASS':False}
write(D/'result.json',result);shutil.copyfile(D/'result.json',bundle/'result.json')
for name,value in files.items():assert sha(bundle/name)==value['sha256'] and (bundle/name).stat().st_size==value['bytes']
assert source_identity.identity(ROOT)['source_tree_sha256']==source['source_tree_sha256']
assert (D/'result.json').read_bytes()==(bundle/'result.json').read_bytes()
print(json.dumps({'status':result['status'],'result_ref':ref(D/'result.json'),'package_ref':ref(P/'package.json'),'source_tree_sha256':source['source_tree_sha256'],'materialized_ref':result['materialized_ref'],'groups':9,'recordings':18,'new_browser_workers':0}))
