"""P31 source-allowance correction; preserve P20 and all r1 evidence."""
import copy
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OLD = ROOT / '.aegis/packages/G5-UR0b-correctness-repair-r1'
NEW = ROOT / '.aegis/packages/G5-UR0b-correctness-repair-r2'
OUT = Path(__file__).resolve().parent
NEW.mkdir(parents=True, exist_ok=True)

def read(p): return json.loads(Path(p).read_text())
def ref(p):
    p = Path(p)
    return {'path': str(p.relative_to(ROOT)), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'identity_kind': 'local-content-addressed'}
def put(name, data): (NEW / name).write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n')

extra = ['verification/runners/g5/ur0/repair_runner.py', 'verification/tests/test_g5_ur0_repair_runner.py']
old_package = read(OLD / 'package.json')
returned = ROOT / '.aegis/results/G5-UR0b/correctness-repair-r1/result.json'
amendment = {
    'schema': 'aegis-p31-scope-correction-v1', 'status': 'AUTHORIZED', 'owner': 'aegis-implementation / P31 control',
    'classification': 'TASK_PACKAGE_DEFECT', 'original_work_order_ref': old_package['authorization']['work_order_ref'],
    'original_package_ref': ref(OLD / 'package.json'), 'terminal_result_ref': ref(returned),
    'witness_ref': ref(ROOT / '.aegis/results/G5-UR0b/correctness-repair-r1/profile-preflight.json'),
    'why': 'The already-required exact repaired creation profile must also be accepted by the existing Python canonical-chain admission owner. Its literal old-profile equality cannot accept a changed worker tuple. Omitting this consumer and its regression test from source allowance made r1 incomplete.',
    'authority': 'Existing human authorization to control/design here, implement in adjacent conversation, continue to completion and proactively report terminal results; this is the minimal package correction for that same approved work.',
    'added_source_paths': extra,
    'allowed_changes_in_added_paths': 'Only support the existing P20 old exact profile plus the single exact repaired worker/consumer/helper profile, and test their acceptance/rejection through require_canonical_chain. Preserve all identity, generation, input-hash, count, committed/mutation-receipt, closed-window, artifact and timing checks.',
    'profile_rule': 'Keep one coherent explicit allowlist across JS producer and Python reader, bound to the P20-approved exact profiles. Reject unknown/partial/mismatched tuple, changed consumer/helper, foreign/ambiguous owner and forged initialization. Never substitute the old worker hash for a new worker.',
    'precedence': 'This current r2 P31 correction overrides only the old work-order/r1 source_scope omission and task/output locations. The original P20 spec, all12 oracle groups and all other original obligations/non-goals remain exact. No further P20 change or approval is required for this correction.',
    'new_sampling_workers_max': 0, 'prior_sampling_starts': 33, 'prior_sampling_seconds': 1251,
    'old_packages_results_and_windows_immutable': True, 'commit_push': False}
put('scope-amendment.json', amendment)

authority = read(OLD / 'authority.lock.json')
authority['scope_amendment_ref'] = ref(NEW / 'scope-amendment.json')
authority['effective_scope_authority'] = 'Original work order as narrowly corrected by scope-amendment.json; execution-contract.json carries the complete13-file allowance.'
authority['starting_source_sha256'] = read(returned)['source_tree_sha256']
put('authority.lock.json', authority)

verification = read(OLD / 'verification.lock.json')
put('verification.lock.json', verification)

execution = read(OLD / 'execution-contract.json')
c = execution['EXECUTION_CLOSURE_CONTRACT']
c['authorized_source_paths'] += extra
c['required_changes'].append('Complete the already-required creation-profile producer/consumer contract in repair_runner.py and its regression test, accepting only old exact + single repaired exact tuple. This instantiates CREATION-PROFILES/PAIRED-INIT-OWNERS; no new oracle group or product requirement.')
c['authorized_outputs'] = [p.replace('correctness-repair-r1', 'correctness-repair-r2') for p in c['authorized_outputs']]
c['scope_amendment_ref'] = ref(NEW / 'scope-amendment.json')
c['preserve_completed_work'] = 'Reuse the r1 binding/preflight/reproduction as historical supporting evidence; old r1 return stays TASK_PACKAGE_DEFECT. No harness code was implemented, so start the existing RED/implementation steps now rather than repeating checkpoint-only returns.'
put('execution-contract.json', execution)

evidence = read(OLD / 'evidence-contract.json')
for key in ['result_root', 'evidence_root']:
    evidence[key] = evidence[key].replace('correctness-repair-r1', 'correctness-repair-r2')
