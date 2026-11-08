"""P20 correction: distinguish recorded page lifecycle from reused numeric IDs."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
read=lambda p:json.loads(Path(p).read_text())
def ref(p):
    p=Path(p);p=p if p.is_absolute() else ROOT/p
    return {'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'identity_kind':'local-content-addressed'}
def put(n,v):(HERE/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
result=read(ROOT/'.aegis/results/G5-UR0b/resume-r5/result.json')
bundle=Path(result['materialized_ref']['path'])
raw=bundle/'execution/raw-runs/correctness-lower_z_translucent_drag'
if not raw.exists():
    candidates=list(bundle.glob('**/correctness-lower_z_translucent_drag/worker-result.json'))
    assert len(candidates)==1,candidates
    raw=candidates[0].parent
run=read(raw/'worker-result.json')
assert run['init']['state']==run['paired_init']['state']
off=run['init']['environment']['timeOrigin'];on=run['paired_init']['environment']['timeOrigin']
assert on>off>0 and len(run['correctness'])==4
inputs={'schema':'ur0b-trace-lifecycle-inputs-v1','original_r5_result_ref':ref('.aegis/results/G5-UR0b/resume-r5/result.json'),
    'original_r5_manifest_ref':ref(bundle/'manifest.json'),'blocker_ref':ref('.aegis/results/G5-UR0b/resume-r5/blocker-analysis.json'),
    'r5_worker_inputs':[ref(raw/n) for n in ['raw-trace.jsonl','worker-result.json','run-inputs.json']],
    'old17_reuse_ref':ref('.aegis/results/G5-UR0b/control-review-correctness-final/reuse-acceptance.json'),
    'old17_input_set_ref':ref('.aegis/verification/G5-UR0b-initialization-v1/evidence-inputs.json'),
    'current_recorded_proof_ref':ref(raw/'canonical-chain.json'),
    'pinned_worker_ref':ref('verification/runners/g5/ur0/worker.mjs'),'pinned_consumer_ref':ref('verification/runners/g5/ur0/consumer.mjs'),
    'actual_public_state_values_equal':True,'actual_off_time_origin_ms':off,'actual_on_time_origin_ms':on,
    'historical_starts':34,'historical_seconds':1264,'old_windows':'CLOSED_UNCHANGED'}
put('evidence-inputs.json',inputs)
oracles=[
 {'id':'ACTUAL-R5-RED','rule':'Original compiler on immutable actual r5 data must expose raw6/init0/committed5/unresolved1 and the foreign-or-ambiguous public initialization reason. No merely missing-new-field RED.'},
 {'id':'ACTUAL-SCOPED-POSITIVE','rule':'Same actual raw/input bytes under the corrected known-profile lifecycle rule yield raw6/init1/committed5/unresolved0; the five committed chains/receipts, four QUALIFIED pixel results, subject proofs and observations are unchanged.'},
 {'id':'CROSS-PAGE-ALIAS','rule':'Equal public IDs and entire state across distinct OFF/ON page origins are valid; select ON paired_init only after the fixed final-ON-drain protocol is qualified. Numeric equality alone never selects a lifecycle.'},
 {'id':'UNKNOWN-SCOPE','rule':'Unknown/mismatched worker/consumer/helper hash, missing paired state/page-origin metadata, same/reversed/nonfinite origins, incomplete/duplicate/foreign pair, wrong branch/mode, missing/lossy trace or wrong raw record count remains UNRESOLVED.'},
 {'id':'WITHIN-SCOPE-IDENTITY','rule':'Within the chosen ON lifecycle, wrong document/epoch/attachment/view, nonempty/restored/nonzero state, nonzero operation, duplicate same eligible state path or ambiguous native owner fails. The OFF state cannot substitute for absent/foreign ON even if its values match.'},
 {'id':'OUTCOME-INDEPENDENCE','rule':'Trace source qualification depends on complete protocol and exact source, not pixel green. Complete FAILED/MISSING pixel comparisons cannot manufacture qualification/PASS; native provenance may be resolved while separate pixel/case admission still fails.'},
 {'id':'PRODUCER-READER-BINDING','rule':'JS derived trace_scope and Python validation agree; forged scope/path/timeOrigin/authority ref or discrepancy from actual worker-result/raw/input bytes is rejected by strict bound admission. No current-file-hash wildcard.'},
 {'id':'LEGACY17-INVARIANCE','rule':'Recompile17 immutable complete prior records separately. Canonical/decoded/metrics/external bytes and non-provenance summaries remain identical to their accepted baselines; original modes/source/artifacts and committed receipts unchanged.'},
 {'id':'HISTORICAL-TEST-CONTEXT','rule':'Existing r5 tests of its old current-compiler acceptance use the exact a00dc4a2 historical compiler bytes for positive or later-stage negative cases. Actual new compiler must still reject that old packet. Preserve real checks and all guard/ledger tests; no success mocks/skip/xfail.'}
]
put('oracle-cases.json',{'schema':'ur0b-trace-lifecycle-oracles-v1','cases':oracles,'scope':'Only the initialization trace-lifecycle proof defect; previous pixel/transfer12 groups remain applicable and unchanged.'})
spec={
 'schema':'ur0b-trace-lifecycle-verification-v1','status':'CURRENT_FOR_OFFLINE_CORRECTNESS_LIFECYCLE_REPAIR',
 'role':'aegis-verification / P20 control',
 'authority':{'basis':'User authorized this conversation to design/control, adjacent thread to implement, continue the baseline, and proactively notify terminal results.',
    'inherited_correctness_spec_ref':ref('.aegis/verification/G5-UR0b-correctness-harness-v1/verification-spec.json'),
    'inherited_fresh_empty_contract_ref':ref('.aegis/verification/G5-UR0b-initialization-v1/completion-contract.json'),
    'actual_failed_result_ref':inputs['original_r5_result_ref']},
 'objective':'Bind an initialization public state to the actual browser-page lifecycle that produced the native trace, then apply all existing unique-owner/fresh-empty/canonical checks.',
 'non_goals':['No Runtime/SDK/public API/collector/worker/consumer/native schema change','No new browser start, retry or build','No change to pixel or timing/budget limits','No rewriting r5 rejected/CLOSED history'],
 'required_analysis':[
    'Current exact worker3e3d5e... performs OFF correctness -> closes OFF -> reloads -> records paired_init from ON -> completes ON -> drains raw-trace.jsonl from the current ON page, then closes/drains cleanup.',
    'Public and native numeric identities may restart on module reload; equality across distinct pages does not create an ambiguity inside one trace lifecycle.',
    'Actual OFF/ON timeOrigin values are distinct and ordered; this corroborates the known creation/reload protocol, not a new native epoch or semantic identity.'],
 'trace_lifecycle_contract':{
    'scheme':'CORRECTNESS_FINAL_ON_PAGE_V1',
    'applicability':'Only the existing exact repairedInitializationProfile tuple, mode correctness, toggle ON and one of the existing six branch cases. Original old profile and all non-correctness modes retain existing init-only behavior unchanged.',
    'source_protocol':'Do not trust an input label or generation0 alone. Exact pinned worker/consumer/helper source proves the single OFF-close/reload/ON-init/final-current-page-drain sequence. Require OBSERVED complete worker, source/artifacts stable, complete four unique OFF0/OFF1/ON0/ON1 comparison entries for the declared case, and bound actual raw trace/input/worker bytes. Comparison outcomes may be QUALIFIED/FAILED/MISSING; do not make provenance depend on pixel PASS.',
    'page_evidence':'Both actual init.environment.timeOrigin and paired_init.environment.timeOrigin finite positive, ON strictly greater than OFF; paired_init.state exists. Raw record count agrees with run.trace.records; existing critical-loss checks stay. Preserve pending_gpu metadata without adding a new source-lifecycle Gate for it. Missing/contradictory protocol remains unresolved.',
    'qualifying_state_path':'worker-result.json/paired_init/state',
    'selection':'First establish final ON trace lifecycle, then require a unique state/owner match within that lifecycle. Retain init and paired_init as separate provenance entries even if values equal. Never collapse equal values, select first/last by convenience, prefer a matching OFF state, or treat two same-lifecycle candidates as unique.',
    'identity':'All existing complete native runtime/lifetime/attachment/document/epoch/resource/view, successful attempt, sequence and input hash checks stay. Public fresh-empty0/no-operation constraints stay; no Operation/Scene mutation or ID is invented.',
    'derived_proof':'Emit an explicit trace_scope with scheme, qualifying_state_path, off_page_time_origin_ms, trace_page_time_origin_ms, and exact ref to this Verification spec. It is derived host evidence backed by the fixed profile and recorded bytes, not a fabricated native event or new semantic epoch.',
    'python_consumer':'Validate the same scoped rule in require_canonical_chain; require_bound_compilation additionally reconstructs expected scope/public states from actual worker-result and checks exact input/profile/spec hashes. Reject mismatches and unknown scope. Existing source/profile literal tuple remains unchanged.'},
 'actual_r5_expected':{
    'recording_source':'17affbd9dcf5b2c262755fb2320aa617a815a0a021a18c479eea596e50f13dbe',
    'original_mode':'WORKER_COMPILE','new_derived_mode':'OFFLINE_REVERIFICATION',
    'canonical_scope':'NATIVE_ONLY','accepted_raw_count':6,'qualified_initialization_count':1,'qualified_committed_count':5,
    'qualified_decoded_count':5,'metric_receipt_count':None,'unresolved_accepted_count':0,
    'metrics_schema':'No measurement exists for this correctness worker. Leave metrics MISSING/not applicable; do not fabricate a metrics document or make native-only proof pretend to have mutation metric receipts.',
    'immutable':'All original raw/worker/input bytes, six accepted records, five committed chains, pixels/subjects, artifact identities, timings, guard and original r5 rejected status/window stay unchanged.',
    'reuse':'Report eligible-pending-independent-review for this one recorded correctness case; do not admit it into the old closed window or self-issue reuse/P34/performance PASS.'},
 'evidence_qualification':{'failure_mode':'Cross-page numeric identity alias is misclassified as ambiguous initialization, or a naive fix chooses an unrelated runtime state.',
    'impact':'Blocks a valid real browser prerequisite or can hide a foreign/stale initialization if uniqueness is simply weakened.',
    'existing_independent_coverage':'Exact source profile, public states, full native owner/frame chains and immutable recording bindings.',
    'residual_proof_gap':'Scope the public state selection to the page that the fixed worker actually drained.',
    'new_evidence_unique_detection_value':'Actual equal-state distinct-page recording plus wrong/unknown lifecycle negatives directly isolate this failure mode.',
    'blocking_justification':'Fix of the existing all-accepted-record reconciliation obligation; no new product gate.'},
 'required_output':['Actual-record RED then scoped corrected derivation','Nine frozen positive/negative groups and full regression',
    '17 prior invariance plus one actual r5 native-only recompile, with honest source/provenance separation','Exact result/bundle and prospective reuse assessment'],
 'quality_evidence_gate':{'required':['All nine groups','Existing full Node/Python/boundary and diff checks','Unchanged old recordings, worker/consumer/profiles, existing timers/budgets and readers outside named repair scope'],'official_gate_pass':False},
 'handoff':{'owner':'Adjacent implementation conversation; materialize exact P31 and execute all offline work without another approval if preflight is clean.'},
 'oracle_ref':ref(HERE/'oracle-cases.json'),'inputs_ref':ref(HERE/'evidence-inputs.json'),
 'supersession':'Narrow additive correction of cross-page uniqueness interpretation in the previous correctness paired-state contract. Original fresh-empty/native ownership requirements, old results, all other12-group pixel/transport contracts and old profile stay unchanged.'}
put('verification-spec.json',spec)
paths=['verification/runners/g5/ur0/initialization_basis.mjs','verification/runners/g5/ur0/repair_runner.py',
 'verification/tests/g5_ur0_initialization.test.mjs','verification/tests/test_g5_ur0_repair_runner.py',
 'verification/tests/test_g5_ur0_reverified_remaining_runner.py']
request={
 'schema':'ur0b-trace-lifecycle-repair-work-order-v1','status':'AUTHORIZED_OFFLINE_REPAIR',
 'owner':'aegis-implementation in adjacent thread','stage_request':['P30/P31 exact package','P32 complete offline repair and return'],
 'repository':{'provider':'github','full_name':'Mostorm-Labs/axiom'},'worktree':str(ROOT),
 'execution_ref':'refs/heads/codex/geometry-navigation-integration',
 'task_anchor':{'revision':'f651523a40c9b383b7d8e2115fcd32f75ddc38a7','relation':'ancestor'},
 'resume_cursor':{'revision':'a00dc4a2d4b4ad3a5ea1353d44794228a8355ca7','accepted_predecessor_source_sha256':inputs['original_r5_result_ref'] and result['source_tree_sha256'],
    'completed_through':'r5 one real worker completed with all4pixels/subjects QUALIFIED, admission rejected solely for cross-page initialization alias; original window CLOSED.',
    'next_action':'Freeze P31 and repair the scoped proof/readers, historical test contexts, actual-r5 derivation and17 invariance.'},
 'verification_spec_ref':ref(HERE/'verification-spec.json'),'oracle_ref':ref(HERE/'oracle-cases.json'),'evidence_inputs_ref':ref(HERE/'evidence-inputs.json'),
 'package_to_materialize':'.aegis/packages/G5-UR0b-trace-lifecycle-repair-r1/package.json',
 'authorization':'Existing human design/control/implementation and continuation delegation covers this concrete verification repair. No additional confirmation after a clean package preflight.',
 'source_scope':paths,
 'required_changes':[
    'Derive the final ON-page trace scope only for the exact existing repaired correctness profile and complete fixed reload protocol; then apply uniqueness/native/fresh-empty checks in that scope. Preserve all states and full accepted population.',
    'Keep JS producer/Python reader and bound actual-record reconstruction coherent, with an exact reference to this P20 scope contract. Worker and7-file creation-profile literals are read-only; no new worker tuple is needed.',
    'Run actual old r5 RED; recompile that exact immutable recording to new outputs under the corrected rule, expected native-only6=1+5+0 with all5committed chains and all pixel/subject results unchanged.',
    'Recompile the17 previous records into new outputs and prove invariance except explicit compiler provenance.',
    'Update only compiler-context-dependent r5 regression fixtures to use the exact a00dc4a2 Git compiler bytes when exercising the historical acceptance, retain actual-new-compiler rejection and all other record/guard/ledger negatives. Do not weaken or edit the old r5 runner/acceptance.'],
 'read_only':['Runtime/SDK/collector/worker.mjs/consumer.mjs/all seven repaired-profile files and literal identity tuple',
    'All original packages/recordings/results/windows/ledgers; all P20 files including this directory',
    'All pixel, timing, pressure, schema, budget and canonical mutation authorities; all orchestration readers except the named repair_runner proof path'],
 'new_control_paths':['.aegis/packages/G5-UR0b-trace-lifecycle-repair-r1/**'],
 'control_delta':'This P20 directory and the new package are authorized additive control inputs; compute actual current source before code, do not require equality with the predecessor hash.',
 'output_paths':['.aegis/results/G5-UR0b/trace-lifecycle-repair-r1/**','out/ur0b/trace-lifecycle-repair-r1/**',
    'verification/evidence/performance/g5-unified-render-ur0b/<new-source-sha>/trace-lifecycle-repair-r1/**'],
 'required_tests':['Nine frozen oracle groups including actual real-record RED',
    'node --test verification/tests/g5_*.test.mjs verification/sdk_web_external/pointer_input_test.mjs verification/sdk_web_external/control_api_test.mjs',
    "python3.12 -m unittest discover -s verification/tests -p 'test_g5_*.py'",'python3.12 tools/check_runtime_boundaries.py --root .',
    'All18 frozen recorded inputs (17 invariants + one corrected native-only actual r5 case) derived into a new directory','git diff --check'],
 'forbidden':['Any new browser/smoke/probe/recording/build/valid native qualification call; only the2 existing malformed-snapshot unit checks are allowed/countable',
    'Choosing init state by equal values, arbitrary first/last, ignoring unknown lifecycle or dropping a raw record',
    'Forging an Operation/Scene publication/native epoch, changing input IDs, metrics counts or pixel outcomes',
    'Reopening/admitting into r5 or changing original rejected/CLOSED history',
    'Changing worker/helpers/profile/source-identity exclusions or any limits; commit/push; UR0c/UR1'],
 'closure':{'all_of':['All frozen scope and9oracle groups complete','All17 invariant plus r5 expected6=1+5+0 native-only proof with raw/pixels untouched',
    'Full relevant regression green and honest source/scope/proof/reuse applicability report','New content-addressed result/evidence with exact hashes and proactive terminal notification'],
    'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW','stop_after':'OFFLINE_TRACE_LIFECYCLE_REPAIR',
    'terminal_blockers':['AUTHORITY_CONFLICT','TASK_PACKAGE_DEFECT','VERIFICATION_DESIGN_DEFECT','FROZEN_VERIFICATION_FAILURE','BLOCKED_EVIDENCE','ENVIRONMENT_BLOCKER']},
 'new_sampling_workers_max':0,'historical_starts':34,'historical_seconds':1264,'old_windows':'CLOSED_UNCHANGED',
 'future_work':'After independent review, the one actual corrected correctness record may qualify for exact reuse; then5othercorrectness+30main remain. This package does not authorize those starts or self-accept that reuse.',
 'commit_push':False,'official_gate_pass':False,
 'notify_terminal':{'required':True,'target_thread':'01a1b6f3-e69a-7fe2-8d9b-0222e8741f03','hostId':'local',
    'when':['READY_FOR_CONTROL_REVIEW','any actual blocker/frozen stop'],
    'contents':['task/package/status','actual revision/dirty/source SHA','result/bundle/manifest hashes',
    'nine groups/18-record derivation/committed invariance/actual-case native-only counts and unchanged pixels','sampling0, cumulative34/1264, all remaining work']}}
put('implementation-request.json',request)
print(json.dumps({'status':'READY_TO_DISPATCH','spec_ref':ref(HERE/'verification-spec.json'),'work_order_ref':ref(HERE/'implementation-request.json'),
    'actual_record_inputs':len(inputs['r5_worker_inputs']),'actual_off_origin':off,'actual_on_origin':on,'mutable_source_paths':len(paths)}))
