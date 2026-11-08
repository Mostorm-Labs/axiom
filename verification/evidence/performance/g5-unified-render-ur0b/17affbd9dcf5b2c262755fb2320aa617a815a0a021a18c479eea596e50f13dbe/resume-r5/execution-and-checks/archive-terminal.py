"""Archive the sole r5 observed stop; never launch or repair a worker."""
import hashlib,json,math,re,shutil,subprocess,sys
from collections import Counter
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
D=Path(__file__).resolve().parent
P=ROOT/'.aegis/packages/G5-UR0b-resume-r5'
C=ROOT/'.aegis/results/G5-UR0b/control-review-correctness-final'
O=ROOT/'out/ur0b/resume-r5/execution'
R=O/'raw-runs/correctness-lower_z_translucent_drag'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import source_identity

def load(p):return json.loads(p.read_text())
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(p,value):p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
def copy(p,q):q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)

assert sha(P/'package.json')=='0feefbab71d9b214bbebdd6a456290a461d767734280d73c86677c0fc5cd06c6'
current=source_identity.identity(ROOT)
assert current['revision']=='a00dc4a2d4b4ad3a5ea1353d44794228a8355ca7'
assert current['source_tree_sha256']=='17affbd9dcf5b2c262755fb2320aa617a815a0a021a18c479eea596e50f13dbe'
assert current['files']==load(D/'full-source-identity.json')['files']
assert subprocess.check_output(['git','diff','HEAD','--name-only'],cwd=ROOT)==b''
window=load(D/'window.json');worker=load(R/'worker-result.json')
guard=load(O/'guards/correctness-lower_z_translucent_drag.worker.json')
summary=load(R/'compiled-summary.json');chain=load(R/'canonical-chain.json');decoded=load(R/'decoded.json')
reused=load(D/'reused-prerequisites.json');cleanup=load(D/'process-cleanup.json')
assert window['status']=='CLOSED' and window['terminal_status']=='FROZEN_VERIFICATION_FAILURE'
assert window['new_starts']==1 and window['new_spent_seconds']==math.ceil(guard['elapsed_s'])==13
assert window['charged_seconds']==1264 and window['reserved_seconds']==0
assert guard['reason']=='completed' and guard['returncode']==0 and guard['cleanup_complete']
assert guard['peak_process_family_rss_kib']<=guard['rss_limit_kib']==3145728
assert cleanup['status']=='CLEANUP_VERIFIED' and not cleanup['currently_live_observed_pids']
assert summary['critical_loss']==0 and summary['statistical_loss']==0 and summary['identity_errors']==[]
assert summary['source_stable'] and summary['artifacts_stable']
assert summary['analysis']['mode']=='WORKER_COMPILE'
assert chain['accepted_raw_count']==6 and chain['qualified_committed_count']==5
assert chain['qualified_initialization_count']==0 and chain['unresolved_accepted_count']==1
assert len(decoded['unqualified_completions'])==1
unqualified=decoded['unqualified_completions'][0]
assert unqualified['initialization_unresolved']=='public initialization state absent, foreign or ambiguous'
assert worker['init']['state']==worker['paired_init']['state']
assert worker['status']=='OBSERVED' and worker['correctness_status']=='QUALIFIED'
assert {(e['toggle'],e['step']) for e in worker['correctness']}=={('OFF',0),('OFF',1),('ON',0),('ON',1)}
assert all(e['comparison']['status']=='QUALIFIED' and e['comparison']['interior_mismatched_pixels']==0 for e in worker['correctness'])
for k in ['source_before','source_after']:
 assert worker[k]['revision']==current['revision'] and worker[k]['source_tree_sha256']==current['source_tree_sha256']
assert reused['status']=='ACCEPTED_CURRENT_COMPILER_RECORDED_REUSE' and len(reused['records'])==17
assert {r['mode'] for r in reused['records']}=={'OFFLINE_REVERIFICATION'}

