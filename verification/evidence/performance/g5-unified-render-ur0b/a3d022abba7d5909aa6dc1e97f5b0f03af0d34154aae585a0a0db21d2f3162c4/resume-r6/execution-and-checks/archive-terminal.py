"""Archive the sole r6 frozen stop without repair, retry, or sampling."""
import hashlib,json,math,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
P=ROOT/'.aegis/packages/G5-UR0b-resume-r6'
C=ROOT/'.aegis/results/G5-UR0b/control-review-trace-lifecycle-r1'
O=ROOT/'out/ur0b/resume-r6/execution';R=O/'raw-runs/correctness-permanent_live_mask'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'));import source_identity
load=lambda p:json.loads(p.read_text())
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def copy(p,q):q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
assert sha(P/'package.json')=='0dc1abe9149759d25bbea9b37565094e943b23fd583f995194324818de44f920'
current=source_identity.identity(ROOT);assert current['revision']=='a00dc4a2d4b4ad3a5ea1353d44794228a8355ca7'
assert current['source_tree_sha256']=='a3d022abba7d5909aa6dc1e97f5b0f03af0d34154aae585a0a0db21d2f3162c4'
assert current['files']==load(D/'full-source-identity.json')['files']
w=load(D/'window.json');run=load(R/'worker-result.json');s=load(R/'compiled-summary.json');p=s['canonical_chain'];decoded=load(R/'decoded.json')
g=load(O/'guards/correctness-permanent_live_mask.worker.json');reused=load(D/'reused-prerequisites.json');cleanup=load(D/'process-cleanup.json')
assert w['status']=='CLOSED' and w['terminal_status']=='FROZEN_VERIFICATION_FAILURE'
assert w['new_starts']==1 and w['new_spent_seconds']==math.ceil(g['elapsed_s'])==12
assert w['charged_seconds']==1276 and w['reserved_seconds']==0
assert g['reason']=='completed' and g['returncode']==0 and g['cleanup_complete'] and g['remaining_owned_pids']==[]
assert g['peak_process_family_rss_kib']<=g['rss_limit_kib']==3145728
assert cleanup['status']=='CLEANUP_VERIFIED' and not cleanup['currently_live_observed_pids']
assert s['critical_loss']==0 and s['statistical_loss']==0 and s['identity_errors']==[]
assert s['source_stable'] and s['artifacts_stable'] and s['analysis']['mode']=='WORKER_COMPILE'
assert (p['accepted_raw_count'],p['qualified_initialization_count'],p['qualified_committed_count'],p['unresolved_accepted_count'])==(7,1,5,1)
assert decoded['missing']==[{'reason':'accepted completion outside observed work','seq':'22'}]
assert not any(f['work']=='1' for f in decoded['frames'])
assert p['accepted_records'][0]=={'completion_seq':'22','work_id':'1','classification':'UNRESOLVED'}
assert p['initialization_chains'][0]['initialization_basis']['trace_scope']['scheme']=='CORRECTNESS_FINAL_ON_PAGE_V1'
assert run['correctness_status']=='QUALIFIED' and len(run['correctness'])==4
assert all(e['comparison']['status']=='QUALIFIED' and e['comparison']['interior_mismatched_pixels']==0 for e in run['correctness'])
for k in ['source_before','source_after']:assert run[k]['source_tree_sha256']==current['source_tree_sha256'] and run[k]['revision']==current['revision']
assert reused['status']=='ACCEPTED_SCOPED_17_PLUS_ONE_REUSE' and len(reused['records'])==17 and len(reused['recorded_correctness_cases'])==1
assert reused['recorded_correctness_cases'][0]['case_id']=='lower_z_translucent_drag' and reused['recorded_correctness_cases'][0]['fresh'] is False
assert sha(ROOT/'.aegis/results/G5-UR0b/resume-r5/window.json')=='2cc5de5d4dce51651ecf6c273a6136c60bb4e32fd7e649e7d1b310a12844dc3f'
assert sha(ROOT/'.aegis/results/G5-UR0b/resume-r4/window.json')=='c07a4aa8560b6ab51d472bc38d127d641c43bce9043ff3d10a2cb1a687981c70'
audited={}
def audit(x,base=ROOT):
 if isinstance(x,dict):
  if 'path' in x and 'sha256' in x:
   path=Path(x['path']);candidates={path.resolve()} if path.is_absolute() else {(base/path).resolve(),(ROOT/path).resolve()};candidates=[q for q in candidates if q.is_file()]
   assert len(candidates)==1,(x['path'],candidates);q=candidates[0];assert q.is_relative_to(ROOT) and sha(q)==x['sha256'];audited[str(q.relative_to(ROOT))]={'sha256':x['sha256'],'bytes':q.stat().st_size}
  for child in x.values():audit(child,base)
 elif isinstance(x,list):
  for child in x:audit(child,base)
