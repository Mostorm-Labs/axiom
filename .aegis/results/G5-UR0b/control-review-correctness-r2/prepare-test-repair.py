"""P35 classification and narrow P36 regression-fixture repair dispatch."""
import copy
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent
OLD = ROOT / '.aegis/packages/G5-UR0b-correctness-repair-r2'
NEW = ROOT / '.aegis/packages/G5-UR0b-correctness-test-reverify-r1'
NEW.mkdir(parents=True, exist_ok=True)
read = lambda p: json.loads(Path(p).read_text())
def ref(p):
    p = Path(p)
    return {'path': str(p.relative_to(ROOT)), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'identity_kind': 'local-content-addressed'}
def put(name, value): (NEW / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')

subject = ROOT / '.aegis/results/G5-UR0b/correctness-repair-r2/result.json'
classification = {
    'schema': 'aegis-p35-test-defect-v1', 'status': 'IMPLEMENTATION_OWNED_TEST_FIXTURE_REPAIR_AUTHORIZED',
    'primary_owner': 'aegis-gate-review', 'stage': 'P35/P36', 'subject_ref': ref(subject),
    'failure_ref': ref(ROOT/'.aegis/results/G5-UR0b/correctness-repair-r2/frozen-failure.json'),
    'classification': 'IMPLEMENTATION_DEFECT_TEST_FIXTURE_CONTEXT',
    'frozen_requirement': 'Exact compiler-bound accepted reuse must retain old recording identities and reject compiler mismatch; all mandatory regression tests must pass.',
    'actual_cause': 'The unchanged positive integration test evaluates an explicitly frozen historical reuse packet against ambient current compiler files, although initialization_basis.mjs legitimately changed under the authorized repair. The current-compiler mismatch is expected fail-closed behavior, not an admission regression.',
    'independent_reproduction': 'Focused original positive test rerun: ERROR at remaining_runner.py accepted_reuse, current compiler differs from accepted reuse compiler. Subject refs and all188 archived payload files verified.',
    'owning_layer_reason': 'The acceptance rule and independent old recording proof remain explicit and trusted. Correct the test environment/applicability to that rule; no new requirement/oracle/threshold or reader policy is introduced.',
    'allowed_source_paths': ['verification/tests/test_g5_ur0_remaining_runner.py'],
    'preserve': 'All completed r2 harness code,12-group offline evidence,17 invariant recompilations, original r2 failure and all old packages/windows.',
    'official_gate_pass': False}
put('repair-authorization.json', classification)

authority = {'schema_version': 1, 'status': 'CURRENT_FOR_NARROW_P36',
             'predecessor_package_ref': ref(OLD/'package.json'), 'repair_authorization_ref': ref(NEW/'repair-authorization.json'),
             'verification_spec_ref': read(OLD/'verification.lock.json')['verification_spec_ref'],
             'rule': 'Existing human design/control and implementation delegation authorizes this implementation-owned test repair. This current P36 scope authorizes only the single named regression test beyond prior r2 allowance; existing harness and readers are now read-only.'}
put('authority.lock.json', authority)
put('verification.lock.json', {'schema_version': 1, 'status': 'CURRENT',
    'inherited_verification_ref': ref(OLD/'verification.lock.json'),
    'inherited_oracles_unchanged': True,
    'obligations': [{'requirement': 'Historical reuse success is conditional on the pinned compiler; current mismatch rejects.',
                     'oracle': 'Exercise accepted_reuse in explicitly isolated historical matching compiler context and in actual mismatching current context.',
                     'acceptance': 'Historical positive retains15 OFFLINE records/original source/counts/artifacts/receipt checks; current mismatching compiler raises the existing exact rejection. No skip, expectedFailure or global success mock.',
                     'evidence_required': ['Focused old test RED', 'Repaired positive/negative tests', 'Full Python regression', 'Exact unchanged compiler/harness/r2 evidence refs']} ]})

old_c = read(OLD/'execution-contract.json')['EXECUTION_CLOSURE_CONTRACT']
c = {'authorized_source_paths': classification['allowed_source_paths'],
     'read_only': ['All Runtime/SDK/collector/consumer/render/brush/semantic/build/dependency sources',
                   'All UR0 harness, compiler, Python readers and their exact profiles, including remaining_runner.py',
                   'All old P20/packages/results/evidence/windows including r2 terminal records'],
     'required_changes': [
         'Repair only the stale positive reuse test fixture/context. Its positive case must explicitly model the accepted historical compiler rather than require ambient current files to have old hashes.',
         'Add an explicit actual-current-compiler negative for the unchanged r4 packet, expecting current compiler differs from accepted reuse compiler. Keep old recording mode/source/receipt/artifact checks.',
         'For matching historical context, prefer exact archived/pinned compiler bytes in a temporary fixture (the accepted df214581 git blob is available). A narrowly controlled unit condition is permitted, but never mock accepted_reuse, admit_record, canonical validation or the entire success result, never change the packet, and restore context at test exit.',
         'Retain all existing negatives and closure/guard tests; do not skip, xfail, mark expectedFailure or delete coverage.',
         'Hash-check unchanged compiler/harness files and r2 evidence; reference the existing12-group/17-input evidence without rerunning unchanged compilation. Independent reuse acceptance is a control responsibility, not a producer-issued PASS.'],
     'required_tests': ['Focused original test RED already independently reproduced; preserve the error and test hash',
                        'Focused repaired historical positive plus current-compiler negative and remaining_runner test module',
                        *[t for t in old_c['required_tests'] if not t.startswith('12 frozen')]],
     'forbidden_changes': [*old_c['forbidden_changes'], 'Any reader/compiler/profile/harness mutation or loosening compiler matching',
                           'Overwriting prior r2 results with new green status or issuing compiler-reuse acceptance on behalf of control'],
     'authorized_outputs': ['.aegis/results/G5-UR0b/correctness-test-reverify-r1/**', 'out/ur0b/correctness-test-reverify-r1/**',
                            'verification/evidence/performance/g5-unified-render-ur0b/<new-source-sha>/correctness-test-reverify-r1/**'],
     'preserved_source_baseline_sha256': read(subject)['source_tree_sha256'],
     'new_package_control_delta': 'Seven files in this package are authorized additive control inputs; compute new source identity rather than requiring b98 equality.',
     'new_sampling_workers_max': 0, 'prior_sampling_starts': 33, 'prior_sampling_seconds': 1251,
     'commit_push': False, 'hosted_verification': {'required': [], 'optional': []},
     'terminal_success': {'all_of': ['Only the one test changed; harness/compiler/r2 records unchanged',
                                     'Historical positive and current mismatch negative pass with all relevant/full mandatory suites',
                                     'Exact result/source/bundle and preserved r2 evidence refs; no false reuse or performance claim',
                                     'Return READY_FOR_CONTROL_REVIEW and proactively notify control']},
     'terminal_blockers': old_c['terminal_blockers'], 'continue_until_terminal_state': True,
     'return_surface': 'CONTROL_REVIEW', 'stop_after': 'TEST_FIXTURE_REVERIFICATION', 'notify_terminal': old_c['notify_terminal']}
put('execution-contract.json', {'schema_version': 1, 'EXECUTION_CLOSURE_CONTRACT': c})
put('evidence-contract.json', {'schema_version': 1, 'materialization': 'Shared controlled worktree, exact result/content-addressed bundle with SHA256; no commit/push.',
    'result_root': '.aegis/results/G5-UR0b/correctness-test-reverify-r1/', 'required': c['terminal_success']['all_of'],
    'preserved_r2_result_ref': ref(subject), 'preserved_reverification_ref': ref(ROOT/'.aegis/results/G5-UR0b/correctness-repair-r2/reverification.json'),
    'notification': c['notify_terminal']})
(NEW/'implementation-context.md').write_text('''# Narrow regression-fixture repair

The r2 harness is implemented; preserve it. One legacy positive test accidentally
uses ambient current compiler files with a frozen old-compiler reuse packet.
remaining_runner rejects this correctly. Fix only the test context and explicitly
retain actual-current compiler rejection. Do not change any reader or old packet.

No new product or Verification design is needed. Temporary archived compiler
fixtures are allowed; broad success mocks, skips and weakened checks are not.
Run focused and mandatory regressions, bind unchanged r2 compiler/evidence, then
return READY_FOR_CONTROL_REVIEW with proactive notification. Do not redo the
harness implementation or unchanged17 recompilations. No sampling/build/push.
''')
package = copy.deepcopy(read(OLD/'package.json'))
package['task_id'] = 'G5-UR0b-correctness-test-reverify-r1'
package['stage'] = 'P36_CODE_REVERIFY'
package['status'] = 'AUTHORIZED_TEST_FIXTURE_REPAIR'
package['supersedes_for_execution'] = ref(OLD/'package.json')
package['authorization'] = {'repair_authorization_ref': ref(NEW/'repair-authorization.json'), 'new_sampling_workers_max': 0, 'commit_push': False}
package['resume_cursor']['accepted_source_sha256'] = read(subject)['source_tree_sha256']
package['resume_cursor']['predecessor_return_ref'] = ref(subject)
package['resume_cursor']['completed_through'] = 'Implemented r2 harness, all12 offline oracle groups and17 invariant recompilations; full Python one stale positive fixture error.'
package['resume_cursor']['next_action'] = 'Repair only the legacy reuse test context, preserve real current mismatch rejection, run mandatory regression and return.'
package['resume_cursor']['source_delta_rule'] = c['new_package_control_delta']
package['stop_after'] = c['stop_after']
for k,n in [('authority_lock_ref','authority.lock.json'),('verification_lock_ref','verification.lock.json'),('execution_contract_ref','execution-contract.json'),('implementation_context_ref','implementation-context.md'),('evidence_contract_ref','evidence-contract.json')]:
    package[k] = {'path': n, 'sha256': ref(NEW/n)['sha256']}
put('package.json', package)
assert c['authorized_source_paths'] == ['verification/tests/test_g5_ur0_remaining_runner.py']
assert c['new_sampling_workers_max'] == 0
report = {'status': 'P36_TEST_REPAIR_READY', 'package_ref': ref(NEW/'package.json'), 'classification_ref': ref(NEW/'repair-authorization.json'),
          'new_mutable_source_count': 1, 'preserved_r2_source_sha256': c['preserved_source_baseline_sha256'], 'new_sampling_workers_authorized': 0}
(OUT/'test-repair-dispatch-input.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
