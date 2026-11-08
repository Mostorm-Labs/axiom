"""Preserve a terminal pre-implementation scope blocker for independent review."""
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
D = Path(__file__).resolve().parent
P = ROOT / '.aegis/packages/G5-UR0b-correctness-repair-r1'
V = ROOT / '.aegis/verification/G5-UR0b-correctness-harness-v1'
sys.path.insert(0, str(ROOT / 'verification/runners/g5/ur0'))
from source_identity import identity

def load(p):
    return json.loads(p.read_text())

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def ref(p):
    return {'path': str(p.relative_to(ROOT)), 'sha256': sha(p)}

def write(p, value):
    p.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')

package = load(P / 'package.json')
assert package['task_id'] == 'G5-UR0b-correctness-repair-r1'
for key in ['authority_lock_ref', 'verification_lock_ref', 'execution_contract_ref',
            'implementation_context_ref', 'evidence_contract_ref']:
    binding = package[key]
    assert sha(P / binding['path']) == binding['sha256']
pre = load(D / 'profile-preflight.json')
assert pre['status'] == 'REPRODUCED_SCOPE_CONFLICT'
assert pre['baseline']['observed'] == 'ACCEPTED'
assert len(pre['counterfactuals']) == 2
node_log = (D / 'initialization-control-tests.log').read_text()
python_log = (D / 'reader-control-tests.log').read_text()
assert '# pass 13' in node_log and '# fail 0' in node_log
assert 'Ran 11 tests' in python_log and python_log.endswith('OK\n')
write(D / 'validation.json', {
    'status': 'PREIMPLEMENTATION_CHECKS_ONLY',
    'controls': [
        {'command': 'node --test verification/tests/g5_ur0_initialization.test.mjs',
         'exit_code': 0, 'tests': 13, 'failures': 0,
         'log_ref': ref(D / 'initialization-control-tests.log')},
        {'command': 'python3.12 -m unittest discover -s verification/tests -p test_g5_ur0_repair_runner.py',
         'exit_code': 0, 'tests': 11, 'failures': 0,
         'log_ref': ref(D / 'reader-control-tests.log')},
    ],
    'profile_compatibility_ref': ref(D / 'profile-preflight.json'),
    'mandatory_full_suites': 'NOT_RUN_IN_THIS_CONTINUATION_PREIMPLEMENTATION_SCOPE_BLOCKER',
    'mandatory_native_malformed_checks': {
        'existing_checks_in_full_python_suite': 2,
        'this_continuation_executed': 0,
        'checkpoint_commit_validation_before_this_continuation': 2,
        'checkpoint_context': 'The preceding commit/push turn ran 56 Python tests, including these two allowed checks. Not a new repair closure run.',
    },
    'new_native_qualification_probes': 0,
    'new_browser_sampling_workers': 0,
    'new_builds': 0,
})
guard = {
    'path': 'verification/runners/g5/ur0/repair_runner.py',
    'function': 'require_canonical_chain',
    'condition': "origin.get('source_profile')!=profile",
    'accepted_proof_ref': pre['baseline']['input_ref'],
}
source_lines = (ROOT / guard['path']).read_text().splitlines()
guard['line'] = next(i for i, s in enumerate(source_lines, 1) if guard['condition'] in s)
guard['file_sha256'] = sha(ROOT / guard['path'])
write(D / 'design-preflight.json', {
    'status': 'TASK_PACKAGE_DEFECT', 'authority_conflict': False,
    'verification_threshold_conflict': False, 'package_scope_conflict': True,
    'current_owner_path': [
        'worker init/state and recorded source map', 'initialization_basis.mjs',
        'decode_trace.mjs / canonical_chain.mjs',
        'remaining_runner.require_fresh -> resume_runner.check_worker',
        'repair_runner.require_canonical_chain -> exact old source_profile guard',
    ],
    'required_change': 'An exact additional repaired worker profile and full paired-init ownership checks.',
    'conflict': 'The enforcing Python reader only accepts the old exact two-hash profile, '
                'and it is not in the frozen source scope. Changing only allowed JS '
                'producers cannot truthfully make the repaired profile pass this reader.',
    'reader_guard': guard, 'reproduction_ref': ref(D / 'profile-preflight.json'),
    'minimal_scope_reconciliation': pre['required_scope_repair'],
    'required_reader_semantics': 'Accept only the original profile and one exact new tuple; '
        'validate all pinned worker/consumer/helper hashes and full provenance; keep '
        'foreign/unknown/mismatched profiles and ambiguous paired owners rejected. No wildcard.',
    'implementation_started': False,
    'evidence_contract_preflight': 'BLOCKED_SOURCE_SCOPE_DEPENDENCY',
})
old = load(D / 'starting-source-identity.json')
now = identity(ROOT)
changed = sorted(n for n in set(old['files']) | set(now['files'])
                 if old['files'].get(n) != now['files'].get(n))