for path,base in [(P/'package.json',P),(D/'mechanism-proofs.json',ROOT),(D/'artifact-provenance.json',ROOT),(C/'continuation-request.json',ROOT),(C/'reuse-acceptance.json',ROOT),(D/'reused-prerequisites.json',ROOT)]:audit(load(path),base)
write(D/'input-reference-audit.json',{'status':'EXACT_INPUT_HASHES_PRESERVED','files':audited,'metrics_prerequisites':17,'native_only_recorded_correctness':1,'recorded_is_fresh':False,'recompilations_in_this_audit':0,'prior_frozen_evidence_mutations':0})
write(D/'execution-source-identity.json',current)
raw=[json.loads(line) for line in (R/'raw-trace.jsonl').read_text().splitlines() if line]
write(D/'blocker-analysis.json',{
 'status':'FROZEN_VERIFICATION_FAILURE','classification':'BLOCKED_EVIDENCE',
 'subtype':'ACCEPTED_NATIVE_COMPLETION_OUTSIDE_OBSERVED_WORK',
 'actual_admission_error':w['reason'],'decoded_missing':decoded['missing'],
 'unresolved_raw_record':next(row for row in raw if row['seq']=='22'),
 'unresolved_accepted_record':p['accepted_records'][0],
 'observed_frame_work_ids':[f['work'] for f in decoded['frames']],
 'qualified_initialization_trace_scope':p['initialization_chains'][0]['initialization_basis']['trace_scope'],
 'canonical_counts':{k:p[k] for k in ['accepted_raw_count','qualified_initialization_count','qualified_committed_count','qualified_decoded_count','metric_receipt_count','unresolved_accepted_count']},
 'facts':'The final-ON-page initialization proof now qualifies work2. A separate accepted work1 completion at seq22 has no matching observed frame/work chain under the current decoder. All7 accepted records remain in the denominator; one is unresolved.',
 'cause_not_established':'This recording does not by itself establish the upstream reason for the missing work1 frame/capture provenance. Control/Verification must review the observation-window/initial-work boundary; no product or collector fix is inferred here.',
 'not_observed_as_failure':['cross-page initialization ambiguity','timer expiry','RSS overflow','worker nonzero exit','trace critical/statistical loss','pixel mismatch'],
 'repair_or_retry_executed':False,'next_owner_action':'Independent control/Verification classification and observation-boundary review. Keep r6 CLOSED; do not synthesize a frame, drop the accepted record or reuse unused allowance as retry permission.'
})
executed=[r['run_id'] for r in w['rows'] if r['status']!='MISSING'];missing=[r['run_id'] for r in w['rows'] if r['status']=='MISSING']
assert len(executed)==1 and len(missing)==34
write(D/'terminal-accounting.json',{'status':'CLOSED_FROZEN_STOP','planned_ids':[r['run_id'] for r in w['rows']],'executed_ids':executed,'admitted_ids':[],'rejected_ids':executed,'missing_ids':missing,'missing_correctness':4,'missing_main':30,'rows':w['rows'],'historical_starts':34,'historical_charged_seconds':1264,'new_starts':1,'new_elapsed_seconds':g['elapsed_s'],'new_charged_seconds':12,'total_starts':35,'total_charged_seconds':1276,'outstanding_reserved_seconds':0,'original_cap_seconds':8370,'new_window_nominal_limit_seconds':5850,'window_status':'CLOSED','unused_budget_authorizes_retry':False,'unknown_starts':0,'new_builds':0,'extra_browser_probes':0,'unplanned_native_probes':0,'native_unit_invocations_before_window':{'count':2,'input':'malformed snapshots only','included_in_full_python_suite':True},'recorded_correctness_reused_not_restarted':['lower_z_translucent_drag']})
write(D/'correctness-observation.json',{'status':'PIXELS_QUALIFIED_CANONICAL_CHAIN_REJECTED','case_id':run['branch'],'fresh_mode':s['analysis']['mode'],'worker_correctness_status':run['correctness_status'],'admission_status':'REJECTED','captures':[{'toggle':e['toggle'],'step':e['step'],'comparison':e['comparison'],'candidate_sha256':e['candidate_sha256'],'reference_snapshot_sha256':e['reference_snapshot_sha256']} for e in run['correctness']],'recorded_reuse_is_fresh':False,'performance_or_gate_pass':False})
(D/'README.md').write_text('''# G5-UR0b resume r6 — FROZEN_VERIFICATION_FAILURE

The one authorized r6 window actually ran at a00dc4a2/source a3d022ab.
All16 new offline runner tests,119 Node/92 Python/120 boundary checks passed.
Exact17 metrics/native prerequisites plus one recorded native-only lower-z
case were independently accepted and verified before claim. Lower-z was reused
as recorded and never restarted; old r5 remains REJECTED/CLOSED.

The first new permanent_live_mask worker completed in11.4404s, below the90s
outer/30s phase and3GiB family limits. All4 OFF/ON pixel captures qualified
with zero interior mismatches. Final-ON initialization provenance qualifies
work2, so the prior cross-page alias is resolved in this actual recording.

A different accepted completion, work1 at seq22, lacks a matching observed
frame/work chain. Native counts are7=init1+committed5+unresolved1. The decoder
reports accepted completion outside observed work; zero trace loss and zero
identity errors do not provide the missing provenance. No accepted record was
dropped and no frame/Operation/Scene was fabricated. Upstream cause remains
for independent observation-boundary review; no reader or collector was changed.

The runner immediately closed.4 correctness and all30 main rows are MISSING;
main never started. One new start is charged ceil(11.4404)=12s. History34/1264
is preserved, making35 starts/1276s total, zero reservations. Cleanup completed;
all18 observed owned PIDs are absent. No retry, extra probe, build, further
commit/push or UR0c/UR1. No performance/platform/P34 PASS is issued.
''')
bundle=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/current['source_tree_sha256']/'resume-r6';bundle.mkdir(parents=True,exist_ok=False)
shutil.copytree(P,bundle/'package');shutil.copytree(O,bundle/'execution')
shutil.copytree(D,bundle/'execution-and-checks',ignore=shutil.ignore_patterns('result.json','terminal-notification.txt','notification-delivery.json','__pycache__','window.lock'))
shutil.copytree(ROOT/'verification/runners/g5/ur0',bundle/'source/verification/runners/g5/ur0',ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
test=Path('verification/tests/test_g5_ur0_scoped_remaining_runner.py');copy(ROOT/test,bundle/'source'/test)
for name in ['continuation-request.json','reuse-acceptance.json','remaining-run-plan.json','result.json']:copy(C/name,bundle/'control-review'/name)
for name in ['resume-r4','resume-r5']:copy(ROOT/'.aegis/results/G5-UR0b'/name/'window.json',bundle/'historical'/(name+'-window.json'))
for row in reused['records']:
 for key,name in [('summary_ref','compiled-summary.json'),('proof_ref','canonical-chain.json')]:
  q=Path(row[key]['path']);q=q if q.is_absolute() else ROOT/q;copy(q,bundle/'recorded-reuse'/row['run_id']/name)
for row in reused['recorded_correctness_cases']:
 for key,name in [('summary_ref','compiled-summary.json'),('proof_ref','canonical-chain.json')]:
  q=Path(row[key]['path']);q=q if q.is_absolute() else ROOT/q;copy(q,bundle/'recorded-correctness'/row['case_id']/name)
files={str(q.relative_to(bundle)):{'sha256':sha(q),'bytes':q.stat().st_size} for q in sorted(bundle.rglob('*')) if q.is_file()}
write(bundle/'manifest.json',{'schema':'g5-ur0b-resume-r6-terminal-bundle-v1','source_tree_sha256':current['source_tree_sha256'],'execution_revision':current['revision'],'files':files,'excluded_envelopes':['manifest.json','result.json'],'envelope_rule':'Result binds manifest; terminal notice supplies both hashes without circular binding.'})
result={'schema_version':1,'task_id':'G5-UR0b-resume-r6','stage':'P36_BOUNDED_REAL_BROWSER_REVERIFICATION','status':'FROZEN_VERIFICATION_FAILURE','continue_execution':False,'return_surface':'CONTROL_REVIEW','result_revision':current['revision'],'dirty':True,'dirty_scope':'Accepted prior dirty work preserved; two new authorized source files and package plus excluded outputs.','source_tree_sha256':current['source_tree_sha256'],'package_ref':ref(P/'package.json'),'continuation_request_ref':ref(C/'continuation-request.json'),'mechanism_proofs_ref':ref(D/'mechanism-proofs.json'),'source_scope_ref':ref(D/'source-scope-audit.json'),'regression_ref':ref(D/'regression.json'),'blocker_analysis_ref':ref(D/'blocker-analysis.json'),'guard_ref':ref(O/'guards/correctness-permanent_live_mask.worker.json'),'cleanup_ref':ref(D/'process-cleanup.json'),'window_ref':ref(D/'window.json'),'accounting_ref':ref(D/'terminal-accounting.json'),'correctness_ref':ref(D/'correctness-observation.json'),'canonical_chain_ref':ref(R/'canonical-chain.json'),'compiled_summary_ref':ref(R/'compiled-summary.json'),'exact17_plus_one_reuse_ref':ref(D/'reused-prerequisites.json'),'input_reference_audit_ref':ref(D/'input-reference-audit.json'),'source_identity_ref':ref(D/'execution-source-identity.json'),'materialized_ref':{'kind':'SHARED_WORKTREE_CONTENT_ADDRESSED_BUNDLE','path':str(bundle),'manifest_sha256':sha(bundle/'manifest.json'),'payload_files':len(files)},'planned_ids':[r['run_id'] for r in w['rows']],'executed_ids':executed,'admitted_ids':[],'missing_ids':missing,'fresh_browser_starts':1,'new_charged_seconds':12,'total_starts':35,'total_charged_seconds':1276,'window_closed':True,'outstanding_reservations':0,'retry_authorized':False,'blocker_classification':'BLOCKED_EVIDENCE','next_action':'Independent control/Verification review of accepted work1 completion outside observed frame/work. r6 remains closed.','performance_or_official_gate_PASS':False}
write(D/'result.json',result);copy(D/'result.json',bundle/'result.json')
for name,item in files.items():assert sha(bundle/name)==item['sha256'] and (bundle/name).stat().st_size==item['bytes']
assert (D/'result.json').read_bytes()==(bundle/'result.json').read_bytes()
assert source_identity.identity(ROOT)['source_tree_sha256']==current['source_tree_sha256']
print(json.dumps({'status':result['status'],'result_ref':ref(D/'result.json'),'manifest_ref':ref(bundle/'manifest.json'),'payload_files':len(files),'input_refs_verified':len(audited),'new_starts':1,'new_charged_seconds':12,'missing':34}))