# Audit the exact already accepted inputs without recompilation or resampling.
audited={}
def audit(value,base=ROOT):
 if isinstance(value,dict):
  if isinstance(value.get('path'),str) and re.fullmatch('[0-9a-f]{64}',str(value.get('sha256',''))):
   p=Path(value['path'])
   candidates={p.resolve()} if p.is_absolute() else {(base/p).resolve(),(ROOT/p).resolve()}
   candidates=[q for q in candidates if q.is_file()]
   assert len(candidates)==1,(value['path'],candidates)
   p=candidates[0]
   assert p.is_relative_to(ROOT) and p.is_file(),str(p)
   assert sha(p)==value['sha256'],str(p)
   audited[str(p.relative_to(ROOT))]={'sha256':value['sha256'],'bytes':p.stat().st_size}
  for child in value.values():audit(child,base)
 elif isinstance(value,list):
  for child in value:audit(child,base)
for path,base in [(P/'package.json',P),(D/'mechanism-proofs.json',ROOT),(D/'artifact-provenance.json',ROOT),(C/'continuation-request.json',ROOT),(C/'reuse-acceptance.json',ROOT),(D/'reused-prerequisites.json',ROOT)]:audit(load(path),base)
assert sha(ROOT/'.aegis/results/G5-UR0b/resume-r4/window.json')=='c07a4aa8560b6ab51d472bc38d127d641c43bce9043ff3d10a2cb1a687981c70'
write(D/'input-reference-audit.json',{'status':'EXACT_INPUT_HASHES_PRESERVED','files':audited,'recorded_runs':17,'original_source_groups':dict(Counter(r['recording_source_tree_sha256'] for r in reused['records'])),'fresh_recordings_created_by_this_audit':0,'recompilations':0,'prior_frozen_evidence_mutations':0})
write(D/'execution-source-identity.json',current)
write(D/'blocker-analysis.json',{
 'status':'FROZEN_VERIFICATION_FAILURE','classification':'VERIFICATION_DESIGN_DEFECT',
 'subtype':'CORRECTNESS_RELOAD_INITIALIZATION_IDENTITY_AMBIGUITY',
 'owning_boundary':'Verification correctness lifecycle/initialization evidence binding; independent control review required',
 'actual_admission_error':window['reason'],
 'decoded_unqualified_completion':unqualified,
 'matching_public_initial_states':{k:worker[k]['state'] for k in ['init','paired_init']},
 'observed_cause':'OFF and ON page lifecycles restart the same numeric document/attachment/view/runtime epoch values. Both public initial states match the ON trace; the frozen helper requires exactly one match and refuses the ambiguity.',
 'canonical_chain':{k:chain[k] for k in ['status','accepted_raw_count','qualified_initialization_count','qualified_committed_count','qualified_decoded_count','unresolved_accepted_count','unresolved']},
 'pixel_result':'All four OFF/ON captures QUALIFIED with zero interior mismatches, but this does not qualify the missing canonical initialization chain.',
 'not_observed_as_cause':['timer expiry','RSS overflow','nonzero browser worker exit','trace loss','product pixel mismatch'],
 'repair_executed':False,'retry_executed':False,
 'next_owner_action':'Control/Verification review must bind a native trace to its actual initialization lifecycle without weakening unique identity, inventing a commit, modifying this closed window, or reusing unused budget as retry permission.'
})
executed=[r['run_id'] for r in window['rows'] if r['status']!='MISSING']
missing=[r['run_id'] for r in window['rows'] if r['status']=='MISSING']
assert len(executed)==1 and len(missing)==35
write(D/'terminal-accounting.json',{
 'status':'CLOSED_FROZEN_STOP','planned_ids':[r['run_id'] for r in window['rows']],
 'executed_ids':executed,'admitted_ids':[],'rejected_ids':executed,'missing_ids':missing,
 'missing_correctness':5,'missing_main':30,'rows':window['rows'],
 'historical_starts':33,'historical_charged_seconds':1251,'new_starts':1,
 'new_elapsed_seconds':guard['elapsed_s'],'new_charged_seconds':13,
 'total_starts':34,'total_charged_seconds':1264,'outstanding_reserved_seconds':0,
 'original_cap_seconds':8370,'new_window_nominal_limit_seconds':5940,
 'window_status':'CLOSED','unused_budget_authorizes_retry':False,'unknown_starts':0,
 'new_builds':0,'additional_browser_probes':0,'unplanned_native_probes':0,
 'native_unit_invocations_before_window':{'count':2,'input':'malformed snapshots only','included_in_frozen_full_python_suite':True},
 'native_reference_and_subject_calls':'Only the reference/subject oracle calls inside the one frozen correctness worker; no extra standalone probe.'
})
write(D/'correctness-observation.json',{
 'status':'PIXELS_QUALIFIED_CANONICAL_CHAIN_REJECTED','case_id':worker['branch'],
 'fresh_mode':summary['analysis']['mode'],'worker_status':worker['status'],
 'worker_correctness_status':worker['correctness_status'],'admission_status':'REJECTED',
 'captures':[{'toggle':e['toggle'],'step':e['step'],'comparison':e['comparison'],
  'candidate_sha256':e['candidate_sha256'],'reference_snapshot_sha256':e['reference_snapshot_sha256'],
  'applicability':e.get('applicability')} for e in worker['correctness']],
 'recorded_reuse_is_fresh':False,'performance_or_gate_pass':False
})
(D/'README.md').write_text('''# G5-UR0b resume r5 — FROZEN_VERIFICATION_FAILURE

The sole authorized r5 window was actually launched at a00dc4a2, source
17affbd9dcf5b2c262755fb2320aa617a815a0a021a18c479eea596e50f13dbe.
The first lower-z translucent-drag correctness worker exited 0 in 12.6454s,
below the 90s outer/30s phase and 3 GiB process-family limits. All four OFF/ON
pixel comparisons qualified with zero interior mismatches. The independent
guard completed cleanup; every one of 21 sampled owned PIDs is absent.

Admission failed because one native initialization completion lacks a unique
qualified chain. OFF init and ON paired_init have identical document/epoch
values after page reload. The unchanged initialization reader refuses this
ambiguity. Six accepted native completions yield five committed chains, zero
qualified initialization chains and one unresolved completion. This is a
Verification lifecycle-identity evidence blocker, not an observed pixel RED.

The runner closed the window immediately; 5 correctness and all 30 main rows
are MISSING. One new start is charged ceil(12.6454)=13 seconds. History remains
33 starts/1251 seconds, making 34 starts/1264 seconds total, no reservation.
There was no retry, hidden probe, rebuild, harness change or further commit.

The existing 13 new offline runner tests, complete 114 Node/74 Python tests,
and 120 boundary contracts are preserved rather than rerun in this continuation.
Two malformed native unit checks belong to that earlier Python suite.
Seventeen accepted OFFLINE records retain their individual original source,
artifact and proof identities; they are never promoted to fresh measurements.
The old r4 window and failed records remain unchanged. Control/Verification
must review the lifecycle binding before any separately authorized next window.
No performance, platform, UR0c/UR1 or P34 PASS is issued.
''')

