"""Reproducible P31 binding/design checks; never claims product/P32 PASS."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
PKG = Path(__file__).resolve().parent
OUT = ROOT / '.aegis/results/FC04-D07-AB-P31-v0.1'
GIT = 'D:/Softwares/Git/cmd/git.exe'
SUBJECT = '832866a2d001a247c4eb374c1fb1899258a6f6cf'
FEATURE = 'c7f874395d55952e9d5288e2a336a21747f37f11'
SOURCE_PATHS = [
    'runtime/interaction/include/canvas/interaction/editor_history.hpp',
    'runtime/interaction/include/canvas/interaction/canvas_editing_runtime.hpp',
    'runtime/interaction/include/canvas/interaction/interaction_ports.hpp',
    'runtime/interaction/src/editor_history.cpp', 'runtime/interaction/src/undoable_command.cpp',
    'runtime/interaction/src/undoable_command.hpp', 'runtime/interaction/src/canvas_editing_runtime.cpp',
    'runtime/interaction/CMakeLists.txt',
    'runtime/semantic/src/operation_engine.cpp', 'runtime/semantic/src/atomic_apply.cpp', 'runtime/semantic/src/atomic_apply.hpp',
    'runtime/semantic/src/indexed_object_store.cpp', 'runtime/semantic/src/object_store_mutator.hpp',
    'runtime/semantic/src/applied_operation_ledger.cpp', 'runtime/semantic/src/change_set_builder.cpp',
    'runtime/semantic/src/change_set_builder.hpp', 'runtime/semantic/src/object_index.cpp', 'runtime/semantic/src/object_index.hpp',
    'runtime/semantic/include/canvas/semantic/operation_engine.hpp',
    'runtime/semantic/include/canvas/semantic/indexed_object_store.hpp',
    'runtime/semantic/include/canvas/semantic/applied_operation_ledger.hpp',
    'runtime/scene/src/incremental_runtime_coordinator.cpp', 'verification/fc04_d07/audit_test.cpp',
    'verification/fc04_d07/postcommit_fault.cpp', 'verification/fc04_d07/CMakeLists.txt',
    'tools/check_runtime_abi_manifest.py', 'semantic-sdk.lock.json', 'r1-full-skia-sdk.lock.json', 'AGENTS.md',
    '.aegis/packages/FC04-D07-P31-CORRECTION-v0.2/test-bindings.json',
    '.aegis/results/FC04-D07-P31-CORRECTION-v0.2/report.md',
    '.aegis/results/FC04-D07-P31-CORRECTION-v0.2/result.json',
    '.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/d07-names.txt',
    '.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/selection-names.txt',
    '.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/d07.xml',
    '.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/selection.xml',
    '.aegis/results/FC04-D07-AUDIT/commands.json',
    '.aegis/results/FC04-D07-REPAIR-PREFLIGHT/authority-inputs/current-d07-authority.json',
    '.aegis/results/FC04-D07-REPAIR-PREFLIGHT/authority-inputs/scope-acceptance.json',
    '.aegis/results/FC04-D07-REPAIR-PREFLIGHT/authority-inputs/07-14-current.json',
]
PACKAGE_PATHS = ['package.json', 'closure.md', 'design.md', 'test-matrix.json', 'preflight.py']
def sha_bytes(data): return hashlib.sha256(data).hexdigest()
def sha(path): return sha_bytes(path.read_bytes())
def read(path): return json.loads(path.read_text(encoding='utf-8'))
def git(*args): return subprocess.run([GIT, *args], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
def text_content(name):
    raw = read(OUT / 'authority' / (name + '.json'))
    return json.loads(next(c['text'] for c in raw['content'] if c['type'] == 'text'))['text']
def tests(name):
    cases = list(ET.parse(ROOT / ('.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/' + name + '.xml')).getroot().iter('testcase'))
    return {'count': len(cases), 'failed': [c.get('name') for c in cases if c.find('failure') is not None],
            'skipped': [c.get('name') for c in cases if c.find('skipped') is not None],
            'ids': [c.get('name') for c in cases]}
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('phase', choices=['lock', 'PackageBindingPreflight', 'EvidenceContractPreflight', 'DesignPreflight'])
    args = ap.parse_args()
    if args.phase == 'lock':
        assert not (PKG / 'source-lock.json').exists(), 'Do not overwrite a lock'
        files = []
        for path in SOURCE_PATHS:
            blob = git('show', SUBJECT + ':' + path)
            assert blob.returncode == 0 and sha_bytes(blob.stdout) == sha(ROOT / path), path
            files.append({'path': path, 'sha256': sha(ROOT / path)})
        write(PKG / 'source-lock.json', {'subject': SUBJECT, 'source_files': files,
              'package_files': [{'path': p, 'sha256': sha(PKG / p)} for p in PACKAGE_PATHS]})
        print('source-lock: PASS, files=' + str(len(files)))
        return 0
    lock = read(PKG / 'source-lock.json')
    package = read(PKG / 'package.json')
    matrix = read(PKG / 'test-matrix.json')
    design = (PKG / 'design.md').read_text(encoding='utf-8')
    closure = (PKG / 'closure.md').read_text(encoding='utf-8')
    checks = []
    def check(name, value, detail=''):
        checks.append({'name': name, 'status': 'PASS' if value else 'BLOCKED', 'detail': detail})
    head = git('rev-parse', 'HEAD').stdout.decode().strip()
    if args.phase == 'PackageBindingPreflight':
        remote = git('remote', 'get-url', 'origin').stdout.decode().strip().removesuffix('.git').lower()
        check('repository', remote == 'https://github.com/mostorm-labs/axiom', remote)
        check('anchor_ancestry', git('merge-base', '--is-ancestor', SUBJECT, head).returncode == 0 and
              git('merge-base', '--is-ancestor', FEATURE, head).returncode == 0, {'head': head, 'subject': SUBJECT, 'feature': FEATURE})
        check('source_and_prior_evidence_exact', all(sha(ROOT / f['path']) == f['sha256'] and
              sha_bytes(git('show', SUBJECT + ':' + f['path']).stdout) == f['sha256'] for f in lock['source_files']), len(lock['source_files']))
        check('package_identity', all(sha(PKG / f['path']) == f['sha256'] for f in lock['package_files']), lock['package_files'])
        current, scope = text_content('current'), text_content('scope')
        check('accepted_current_authority', 'ACCEPTED FOR THE BOUNDED LOCAL D07 REQUIREMENT' in current and
              '4064c57a590c819aabc5e9efbf9a2b53' in current and 'fail before canonical mutation' in scope,
              'Live current and acceptance snapshots are recorded under result/authority.')
        check('product_authorization_scope', package['product_P32_authorized_if_preflights_pass'] and
              package['task_anchor']['revision'] == SUBJECT and package['continue_until_terminal_state'], package['implementation_scope'])
    elif args.phase == 'EvidenceContractPreflight':
        prior = read(ROOT / '.aegis/results/FC04-D07-P31-CORRECTION-v0.2/result.json')
        d, s = tests('d07'), tests('selection')
        names = lambda name: (ROOT / ('.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/' + name + '-names.txt')).read_text(encoding='utf-8').splitlines()
        check('exact_D07_baseline', d['count'] == 14 and len(d['failed']) == 3 and not d['skipped'] and set(d['ids']) == set(names('d07')),
              {'count': d['count'], 'failed': d['failed']})
        check('exact_selection_baseline', s['count'] == 160 and not s['failed'] and not s['skipped'] and len(set(names('selection'))) == 160 and set(s['ids']) == set(names('selection')), s['count'])
        ids = [t['id'] for t in matrix['tests']]
        check('explicit_real_producer_oracle_planned', 'fc04.d07.D07.ExplicitIntentGroupProducesOneUndoUnit' in ids and
              'CanvasEditingRuntime intent producer' in next(t['sut'] for t in matrix['tests'] if t['id'].endswith('ExplicitIntentGroupProducesOneUndoUnit')),
              'Planned test is NOT_RUN; P32 must bind the real production API.')
        check('negative_control_and_oom_separate', all(x in ids for x in ['fc04.d07.D07.NaiveSequentialFallbackCannotSatisfyOneUndo_RED',
              'fc04.d07.D07.TwoIndependentCommandsNeedTwoUndoUnits', 'FC04D07PostcommitReceiptGrowthFault']) and prior['fresh_native_tests']['d07']['exit_code'] == 8,
              'Historical failures remain historical; no positive product verdict is synthesized.')
        check('required_invocations_and_no_zero', matrix['no_zero_match_as_pass'] and len(matrix['required_commands']) >= 6 and
              all(t['sut'] and t['assertions'] for t in matrix['tests']), 'Real discovery, native, ABI, allocation, source and test evidence required in P32.')
    else:
        history = (ROOT / 'runtime/interaction/include/canvas/interaction/editor_history.hpp').read_text(encoding='utf-8')
        runtime = (ROOT / 'runtime/interaction/src/canvas_editing_runtime.cpp').read_text(encoding='utf-8')
        atom = (ROOT / 'runtime/semantic/src/atomic_apply.cpp').read_text(encoding='utf-8')
        engine = (ROOT / 'runtime/semantic/src/operation_engine.cpp').read_text(encoding='utf-8')
        check('actual_single_operation_gap', 'semantic::Operation forward;' in history and 'executeUndoable(const semantic::Operation&)' in history and
              'operations.size() != 1U' in runtime and 'submitAdmitted(' in runtime, 'Grouped owner is a P32 obligation, not an existing API.')
        check('actual_semantic_reservation_gap', 'prepareContainerPlan' in atom and 'std::set<ObjectId>' in atom and
              'reserveContainerEntry' in engine and 'prepareChangeSet' in engine,
              'Existing per-call preflight/storage/ledger allocations are not a whole-intent no-fail proof.')
        check('explicit_lifecycle', all(x in design for x in ['`begin`','`prepare`','`commit`','`abort`','beginExplicitIntent','prepareExplicitIntent','commitExplicitIntent','abortExplicitIntent']))
        check('no_implicit_group_or_global_transaction', all(x in closure for x in ['neither temporal adjacency nor operation IDs imply grouping', 'No D06-B', 'No new product/public ABI', 'no general transaction']))
        check('per_operation_and_coherent_visibility', all(x in design for x in ['own ID, generation, stamp and ChangeSet', 'DataBridge notifications', 'partial failed intent', 'Authority/design conflict']))
        check('actual_forward_failure_and_OOM_constraints',
              'No synthetic pass by moving a failpoint' in closure and 'NEW_HIGH_IMPACT_FAILURE_MODE' in closure and
              any('History/store/index/ledger/ChangeSet' in str(t['assertions']) for t in matrix['tests']),
              'P32 must prove every real boundary; this check does not infer proof from a design statement.')
    failures = [c['name'] for c in checks if c['status'] != 'PASS']
    result = {'preflight': args.phase, 'status': 'BLOCKED' if failures else 'PASS', 'exit_code': 2 if failures else 0,
        'scope': 'P31 structural binding/design preflight only; no product behavior verified',
        'subject': SUBJECT, 'actual_head': head, 'package_sha256': sha(PKG / 'package.json'),
        'source_lock_sha256': sha(PKG / 'source-lock.json'), 'checks': checks,
        'P32_producer': 'NOT_IMPLEMENTED', 'P32_product_tests': 'NOT_RUN',
        'p32_technical_gate': 'Prove true no-fail commit, per-operation publication and actual injected boundaries; return blocker if impossible.'}
    write(OUT / (args.phase + '.json'), result)
    print(json.dumps({'phase': args.phase, 'status': result['status'], 'failed': failures, 'product_tests': 'NOT_RUN'}))
    return result['exit_code']
if __name__ == '__main__': sys.exit(main())