assert len(changed) == 6 and all(n.startswith(str(P.relative_to(ROOT)) + '/') for n in changed), changed
assert not subprocess.check_output(['git', '-C', str(ROOT), 'diff', '--name-only'])
assert not subprocess.check_output(['git', '-C', str(ROOT), 'diff', '--cached', '--name-only'])
assert now['revision'] == 'df214581c1c80c2d546939271f3db2dc0e77a38b'
write(D / 'full-source-identity.json', now)
write(D / 'preservation.json', {
    'status': 'UNCHANGED_EXISTING_SOURCE_AND_TRACKED_EVIDENCE',
    'starting_source_sha256': old['source_tree_sha256'],
    'current_source_sha256': now['source_tree_sha256'],
    'source_delta': changed,
    'tracked_and_staged_diff_empty': True,
    'explicit_input_bindings_ref': ref(D / 'binding-preflight.json'),
    'original_17_recompilations': 'NOT_RUN_SCOPE_BLOCKER; original compiler/decoder/helper bytes unchanged',
    'historical_sampling_starts': 33, 'historical_sampling_seconds': 1251,
    'old_windows': 'CLOSED_UNCHANGED',
})
write(D / 'ownership-handoff.json', {
    'type': 'ownership_handoff', 'from_owner': 'aegis-implementation', 'to': 'aegis',
    'reason': 'earlier_untrusted_layer', 'requested_stage': 'P31',
    'earliest_untrusted_layer': 'P31 source-scope contract frozen by the P20 work order',
    'status': 'TASK_PACKAGE_DEFECT', 'suggested_next_stage': 'P31 scope reconciliation',
    'evidence_ref': ref(D / 'design-preflight.json'),
    'requested_action': 'Reissue the exact work order/package scope to include the Python '
                        'profile reader and its regression tests, without changing the '
                        'accepted 12-oracle/17-recording closure, thresholds or sampling limit.',
})
write(D / 'oracle-coverage.json', {
    'status': 'NOT_EXECUTED_IMPLEMENTATION_BLOCKED_AT_PREFLIGHT',
    'cases': [{'id': c['id'], 'status': 'NOT_RUN_PREIMPLEMENTATION_BLOCKER'}
              for c in load(V / 'oracle-cases.json')['cases']],
    'independent_preflight_witness_ref': ref(D / 'profile-preflight.json'),
    'no_claim_of_12_group_or_17_recording_closure': True,
})
(D / 'README.md').write_text('''# G5-UR0b correctness repair r1 — TASK_PACKAGE_DEFECT

P31 has been frozen from the exact P20 work order at the reconciled df214581
cursor. Repository/binding preflight succeeds. Full proof-path design preflight
finds a scope omission before source edits: the frozen work order requires an
additional exact repaired worker profile, while Python require_canonical_chain
compares source_profile to the old two-hash dictionary and is outside scope.

profile-preflight.py accepts the real archived RT120 proof, then shows that
profile-only changes are rejected at that exact guard. The two negative inputs
are explicitly counterfactual unit inputs, not new sampled profiles or recordings.
No worker was executed and no archive was edited. See design-preflight.json for
the call path, reader line/hash and minimum scope correction.

The existing 13 initialization Node tests and 11 Python reader tests pass as
preflight controls. The 12 repair oracle groups, 17 recompilations and full suites
were not run in this blocked implementation attempt. The preceding checkpoint
validation had 2 allowed malformed-snapshot native checks; this continuation has
0 native calls, 0 browser workers, 0 builds and no commit/push. Historical sampling
stays 33 starts / 1251 seconds. All old windows and verdicts remain unchanged.

Return to control for exact source-scope reconciliation. This is neither completed
harness repair nor readiness for new sampling, and contains no Gate/P34 PASS.
''')
bundle = (ROOT / 'verification/evidence/performance/g5-unified-render-ur0b'
          / now['source_tree_sha256'] / 'correctness-repair-r1')
