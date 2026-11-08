"""Freeze the existing work order without extending its source allowance."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
RESULT = Path(__file__).resolve().parent
DESIGN = ROOT / '.aegis/verification/G5-UR0b-correctness-harness-v1'
PACKAGE = ROOT / '.aegis/packages/G5-UR0b-correctness-repair-r1'
sys.path.insert(0, str(ROOT / 'verification/runners/g5/ur0'))
from source_identity import identity

def load(p):
    return json.loads(Path(p).read_text())

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def write(p, value):
    Path(p).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')

def ref(p):
    return {'path': str(p.relative_to(ROOT)), 'sha256': sha(p),
            'identity_kind': 'local-content-addressed'}

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args], text=True).strip()

assert git('remote', 'get-url', 'origin') == 'git@github.com:Mostorm-Labs/axiom.git'
assert git('branch', '--show-current') == 'codex/geometry-navigation-integration'
assert git('rev-parse', 'HEAD') == 'df214581c1c80c2d546939271f3db2dc0e77a38b'
position = ROOT / '.aegis/results/G5-UR0b/control-resume-after-df214581/position.json'
assert sha(position) == 'c6c9e140908d63632784b7be739cf690f870463ce99e2a6cbc4152d8a1871cd5'
subprocess.run(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor',
                'f651523a40c9b383b7d8e2115fcd32f75ddc38a7', 'HEAD'], check=True)
expected = {
    'implementation-request.json': '30f005f237f654959592ec899df5ca9a0ca6e43076b48d3b78c8c19b14972452',
    'verification-spec.json': '0d6169f832ea8eeabcafcad24e9f212e9272b13591658a8cc6247051fcc44b65',
    'oracle-cases.json': '96d392b4dca7e204d4901b5646e89812d27ac96ff7e14b97e6a2de46a3131712',
    'evidence-inputs.json': '77b0c611ed9a1d0134b7db04b0c11bd6ffebda329653312727770f0f8a1ae268',
}
for name, digest in expected.items():
    assert sha(DESIGN / name) == digest, name
order = load(DESIGN / 'implementation-request.json')
spec = load(DESIGN / 'verification-spec.json')
oracles = load(DESIGN / 'oracle-cases.json')['cases']
checked = {}

def check_refs(value):
    if isinstance(value, dict):
        if isinstance(value.get('path'), str) and 'sha256' in value:
            p = (ROOT / value['path']).resolve()
            assert p.is_relative_to(ROOT) and p.is_file(), value['path']
            assert sha(p) == value['sha256'], value['path']
            checked[value['path']] = value['sha256']
        for v in value.values():
            check_refs(v)
    elif isinstance(value, list):
        for v in value:
            check_refs(v)

for name in expected:
    check_refs(load(DESIGN / name))
check_refs(load(ROOT / '.aegis/verification/G5-UR0b-initialization-v1/evidence-inputs.json'))
starting = identity(ROOT)
assert starting['source_tree_sha256'] == load(position)['current_source_sha256']
write(RESULT / 'starting-source-identity.json', starting)
PACKAGE.mkdir(exist_ok=False)
write(PACKAGE / 'authority.lock.json', {
    'schema_version': 1, 'status': 'CURRENT',
    'work_order_ref': ref(DESIGN / 'implementation-request.json'),
    'activation_ref': ref(DESIGN / 'activation.json'),
    'position_ref': ref(position),
    'starting_source_sha256': starting['source_tree_sha256'],
    'trusted_statements': order['required_changes'],
    'read_only': order['read_only'],
    'prior_windows_reopened': False,
})
write(PACKAGE / 'verification.lock.json', {
    'schema_version': 1, 'status': 'CURRENT',
    'verification_spec_ref': ref(DESIGN / 'verification-spec.json'),
    'acceptance_oracle_ref': ref(DESIGN / 'oracle-cases.json'),
    'evidence_inputs_ref': ref(DESIGN / 'evidence-inputs.json'),
    'obligations': [{
        'id': item['id'], 'requirement': item['id'],
        'oracle': item['oracle'],
        'acceptance': 'Satisfy the exact frozen oracle without changing thresholds, subjects, or historical verdicts.',
        'evidence_required': ['Named offline execution record', 'Exact immutable input references'],
    } for item in oracles],
    'live_reference_contract': spec['live_reference_contract'],
    'cold_transport_and_stage_contract': spec['cold_transport_and_stage_contract'],
    'initialization_profile_compatibility': spec['initialization_profile_compatibility'],
    'quality_evidence_gate': spec['quality_evidence_gate'],
})
write(PACKAGE / 'execution-contract.json', {
    'schema_version': 1,
    'EXECUTION_CLOSURE_CONTRACT': {
        **order['closure'], 'authorized_source_paths': order['source_scope'],
        'read_only': order['read_only'], 'required_changes': order['required_changes'],
        'required_tests': order['required_tests'], 'forbidden_changes': order['forbidden'],
        'authorized_outputs': order['output_paths'],
        'hosted_verification': {'required': [], 'optional': []},
        'blocking_evidence': spec['quality_evidence_gate']['required'],
        'corroborative_evidence': spec['evidence_qualification']['corroborative'],
        'new_sampling_workers_max': 0, 'commit_push': False,
        'prior_sampling_starts': 33, 'prior_sampling_seconds': 1251,
        'notify_terminal': order['notify_terminal'],
    },
})
(PACKAGE / 'implementation-context.md').write_text('''# Offline correctness harness repair

Start at reconciled df214581; the f651523a cursor is its ancestor. Preserve the
accepted initialization repair, all recorded inputs and all CLOSED windows.

Runtime owns selection, snapping, geometry and semantic commits. The root SDK
facade exposes public state and opaque snapshots. The correctness driver obtains
public fixture state and pixels; the unchanged native oracle supplies independent
CPU reference/AA. Cold transfer and initialization provenance belong to the
verification harness, not product APIs or renderer semantics.

Current proof flow: worker init/state + raw native ownership -> initialization
basis -> decoder/canonical proof -> Python repair/resume admission. Keep that full
flow in the design preflight when introducing a second exact creation profile.
The Python reader is not in this work order's source allowance; do not bypass or
rewrite it to force admission. A real dependency/scope conflict returns the exact
blocker to control with evidence, rather than silently expanding the package.

Otherwise run the real old HUD RED, implement the fixed live subject/HUD recipe,
cold bounded batching, stages and exact additive creation profile; execute all
12 oracles and 17 invariant recompilations. Do not make OFF0 green by assumption.
No browser runs, builds, extra native probes, commit/push or Gate/P34 PASS. The
two existing malformed-snapshot native checks are allowed only as specified.
''')
write(PACKAGE / 'evidence-contract.json', {
    'schema_version': 1,
    'work_order_ref': ref(DESIGN / 'implementation-request.json'),
    'materialization': 'Shared controlled worktree, content-addressed payload and exact result/manifest references; no commit/push.',
    'result_root': '.aegis/results/G5-UR0b/correctness-repair-r1/',
    'evidence_root': 'verification/evidence/performance/g5-unified-render-ur0b/<new-source-sha>/correctness-repair-r1/',
    'required': order['closure']['all_of'],
    'identity_policy': 'Separate historical recording identity and verdicts from current compiler/package provenance. No offline-to-fresh relabeling.',
    'terminal_blocker_policy': 'Return exact failed preflight and reproduction. Mark unexecuted implementation/tests explicitly; never claim complete repair.',
    'notification': order['notify_terminal'],
})
package = {
    'schema_version': 1, 'task_id': 'G5-UR0b-correctness-repair-r1',
    'status': 'AUTHORIZED_OFFLINE_IMPLEMENTATION',
    'execution_authority_mode': 'repo_materialized',
    'repository': order['repository'], 'worktree': order['worktree'],
    'execution_ref': order['execution_ref'],
    'task_anchor': {'revision': order['resume_cursor']['revision'], 'relation': 'ancestor'},
    'resume_cursor': {
        'execution_ref': order['execution_ref'], 'revision': git('rev-parse', 'HEAD'),
        'accepted_source_sha256': starting['source_tree_sha256'], 'position_ref': ref(position),
        'completed_through': 'Initialization repair and reviewed r4 terminal, then checkpoint commit/push.',
        'next_action': 'Binding and full proof-path design preflight; then exact offline repair or explicit blocker.',
    },
    'authorization': {'work_order_ref': ref(DESIGN / 'implementation-request.json'),
                      'new_sampling_workers_max': 0, 'commit_push': False},
    'continue_until_terminal_state': True, 'return_surface': 'CONTROL_REVIEW',
    'stop_after': order['closure']['stop_after'],
}
for key, name in [('authority_lock_ref', 'authority.lock.json'),
                  ('verification_lock_ref', 'verification.lock.json'),
                  ('execution_contract_ref', 'execution-contract.json'),
                  ('implementation_context_ref', 'implementation-context.md'),
                  ('evidence_contract_ref', 'evidence-contract.json')]:
    package[key] = {'path': name, 'sha256': sha(PACKAGE / name)}
write(PACKAGE / 'package.json', package)
for key, value in package.items():
    if key.endswith('_ref') and isinstance(value, dict):
        assert (PACKAGE / value['path']).resolve().parent == PACKAGE
        assert sha(PACKAGE / value['path']) == value['sha256']
write(RESULT / 'binding-preflight.json', {
    'status': 'PASS', 'scope': 'REPOSITORY_AND_EXACT_BINDINGS_ONLY',
    'package_ref': ref(PACKAGE / 'package.json'), 'position_ref': ref(position),
    'source_before_sha256': starting['source_tree_sha256'],
    'verified_refs': checked, 'implementation_design_preflight': 'PENDING',
})
print(json.dumps({'package_ref': ref(PACKAGE / 'package.json'),
                  'binding_preflight': ref(RESULT / 'binding-preflight.json')}))
