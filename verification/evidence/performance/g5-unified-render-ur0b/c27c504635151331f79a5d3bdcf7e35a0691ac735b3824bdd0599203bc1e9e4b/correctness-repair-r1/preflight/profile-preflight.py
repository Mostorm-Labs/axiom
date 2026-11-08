"""Offline reader compatibility witness, not a new sampled worker/profile.

The positive control is a genuine immutable r4 RT120 canonical proof. Negative
inputs change only its creation-profile field in memory; none is evidence of a
worker run. The changed-worker digest has an exact, documented preimage solely
to demonstrate the reader's equality guard. It is NOT an authorized repaired
profile, and is never installed or used to qualify a recording.
"""
import copy
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
RESULT = Path(__file__).resolve().parent
HERE = ROOT / 'verification/runners/g5/ur0'
sys.path.insert(0, str(HERE))
import repair_runner as repair

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def ref(p):
    return {'path': str(p.relative_to(ROOT)), 'sha256': sha(p)}

archive = (ROOT / 'verification/evidence/performance/g5-unified-render-ur0b/'
           'd33166e423e7558bf4d18b87108425b2cb2553ab6ff15271fd4752a0c200d73a/'
           'resume-r4/execution-and-checks/execution/raw-runs/realtime-120/'
           'compiled-summary.json')
assert sha(archive) == '3ae3e7458dfda5a3900f4a258e83268d0e7f6ac2ec6310c52a9b68b52f243987'
before_sha = sha(archive)
recorded = json.loads(archive.read_text())
assert len(recorded['canonical_chain']['initialization_chains']) == 1
repair.require_canonical_chain(recorded, require_positive=True)
consumer = 'verification/runners/g5/ur0/consumer.mjs'
worker = 'verification/runners/g5/ur0/worker.mjs'
old = recorded['canonical_chain']['initialization_chains'][0]['initialization_basis']['source_profile']
assert old[consumer] == sha(ROOT / consumer)
assert old[worker] == sha(ROOT / worker)
suffix = b'\n// Nonexecuted hash-inequality witness for reader preflight only.\n'
new_hash = hashlib.sha256((ROOT / worker).read_bytes() + suffix).hexdigest()
assert new_hash != old[worker]
cases = []
for name, profile in [
    ('distinct_worker_same_consumer', {**old, worker: new_hash}),
    ('additive_exact_helper_tuple', {
        **old, **{str((HERE / p).relative_to(ROOT)): sha(HERE / p)
                  for p in ['correctness_driver.mjs', 'pixel_oracle.mjs', 'stream_transfer.mjs']}
    }),
]:
    candidate = copy.deepcopy(recorded)
    candidate['canonical_chain']['initialization_chains'][0]['initialization_basis']['source_profile'] = profile
    try:
        repair.require_canonical_chain(candidate, require_positive=True)
    except RuntimeError as error:
        message = str(error)
    else:
        raise AssertionError('Expected strict reader rejection was not reproduced')
    assert message == 'canonical initialization recording/source/input basis missing', message
    cases.append({'name': name, 'input_kind': 'COUNTERFACTUAL_PROFILE_ONLY_NOT_SAMPLED',
                  'observed': 'REJECTED', 'exception': message, 'source_profile': profile})
assert sha(archive) == before_sha
order = json.loads((ROOT / '.aegis/verification/G5-UR0b-correctness-harness-v1/implementation-request.json').read_text())
assert str((HERE / 'repair_runner.py').relative_to(ROOT)) not in order['source_scope']
result = {
    'schema': 'ur0b-correctness-profile-preflight-v1',
    'status': 'REPRODUCED_SCOPE_CONFLICT',
    'baseline': {'input_ref': ref(archive), 'observed': 'ACCEPTED',
                 'qualified_initialization_count': 1, 'qualified_committed_count': 1},
    'counterfactuals': cases,
    'worker_hash_witness': {'original_file_ref': ref(ROOT / worker),
                            'suffix_utf8': suffix.decode(), 'derived_sha256': new_hash,
                            'executed': False, 'is_repaired_creation_profile': False},
    'reader_ref': ref(HERE / 'repair_runner.py'),
    'call_path_refs': [ref(HERE / 'remaining_runner.py'), ref(HERE / 'resume_runner.py')],
    'conclusion': 'The exact old two-hash equality rejects every changed worker hash. '
                  'A truthful new repaired creation-profile proof cannot pass the existing '
                  'future correctness admission path. The enforcing reader is outside the '
                  'frozen source allowance; no product behavior or old recording was changed.',
    'required_scope_repair': ['verification/runners/g5/ur0/repair_runner.py',
                              'verification/tests/test_g5_ur0_repair_runner.py'],
    'not_implemented': 'No helper/driver/worker/profile repair performed before scope reconciliation.',
    'new_sampling_workers': 0, 'native_oracle_invocations': 0, 'builds': 0,
}
(RESULT / 'profile-preflight.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'status': result['status'], 'positive_actual_archive': 'ACCEPTED',
                  'profile_only_negative_inputs_rejected': len(cases),
                  'report_ref': ref(RESULT / 'profile-preflight.json')}))