evidence['scope_amendment_ref'] = ref(NEW / 'scope-amendment.json')
put('evidence-contract.json', evidence)

context = '''# Offline correctness harness repair r2

P31 scope correction for the r1 terminal package defect. HEAD remains df214581;
preserve all r1 preflight/results and old CLOSED sampling windows. P20 and its12
oracle groups remain unchanged. This is an execution authorization, not a request
to prepare another proposal or wait for confirmation.

Runtime still owns editing semantics and Skia rendering. The harness owns cold
capture/transfer, independent pixel comparison and exact creation provenance.
Current proof flow: worker public init/state + raw native ownership -> JS basis
and canonical proof -> repair_runner.require_canonical_chain -> resume/remaining
admission. Repair the producer and the existing Python reader together. The two
newly allowed reader/test paths close the demonstrated omission; other Python
reader behavior and all product code remain outside the change scope.

Run the frozen old HUD RED, implement live subject/HUD, bounded cold batching,
coarse stage diagnosis, exact additive creation profile and coherent admission.
Complete all12 oracles,17 immutable-input recompilations and relevant suites.
Keep real residual OFF0 differences; do not manufacture a green archive.
No browser sampling/build/new qualification oracle probe/commit/push; retain the
two expressly allowed malformed-snapshot native unit checks and count them.
Continue to READY_FOR_CONTROL_REVIEW or a genuine frozen blocker; notify control
thread01a1b6f3-e69a-7fe2-8d9b-0222e8741f03 at the terminal state.
'''
(NEW / 'implementation-context.md').write_text(context)

package = copy.deepcopy(old_package)
package['task_id'] = 'G5-UR0b-correctness-repair-r2'
package['supersedes_for_execution'] = ref(OLD / 'package.json')
package['status'] = 'AUTHORIZED_OFFLINE_IMPLEMENTATION'
package['authorization']['scope_amendment_ref'] = ref(NEW / 'scope-amendment.json')
package['authorization']['effective_source_paths'] = c['authorized_source_paths']
package['resume_cursor']['accepted_source_sha256'] = read(returned)['source_tree_sha256']
package['resume_cursor']['completed_through'] = 'r1 bindings and full proof-path preflight; independently confirmed literal-profile reader rejection. No harness code changed.'
package['resume_cursor']['next_action'] = 'Execute all pending frozen offline repair items with the13-path allowance, including the coherent Python consumer; no further approval for a clean preflight.'
package['resume_cursor']['predecessor_return_ref'] = ref(returned)
package['resume_cursor']['source_delta_rule'] = 'The new seven r2 package files are approved additive control inputs; compute the current full-source identity before code rather than requiring equality with the predecessor c27c source.'
for key, filename in [('authority_lock_ref','authority.lock.json'),('verification_lock_ref','verification.lock.json'),
                      ('execution_contract_ref','execution-contract.json'),('implementation_context_ref','implementation-context.md'),
                      ('evidence_contract_ref','evidence-contract.json')]:
    item = ref(NEW / filename)
    package[key] = {'path': filename, 'sha256': item['sha256']}
put('package.json', package)

assert len(c['authorized_source_paths']) == 13
assert read(NEW / 'verification.lock.json') == read(OLD / 'verification.lock.json')
assert set(c['authorized_source_paths']) - set(read(OLD/'execution-contract.json')['EXECUTION_CLOSURE_CONTRACT']['authorized_source_paths']) == set(extra)
for key in ['required_tests','forbidden_changes','blocking_evidence','all_of','new_sampling_workers_max','prior_sampling_starts','prior_sampling_seconds','notify_terminal']:
    assert c[key] == read(OLD/'execution-contract.json')['EXECUTION_CLOSURE_CONTRACT'][key], key
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
    item = package[key]
    assert hashlib.sha256((NEW/item['path']).read_bytes()).hexdigest() == item['sha256']
report = {'status': 'CORRECTED_P31_PACKAGE_READY', 'classification': 'TASK_PACKAGE_DEFECT_SOURCE_ALLOWANCE_FIXED',
          'package_ref': ref(NEW/'package.json'), 'scope_amendment_ref': ref(NEW/'scope-amendment.json'),
          'added_source_paths': extra, 'effective_source_paths_count': 13, 'p20_and12_oracle_groups_unchanged': True,
          'tests_and_thresholds_unchanged': True, 'new_sampling_workers_max': 0, 'reconfirmation_needed': False,
          'independent_reproduction': 'Actual old RT120 accepted; both profile-only counterfactuals rejected at literal old-profile guard; all29 returned payload files verified.'}
(OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
