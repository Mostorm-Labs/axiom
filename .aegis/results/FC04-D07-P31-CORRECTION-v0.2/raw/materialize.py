import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path.cwd()
PKG = ROOT / '.aegis/packages/FC04-D07-P31-CORRECTION-v0.2'
RESULT = ROOT / '.aegis/results/FC04-D07-P31-CORRECTION-v0.2'
ZIP = Path('C:/Users/Chado/Downloads/axiom-fc04-d07-p31-correction.zip')
GIT = 'D:/Softwares/Git/cmd/git.exe'
BASE = 'a85a8077a58fce97c617badc7116794f6bc1a15b'

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

def run(argv, stem):
    p = subprocess.run(argv, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (RESULT / 'raw' / (stem + '.log')).write_bytes(p.stdout)
    value = {'argv': argv, 'cwd': str(ROOT), 'exit_code': p.returncode}
    write(RESULT / 'raw' / (stem + '-command.json'), value)
    return p, value

assert not PKG.exists() and not RESULT.exists(), 'Never overwrite prior package/results'
assert subprocess.check_output([GIT, 'rev-parse', 'HEAD'], text=True).strip() == BASE
assert subprocess.check_output([GIT, 'branch', '--show-current'], text=True).strip() == 'codex/fc04-d07-selection-audit'
assert subprocess.check_output([GIT, 'status', '--porcelain'], text=True).strip() == ''
assert sha(ZIP) == 'cf6b7928c25604400318de8c9e42506944adcdd5223d07894e0d0ad6b92125aa'
assert sha(ROOT / 'out/fc04-d07-repair-preflight/review-bundle.zip') == '53211a9792fb778ac5f90fb20717a16b1663259daddafcbad58afea6d186e215'
PKG.mkdir(parents=True)
(RESULT / 'raw').mkdir(parents=True)

# Only the three named input documents may be restored. Their bytes are immutable.
inputs = []
with zipfile.ZipFile(ZIP) as z:
    expected = {'01-test-binding-correction.md', '02-codex-p31-correction-prompt.md', 'README.md'}
    assert {Path(e.filename).name for e in z.infolist()} == expected
    for e in z.infolist():
        target = PKG / Path(e.filename).name
        target.write_bytes(z.read(e))
        inputs.append({'path': target.relative_to(PKG).as_posix(), 'sha256': sha(target), 'bytes': target.stat().st_size})
write(PKG / 'input-identity.json', {'archive_path': str(ZIP), 'archive_sha256': sha(ZIP), 'files': inputs})

historical = []
for name in ['FC04-D07-AUDIT', 'FC04-D07-REPAIR-PREFLIGHT']:
    folder = ROOT / '.aegis/results' / name
    manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
    for f in manifest['files']:
        path = folder / f['path']
        assert sha(path) == f['sha256'], f'Historical evidence changed: {path}'
        historical.append({'path': path.relative_to(ROOT).as_posix(), 'sha256': sha(path)})
write(RESULT / 'historical-preservation.json', {'verified_files': historical})

for ref in ['c7f874395d55952e9d5288e2a336a21747f37f11', 'e02fb7b8601bd01be968dbfb6aa783cfb3027e25']:
    p, _ = run([GIT, 'merge-base', '--is-ancestor', ref, BASE], 'ancestry-' + ref[:8])
    assert p.returncode == 0
run([GIT, 'remote', '-v'], 'repository')
run([GIT, 'worktree', 'list', '--porcelain'], 'worktrees')
run(['rg', '-n', 'IntentPlan|LocalIntentPlan|UndoIntent|HistoryGroup|IntentCommitSegment', 'runtime', 'apps', 'debug_ui'], 'explicit-entry-search')
run(['rg', '-n', 'HistorySubmitPort|executeUndoable|submitAdmitted|publishAdmitted', 'runtime/interaction', 'runtime/composition'], 'owner-search')

anchors = [
    ('runtime/interaction/include/canvas/interaction/editor_history.hpp', 29, 104),
    ('runtime/interaction/src/editor_history.cpp', 194, 237),
    ('runtime/interaction/src/canvas_editing_runtime.cpp', 3347, 3403),
    ('runtime/interaction/src/canvas_editing_runtime.cpp', 3439, 3476),
    ('runtime/interaction/src/canvas_section_commands.cpp', 449, 461),
    ('runtime/foundation/include/canvas/runtime/runtime_facade.hpp', 1, 180),
    ('runtime/composition/include/canvas/runtime/canvas_runtime_facade.hpp', 1, 156),
]
write(RESULT / 'source-bindings.json', {'subject_revision': BASE, 'anchors': [
    {'path': path, 'sha256': sha(ROOT / path), 'start': start, 'end': end,
     'source': '\n'.join((ROOT / path).read_text(encoding='utf-8').splitlines()[start-1:end])}
    for path, start, end in anchors
]})

source = ROOT / 'verification/fc04_d07/audit_test.cpp'
text = source.read_text(encoding='utf-8')
def body(name):
    start = text.index('TEST_F(D07, ' + name + ')')
    end = text.find('\nTEST_F(', start + 1)
    return text[start:end if end >= 0 else text.index('\n} // namespace', start)]
independent = 'TwoIndependentCommandsNeedTwoUndoUnits'
naive = 'NaiveSequentialFallbackCannotSatisfyOneUndo_RED'
write(PKG / 'test-bindings.json', {
    'status': 'BLOCKED_VERIFICATION', 'activate_for_product_repair': False,
    'independent_commands': {'id': 'fc04.d07.D07.' + independent, 'role': 'blocking-characterization',
        'source': str(source.relative_to(ROOT)).replace('\\', '/'), 'source_sha256': sha(source),
        'test_body_sha256': hashlib.sha256(body(independent).encode()).hexdigest(),
        'expected': 'Two independent calls create two Undo units; first Undo leaves first mutation; second restores initial objects; two Redo calls restore both.'},
    'naive_fallback': {'historical_id': 'fc04.d07.D07.' + naive, 'role': 'negative-control',
        'historical_assertion_preserved': True, 'test_body_sha256': hashlib.sha256(body(naive).encode()).hexdigest(),
        'historical_expected_observation': 'FAIL at records-equal-after-one-Undo assertion; this demonstrates the fallback limitation, not grouped-intent conformance.',
        'current_characterization_oracle': 'fc04.d07.D07.' + independent,
        'positive_grouped_acceptance': False},
    'explicit_intent': {'required': True, 'planned_id': 'fc04.d07.D07.ExplicitIntentGroupProducesOneUndoUnit',
        'real_entry': None, 'owner_source_binding': None, 'status': 'NOT_RUN_MISSING_PRODUCTION_ENTRY',
        'expected': 'Real multi-Operation explicit intent; coherent local commit; exactly one Undo unit; whole-intent Undo and fresh-identity Redo.'},
    'legacy_entry_probe': {'id': 'fc04.d07.D07.RequiredMultiIntentEntryIsUnavailable_RED',
        'role': 'historical-gap-probe', 'not_grouped_commit_oracle': True},
    'oom': {'id': 'FC04D07PostcommitReceiptGrowthFault', 'role': 'blocking-product-regression',
        'expected': 'No termination or canonical commit stranded without History; all existing fault assertions preserved.'},
    'preserve': {'d07_existing_passes': 11, 'selection_regressions': 160, 'test_sources_unchanged': True,
        'historical_failures_immutable': True, 'D06_B': 'OPEN'}
})
write(PKG / 'package.json', {
    'task_id': 'FC04-D07-P31-CORRECTION', 'version': '0.2', 'status': 'BLOCKED_VERIFICATION',
    'repository': 'Mostorm-Labs/axiom', 'branch': 'codex/fc04-d07-selection-audit',
    'source_revision': BASE, 'feature_anchor': 'c7f874395d55952e9d5288e2a336a21747f37f11',
    'supersedes_only': 'Contradictory P31 independent-call/grouped-intent test mapping; v0.1 files and results remain immutable.',
    'authorization': 'P31 test-binding/evidence correction only; no product or test source edits; no product repair activation.',
    'allowed_paths': [PKG.relative_to(ROOT).as_posix() + '/', RESULT.relative_to(ROOT).as_posix() + '/'],
    'bindings': 'test-bindings.json', 'authority_inputs': ['current.json', 'acceptance.json', 'correction.json'],
    'required_preflights': ['PackageBindingPreflight', 'EvidenceContractPreflight', 'D07Preflight'],
    'terminal_blocker': 'BLOCKED_VERIFICATION: real explicit-intent production entry is absent; do not implement it in this task.',
    'product_repair_authorized': False, 'D06_B': 'OPEN'
})

env = json.loads((ROOT / '.aegis/results/FC04-D07-AUDIT/current-selection/raw/environment-identity.json').read_text(encoding='utf-8'))
for a in env['artifacts']:
    assert sha(a['path']) == a['sha256'], a['path']
write(RESULT / 'sut-identity.json', {'starting_head': BASE, 'reused_exact_artifacts': env['artifacts'],
    'compiled_source_binding': '.aegis/results/FC04-D07-AUDIT/current-selection/raw/audit-source-bindings.json',
    'build_performed_this_task': False, 'platform': env['platform']})

for name, src in [('d07', '.aegis/results/FC04-D07-REPAIR-PREFLIGHT/raw/original-fourteen-names.txt'),
                  ('selection', '.aegis/results/FC04-D07-AUDIT/current-selection/raw/existing-names.txt')]:
    names = (ROOT / src).read_text(encoding='utf-8-sig').splitlines()
    assert len(names) == (14 if name == 'd07' else 160) and len(names) == len(set(names))
    target = RESULT / 'raw' / (name + '-names.txt')
    target.write_text('\n'.join(names) + '\n', encoding='utf-8')
    run(['ctest', '--test-dir', str(ROOT / 'out/fc04-d07-selection/build'), '--tests-from-file', str(target),
         '--no-tests=error', '--output-on-failure', '--timeout', '180', '--output-junit',
         str(RESULT / 'raw' / (name + '.xml'))], name)
print(json.dumps({'package': str(PKG), 'result': str(RESULT), 'status': 'MATERIALIZED_BLOCKED_BINDING_CANDIDATE'}))