bundle.mkdir(parents=True, exist_ok=False)
shutil.copytree(P, bundle / 'package')
shutil.copytree(V, bundle / 'frozen-p20')
shutil.copytree(ROOT / '.aegis/results/G5-UR0b/control-resume-after-df214581', bundle / 'cursor-reconciliation')
payload = bundle / 'preflight'
payload.mkdir()
for f in sorted(D.iterdir()):
    if f.is_file():
        shutil.copyfile(f, payload / f.name)
files = {str(f.relative_to(bundle)): {'sha256': sha(f), 'bytes': f.stat().st_size}
         for f in sorted(bundle.rglob('*')) if f.is_file()}
write(bundle / 'manifest.json', {
    'schema': 'ur0b-correctness-preflight-bundle-v1',
    'source_tree_sha256': now['source_tree_sha256'], 'files': files,
    'scope': 'Package, frozen P20, cursor reconciliation and offline preflight payload.',
    'excluded_envelopes': ['manifest.json', 'result.json'],
    'envelope_rule': 'result.json binds this manifest. Both hashes are provided by the terminal notice; no circular self-binding.',
})
result = {
    'schema_version': 1, 'task_id': 'G5-UR0b-correctness-repair-r1',
    'stage': 'P31_P32_DESIGN_PREFLIGHT', 'status': 'TASK_PACKAGE_DEFECT',
    'continue_execution': False, 'return_surface': 'CONTROL_REVIEW',
    'result_revision': now['revision'], 'dirty': True,
    'source_tree_sha256': now['source_tree_sha256'],
    'package_ref': ref(P / 'package.json'),
    'work_order_ref': ref(V / 'implementation-request.json'),
    'verification_spec_ref': ref(V / 'verification-spec.json'),
    'position_ref': ref(ROOT / '.aegis/results/G5-UR0b/control-resume-after-df214581/position.json'),
    'binding_preflight_ref': ref(D / 'binding-preflight.json'),
    'design_preflight_ref': ref(D / 'design-preflight.json'),
    'validation_ref': ref(D / 'validation.json'),
    'preservation_ref': ref(D / 'preservation.json'),
    'oracle_coverage_ref': ref(D / 'oracle-coverage.json'),
    'ownership_handoff_ref': ref(D / 'ownership-handoff.json'),
    'materialized_ref': {'kind': 'SHARED_WORKTREE_CONTENT_ADDRESSED_PAYLOAD',
                         'path': str(bundle), 'manifest_sha256': sha(bundle / 'manifest.json')},
    'implementation_source_changes': [],
    'remaining': load(V / 'implementation-request.json')['required_changes'],
    'future_sampling': 'Six correctness workers then 30 main remain unexecuted and unauthorized by this package.',
    'performance_or_official_gate_PASS': False,
    'notification_policy': load(V / 'implementation-request.json')['notify_terminal'],
}
write(D / 'result.json', result)
shutil.copyfile(D / 'result.json', bundle / 'result.json')
for name, value in files.items():
    assert sha(bundle / name) == value['sha256']
assert sha(D / 'result.json') == sha(bundle / 'result.json')
assert identity(ROOT)['source_tree_sha256'] == now['source_tree_sha256']
print(json.dumps({'result_ref': ref(D / 'result.json'), 'source_tree_sha256': now['source_tree_sha256'],
                  'package_ref': ref(P / 'package.json'), 'bundle': result['materialized_ref']}))