bundle=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/current['source_tree_sha256']/'resume-r5'
bundle.mkdir(parents=True,exist_ok=False)
shutil.copytree(P,bundle/'package')
shutil.copytree(O,bundle/'execution')
shutil.copytree(D,bundle/'execution-and-checks',ignore=shutil.ignore_patterns('result.json','terminal-notification.txt','notification-delivery.json','__pycache__','window.lock'))
shutil.copytree(ROOT/'verification/runners/g5/ur0',bundle/'source/verification/runners/g5/ur0',ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
test=Path('verification/tests/test_g5_ur0_reverified_remaining_runner.py');copy(ROOT/test,bundle/'source'/test)
for name in ['continuation-request.json','reuse-acceptance.json','remaining-run-plan.json','result.json']:
 copy(C/name,bundle/'control-review'/name)
position=ROOT/'.aegis/results/G5-UR0b/control-resume-after-a00dc4a2/position.json'
copy(position,bundle/'control-review/descendant-position.json')
copy(ROOT/'.aegis/results/G5-UR0b/resume-r4/window.json',bundle/'historical/window-r4.json')
for row in reused['records']:
 for key,name in [('summary_ref','compiled-summary.json'),('proof_ref','canonical-chain.json')]:
  p=Path(row[key]['path']);p=p if p.is_absolute() else ROOT/p
  copy(p,bundle/'recorded-reuse'/row['run_id']/name)
files={str(p.relative_to(bundle)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(bundle.rglob('*')) if p.is_file()}
write(bundle/'manifest.json',{
 'schema':'g5-ur0b-resume-r5-terminal-bundle-v1','source_tree_sha256':current['source_tree_sha256'],
 'execution_revision':current['revision'],'files':files,
 'external_frozen_inputs_ref':ref(D/'input-reference-audit.json'),
 'excluded_envelopes':['manifest.json','result.json'],
 'envelope_rule':'Result binds manifest; terminal notification gives both exact hashes without circular binding.'
})
result={
 'schema_version':1,'task_id':'G5-UR0b-resume-r5','stage':'P32_BOUNDED_REAL_BROWSER_REVERIFICATION',
 'status':'FROZEN_VERIFICATION_FAILURE','continue_execution':False,'return_surface':'CONTROL_REVIEW',
 'result_revision':current['revision'],'dirty':True,'dirty_scope':'New excluded result/evidence outputs only; no tracked source change.',
 'source_tree_sha256':current['source_tree_sha256'],'package_ref':ref(P/'package.json'),
 'continuation_request_ref':ref(C/'continuation-request.json'),'descendant_position_ref':ref(position),
 'mechanism_proofs_ref':ref(D/'mechanism-proofs.json'),
 'prepared_mechanism_revision':load(D/'mechanism-proofs.json')['source_revision'],
 'actual_worker_revision':worker['source_before']['revision'],
 'regression_ref':ref(D/'regression.json'),'regressions_reused_not_rerun':True,
 'blocker_analysis_ref':ref(D/'blocker-analysis.json'),'guard_ref':ref(O/'guards/correctness-lower_z_translucent_drag.worker.json'),
 'cleanup_ref':ref(D/'process-cleanup.json'),'window_ref':ref(D/'window.json'),
 'accounting_ref':ref(D/'terminal-accounting.json'),'correctness_ref':ref(D/'correctness-observation.json'),
 'canonical_chain_ref':ref(R/'canonical-chain.json'),'compiled_summary_ref':ref(R/'compiled-summary.json'),
 'exact17_recorded_reuse_ref':ref(D/'reused-prerequisites.json'),'input_reference_audit_ref':ref(D/'input-reference-audit.json'),
 'source_identity_ref':ref(D/'execution-source-identity.json'),
 'materialized_ref':{'kind':'SHARED_WORKTREE_CONTENT_ADDRESSED_BUNDLE','path':str(bundle),'manifest_sha256':sha(bundle/'manifest.json'),'payload_files':len(files)},
 'fresh_browser_starts':1,'new_charged_seconds':13,'total_starts':34,'total_charged_seconds':1264,
 'planned_ids':[r['run_id'] for r in window['rows']],'executed_ids':executed,'admitted_ids':[],'missing_ids':missing,
 'window_closed':True,'outstanding_reservations':0,'retry_authorized':False,
 'blocker_classification':'VERIFICATION_DESIGN_DEFECT','next_action':'Independent control/Verification review of paired correctness initialization lifecycle binding; this window remains closed.',
 'performance_or_official_gate_PASS':False
}
write(D/'result.json',result);copy(D/'result.json',bundle/'result.json')
for name,entry in files.items():assert sha(bundle/name)==entry['sha256'] and (bundle/name).stat().st_size==entry['bytes']
assert (D/'result.json').read_bytes()==(bundle/'result.json').read_bytes()
assert source_identity.identity(ROOT)['source_tree_sha256']==current['source_tree_sha256']
print(json.dumps({'status':result['status'],'result_ref':ref(D/'result.json'),'manifest_ref':ref(bundle/'manifest.json'),'payload_files':len(files),'input_refs_verified':len(audited),'new_starts':1,'new_charged_seconds':13,'missing':35}))
