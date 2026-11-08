"""Close offline repair review and issue bounded frozen-evidence continuation."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
OUT=Path(__file__).resolve().parent
read=lambda p:json.loads(Path(p).read_text())
def ref(p):
    p=Path(p)
    return {'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'identity_kind':'local-content-addressed'}
def put(n,v): (OUT/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')

identity=read(OUT/'identity-check.json')
records=read(OUT/'recorded-reuse-check.json')
assert records['status']=='ALL17_INDEPENDENTLY_RECONCILED' and len(records['rows'])==17
assert '# pass 114' in (OUT/'node.log').read_text() and '# fail 0' in (OUT/'node.log').read_text()
assert 'Ran 61 tests' in (OUT/'python.log').read_text() and (OUT/'python.log').read_text().rstrip().endswith('OK')
assert 'passed (120 contract files)' in (OUT/'boundary.log').read_text()

reuse={
    'schema':'ur0b-independent-current-compiler-reuse-v1',
    'status':'ACCEPTED_EXACT_RECORDED_PREREQUISITES_FOR_CURRENT_COMPILER',
    'owner':'aegis-gate-review / independent control',
    'reviewer_thread':'01a1b6f3-e69a-7fe2-8d9b-0222e8741f03',
    'producer_thread':'01a0dacd-61e3-7ac3-a2cb-ab7532f1b28c',
    'repository':{'provider':'github','full_name':'Mostorm-Labs/axiom'},
    'compiler_source_reviewed_at':identity['source_tree_sha256'],
    'compiler_file_sha256':records['compiler_file_sha256'],
    'repaired_worker_and_helpers':records['repaired_profile'],
    'independent_reconciliation_ref':ref(OUT/'recorded-reuse-check.json'),
    'independent_materialization_scope_ref':ref(OUT/'identity-check.json'),
    'old_independent_overhead_acceptance_ref':ref(ROOT/'.aegis/results/G5-UR0b/control-review-init-repair-r1/reuse-acceptance.json'),
    'old_independent_overhead_calculation_ref':ref(ROOT/'.aegis/results/G5-UR0b/control-review-resume-r3-terminal/overhead-check.json'),
    'records':records['rows'],
    'accepted_prerequisites':{'diagnostic_off_on':True,'six_pair_overhead':True,'realtime_hz':[60,120,240]},
    'applicability':[
        'Exactly the17 listed immutable recordings and the current8-file compiler plus exact repaired worker/helper tuple; retain each recording source, OFFLINE mode, raw/input/artifact identity and accepted proof counts.',
        'Canonical chains, decoded data, metrics and external statistics are byte-equal to already accepted baselines, independently compared; no new measured timings or primary data are created.',
        'Current worker changes are cold-correctness-gated; original primary input/calibration/server/shim/native observer/artifacts and measured workloads remain unchanged. Snapshot exhaustion validation is cold transport and is not primary measured work.',
        'Original overhead values/ABBA order/MAD/limits stay unchanged. This accepts recorded applicability, not an unmeasured new cohort.',
        'Any compiler/profile/artifact/input/primary-workload change requires an explicit applicability decision. Old packets/readers remain immutable and continue to reject stale compiler bindings.',
        'New main rows must use the exact approved32/96 or64/192 SDK artifacts, their original budgets and original corpus/SDK semantics.'
    ],
    'historical_r4_packet_now_valid':False,
    'recorded_is_fresh':False,'new_sampling_authorized_by_this_record':False,
    'full_ur0b_or_performance_or_P34_PASS':False
}
put('reuse-acceptance.json',reuse)

result={
    'schema_version':1,'task_id':'G5-UR0b-correctness-independent-control-review',
    'primary_owner':'aegis-gate-review','status':'ACCEPTED_OFFLINE_REPAIR_AND_EXACT_RECORDED_REUSE',
    'subject_refs':[s['result_ref'] for s in identity['subjects']],
    'repository_revision':identity['repository_revision'],'source_tree_sha256':identity['source_tree_sha256'],'dirty':True,
    'frozen_requirement_audit':{'passed':['P20 fixed subject/HUD rules and bounded cold transport implemented within r2 allowance',
        'Exact additive JS/Python creation profile with unique paired-init ownership; old profile retained',
        'P36 changed only stale regression fixture, preserving actual current-compiler mismatch rejection',
        '12 frozen offline groups,17-recording invariance, mandatory regressions and immutable old failure records'], 'failed':[]},
    'frozen_evidence_audit':{'payload_files_verified':220,'independent_recordings_reconciled':17,
        'node_passed':114,'python_passed':61,'boundary_contracts':120,
        'new_browser_workers':0,'new_builds':0,'valid_native_qualification_probes':0,'malformed_native_unit_checks_in_reviewer_python_suite':2},
    'repository_reality_audit':{'runtime_sdk_consumer_collector_unchanged':True,'one_test_only_p36_delta_verified':True,
        'code_review':'Reviewed r2 worker/driver/transfer/HUD/subject/profile diffs and the P36 exact-Git-blob fixture. No success mocks or check weakening. Original timers/primary branches and old profile retained.'},
    'findings':{'blocking':[], 'non_blocking':[
        'Pixel comparator explanatory rule text still describes bounds-only HUD, while the frozen lower-z request also explicitly records the independently defined y260 guide strip. Actual request/mask and20% cap are verified; wording can be cleaned in successor work without changing qualification.',
        'Native snapshot metadata equality corroborates identity/config/count invariance; it is not a full body digest. Independent pixel reference and existing unchanged Semantic transform coverage remain the relevant detection mechanisms.'
    ]},
    'reuse_acceptance_ref':ref(OUT/'reuse-acceptance.json'),
    'full_ur0b_status':'BLOCKED_EVIDENCE_PENDING_REAL_BROWSER_CORRECTNESS_AND_MAIN_MATRIX',
    'uncompleted':['6 real paired correctness workers','30 main matrix rows'],
    'historical_sampling':{'starts':33,'seconds':1251,'old_windows':'CLOSED_UNCHANGED'},
    'official_P34_or_platform_or_performance_PASS':False,
    'refs':[ref(OUT/n) for n in ['identity-check.json','recorded-reuse-check.json','node.log','python.log','boundary.log']]
}
put('result.json',result)

r4plan=read(ROOT/'.aegis/packages/G5-UR0b-resume-r4/run-plan.json')
planned = r4plan if isinstance(r4plan,list) else r4plan.get('rows',r4plan.get('runs'))
assert isinstance(planned,list)
remaining=json.loads(json.dumps(planned[2:]))
old_window_rows={r['run_id']:r for r in read(ROOT/'.aegis/results/G5-UR0b/resume-r4/window.json')['rows']}
for r in remaining:
    r['window_run_key']='G5-UR0b-resume-r5/'+r['run_id']
    r['predecessor_status']=old_window_rows[r['run_id']]['status']
    r['predecessor_admission_status']=old_window_rows[r['run_id']].get('admission_status','NOT_STARTED')
assert len(remaining)==36 and sum(r.get('mode')=='correctness' for r in remaining)==6
assert sum(r['phase']=='main' for r in remaining)==30
assert sum(r.get('limit_seconds',r.get('worker_limit_seconds',0)) for r in remaining)==5940
put('remaining-run-plan.json',{'schema':'ur0b-current-reverified-remaining36-v1','rows':remaining,
    'origin_ref':ref(ROOT/'.aegis/packages/G5-UR0b-resume-r4/run-plan.json'),
    'change':'Drop only120/240 already independently qualified; retry the prior incomplete lower-z case once under this separately authorized corrected-harness window; other5 correctness and30main are first new attempts.',
    'new_max_workers':36,'nominal_worker_limit_seconds_sum':5940,'historical_starts':33,'historical_seconds':1251,
    'cumulative_planned_upper_bound_seconds':7191,'original_all_workers_cap_seconds':8370})

request={
    'schema':'ur0b-frozen-evidence-reverification-continuation-v1','status':'AUTHORIZED_BOUNDED_REVERIFICATION_CONTINUATION',
    'stage':'P36 failed-evidence reverification and previously frozen pending UR0b baseline execution',
    'authority':'Existing human instruction to continue this baseline and have the implementation conversation execute/report back. Independent control has now closed offline repair and accepted exact recorded prerequisites; this request concretely authorizes the remaining frozen36 worker starts, not an open-ended sampling allowance.',
    'review_result_ref':ref(OUT/'result.json'),'accepted_reuse_ref':ref(OUT/'reuse-acceptance.json'),
    'repository':{'provider':'github','full_name':'Mostorm-Labs/axiom'},
    'worktree':str(ROOT),'execution_ref':'refs/heads/codex/geometry-navigation-integration',
    'task_anchor':{'revision':'f651523a40c9b383b7d8e2115fcd32f75ddc38a7','relation':'ancestor'},
    'resume_cursor':{'revision':identity['repository_revision'],'accepted_source_sha256':identity['source_tree_sha256'],
        'completed_through':'Independently accepted offline r2 harness/P36 test repair and17 exact recorded prerequisites under current compiler.',
        'next_action':'Materialize new P31, implement/test the new-window runner in the two allowed paths, bind its mechanism/artifacts, then run6correctness and conditionally30main until terminal result.'},
    'owner_handoff':'Existing implementation conversation applies aegis-implementation for exact P31/window mechanics; clean preflight is an internal checkpoint and must not cause another approval wait.',
    'package_to_materialize':'.aegis/packages/G5-UR0b-resume-r5/package.json',
    'source_scope':['verification/runners/g5/ur0/reverified_remaining_runner.py','verification/tests/test_g5_ur0_reverified_remaining_runner.py'],
    'new_control_paths':['.aegis/packages/G5-UR0b-resume-r5/**'],
    'read_only':['All prior packages/results/windows/ledgers and all17 accepted raw or derived records',
        'All existing Runtime/SDK/consumer/collector/harness/compiler/profile/reader/threshold/corpus files; only the two new orchestration paths are mutable'],
    'accepted_artifact_provenance_ref':ref(ROOT/'.aegis/results/G5-UR0b/resume-r4/mechanism-proofs.json'),
    'required_preflight_and_orchestration':[
        'Explicitly consume the new control acceptance, not the old r4 packet. Bind each of17 listed recordings to its own original source, input hashes, current derived compiler and exact proof/cardinality/artifact. Old15 and newRT2 have distinct source groups; never collapse them into one invented source or mark them fresh.',
        'Keep current frozen compiler/profile/SDK/corpus identities, native proof and actual OFFLINE-vs-WORKER_COMPILE separation. No SDK rebuild or diagnostic/cohort/RT rerun.',
        'Use current canonical/admission helpers with all ownership, commit, clock, metrics, source/artifact and receipt checks. Main requires all6 fresh correctness prerequisites satisfy inherited exact QUALIFIED or complete matching PRODUCT_RED_IN_BOTH_OFF_ON criteria; preserve any product RED as such.',
        'Bind each fresh worker to current source/mechanism and actual SDK artifacts. Native/cold surfaces and viewport requirements remain unchanged.',
        'A single fixed r5 state root with durable exclusive claim prevents concurrency, restart, relocation or second invocation from obtaining another allowance. Test runner/ledger/admission rules offline first; such tests may not start browsers.',
        'Return guard-primary failure cause before missing-summary diagnostics; preserve every raw reason and honest unknown status. No success synthesis or skipping required rows.'
    ],
    'run_plan_ref':ref(OUT/'remaining-run-plan.json'),
    'historical_ledger_ref':ref(ROOT/'.aegis/results/G5-UR0b/resume-r4/window.json'),
    'sampling_limits':{'new_workers_max':36,'new_nominal_worker_seconds_max':5940,'prior_starts':33,'prior_seconds':1251,
        'original_cumulative_cap_seconds':8370,'cumulative_nominal_seconds':7191,'concurrency':1,'process_family_rss_bytes':3221225472,
        'correctness_phase_ms':30000,'correctness_outer_seconds':90,'main_outer_seconds':180,
        'accounting':'Reserve each full worker limit before spawn. Count all started browser/native worker process families including failures; charge actual terminal ceil(elapsed_s), or full reserved limit for unknown interrupted guard. Never clip overrun; stop and record it. Unit/offline compiler tests follow the original separate accounting.',
        'no_hidden_starts':'No smoke/probe/unplanned qualification/extra replay, no retry or resampling. All worker starts must be named in plan before launch.',
        'stop_policy':'First hard timer/RSS/cleanup/nonzero-exit/admission/identity/ledger failure closes the window; cleanup owned processes, mark remaining rows MISSING, notify control. Unused time is not retry permission.'},
    'state_root':'.aegis/results/G5-UR0b/resume-r5/',
    'required_tests':['New runner offline unit tests for exact17 reuse, source groups, stale compiler/profile/input/artifact/proof rejection, scope and fresh/recorded separation',
        'Offline ledger tests: prior33/1251 retained, reserve-before-spawn, unknown charge, overrun honesty, concurrency/restart/relocation refusal, no hidden starts and6-correctness-before-main gating',
        'Full existing Node/Python/boundary regression and git diff --check before browser starts;2 existing malformed native unit checks allowed/counted, no new qualification oracle/build',
        'New exact mechanism proof and immutable SDK artifact checks before claiming the one r5 window',
        'All36 frozen planned evidence runs, or exact early frozen failure with all remaining items explicitly MISSING'],
    'outputs':['.aegis/results/G5-UR0b/resume-r5/**','out/ur0b/resume-r5/**','verification/evidence/performance/g5-unified-render-ur0b/<new-source-sha>/resume-r5/**'],
    'return_policy':{'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW','stop_after':'UR0b',
        'success':'READY_FOR_CONTROL_REVIEW with36 accounted and exact reused/current evidence identities; never product/performance/P34 PASS',
        'blockers':['AUTHORITY_CONFLICT','TASK_PACKAGE_DEFECT','VERIFICATION_DESIGN_DEFECT','FROZEN_VERIFICATION_FAILURE','BLOCKED_EVIDENCE','ENVIRONMENT_BLOCKER']},
    'notify_terminal':read(ROOT/'.aegis/packages/G5-UR0b-correctness-test-reverify-r1/execution-contract.json')['EXECUTION_CLOSURE_CONTRACT']['notify_terminal'],
    'commit_push':False,'UR0c_or_UR1_authorized':False,
    'gate_boundary':'Review acceptance is only the offline repair and listed historical prerequisites. New real correctness/performance evidence remains pending and must be reviewed independently.'
}
request['notify_terminal']['contents'] += ['all planned/executed/missing run IDs and actual guard/cleanup results',
    'original33 starts1251s plus new starts/actual charged seconds/reservations and whether the window is closed',
    'actual OFF/ON correctness verdicts or product RED, current/fresh vs reused source/proof/artifact identities']
put('continuation-request.json',request)
print(json.dumps({'status':result['status'],'result_ref':ref(OUT/'result.json'),'reuse_acceptance_ref':ref(OUT/'reuse-acceptance.json'),
                  'continuation_request_ref':ref(OUT/'continuation-request.json')}))
