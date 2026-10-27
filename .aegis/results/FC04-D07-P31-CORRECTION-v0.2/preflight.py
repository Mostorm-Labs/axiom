"""Task-local P31 structural checks, not a product runner or official Gate.

The supplied correction ZIP contains no executable preflight implementation.
This checker records its explicit required bindings against the actual worktree.
A missing positive production entry must BLOCK, never become a skipped PASS.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
PKG = ROOT / '.aegis/packages/FC04-D07-P31-CORRECTION-v0.2'
GIT = 'D:/Softwares/Git/cmd/git.exe'

def load(path):
    return json.loads(path.read_text(encoding='utf-8'))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def authority(name):
    response = load(OUT / 'authority-inputs' / (name + '.json'))
    return json.loads(next(c['text'] for c in response['content'] if c['type'] == 'text'))

def suite(name):
    xml = ET.parse(OUT / 'raw' / (name + '.xml')).getroot()
    cases = list(xml.iter('testcase'))
    return {'tests': len(cases), 'passed': sum(c.find('failure') is None and c.find('skipped') is None for c in cases),
            'failed_ids': [c.get('name') for c in cases if c.find('failure') is not None],
            'skipped': sum(c.find('skipped') is not None for c in cases),
            'ids': [c.get('name') for c in cases]}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('phase', choices=['PackageBindingPreflight', 'EvidenceContractPreflight', 'D07Preflight'])
    args = parser.parse_args()
    pkg = load(PKG / 'package.json')
    bindings = load(PKG / 'test-bindings.json')
    checks = []
    def check(name, ok, detail):
        checks.append({'check': name, 'status': 'PASS' if ok else 'BLOCKED', 'detail': detail})

    if args.phase == 'PackageBindingPreflight':
        head = subprocess.check_output([GIT, 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        descendant = subprocess.run([GIT, 'merge-base', '--is-ancestor', pkg['source_revision'], head], cwd=ROOT).returncode
        check('source_identity', descendant == 0, {'subject': pkg['source_revision'], 'actual_head': head})
        docs = load(PKG / 'input-identity.json')['files']
        check('supplied_documents_immutable', all(sha(PKG / d['path']) == d['sha256'] for d in docs), docs)
        current = authority('current')
        check('current_D07_acceptance_link',
              'Accepted Scope Supersession' in current['text'] and '4064c57a590c819aabc5e9efbf9a2b53' in current['text'],
              {'page': current['id'], 'last_edited': current['page_last_edited_at']})
        check('independent_test_unchanged', sha(ROOT / bindings['independent_commands']['source']) == bindings['independent_commands']['source_sha256'],
              bindings['independent_commands'])
        check('negative_control_separated', bindings['naive_fallback']['role'] == 'negative-control' and
              not bindings['naive_fallback']['positive_grouped_acceptance'], bindings['naive_fallback'])
        check('real_explicit_intent_owner_bound', bool(bindings['explicit_intent']['real_entry']) and
              bool(bindings['explicit_intent']['owner_source_binding']),
              'No production group entry. EditorHistory::PreparedEntry has one forward Operation; executeUndoable takes one Operation. '
              'CanvasEditingRuntime::submit(span, source) rejects multi-Operation/local-interaction batches. See source-bindings.json.')
        check('OOM_still_blocking', bindings['oom']['role'] == 'blocking-product-regression', bindings['oom'])
    elif args.phase == 'EvidenceContractPreflight':
        preserved = load(OUT / 'historical-preservation.json')['verified_files']
        check('prior_results_immutable', all(sha(ROOT / p['path']) == p['sha256'] for p in preserved), {'verified_files': len(preserved)})
        check('exact_SUT_artifacts', all(sha(Path(a['path'])) == a['sha256'] for a in load(OUT / 'sut-identity.json')['reused_exact_artifacts']),
              'Reused original exact native executables; no product rebuild or source edits.')
        for name, count in [('d07', 14), ('selection', 160)]:
            actual = suite(name)
            frozen = (OUT / 'raw' / (name + '-names.txt')).read_text(encoding='utf-8').splitlines()
            check(name + '_nonempty_exact_invocations', len(actual['ids']) == count and set(actual['ids']) == set(frozen), actual)
        check('positive_oracle_SUT_invocation_bound', bool(bindings['explicit_intent']['real_entry']),
              {'status': 'NOT_RUN_MISSING_PRODUCTION_ENTRY', 'planned_test': bindings['explicit_intent']['planned_id'],
               'reason': 'No real entry/source/SUT invocation can be supplied; baseline negative probes cannot substitute.'})
        check('product_repair_not_activated', not pkg['product_repair_authorized'] and not bindings['activate_for_product_repair'],
              'Correction is a blocked binding candidate; no D07 implementation or Gate PASS.')
    else:
        actual = suite('d07')
        failures = ['fc04.d07.D07.RequiredMultiIntentEntryIsUnavailable_RED',
                    'fc04.d07.D07.NaiveSequentialFallbackCannotSatisfyOneUndo_RED', 'FC04D07PostcommitReceiptGrowthFault']
        check('11_existing_D07_passes_preserved', actual['passed'] == 11 and set(actual['failed_ids']) == set(failures) and actual['skipped'] == 0, actual)
        existing = suite('selection')
        check('160_Selection_passes_preserved', existing['passed'] == 160 and not existing['failed_ids'] and existing['skipped'] == 0, existing)
        check('explicit_group_positive_executable', bindings['explicit_intent']['planned_id'] in actual['ids'],
              'NOT_RUN: no real grouped-intent API; planned positive test is absent, not skipped or treated as PASS.')
        check('postcommit_OOM_product_requirement', bindings['oom']['id'] not in actual['failed_ids'],
              'Original blocking receipt-growth fault still fails; canonical generation 2, objects 3, History 0, Scene 1; exit 86.')

    blocked = any(c['status'] == 'BLOCKED' for c in checks)
    value = {'preflight': args.phase, 'status': 'BLOCKED_VERIFICATION' if blocked else 'PASS',
             'checker_scope': 'Task-local P31 structural evaluation only; not product verification, Authority approval, or Gate review.',
             'package_sha256': sha(PKG / 'package.json'), 'bindings_sha256': sha(PKG / 'test-bindings.json'),
             'source_revision': pkg['source_revision'], 'checks': checks,
             'command': [sys.executable, '-X', 'utf8', str(Path(__file__).resolve()), args.phase], 'exit_code': 2 if blocked else 0}
    (OUT / (args.phase + '.json')).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'preflight': args.phase, 'status': value['status'], 'exit_code': value['exit_code'],
                      'blocked_checks': [c['check'] for c in checks if c['status'] == 'BLOCKED']}))
    return value['exit_code']

if __name__ == '__main__':
    sys.exit(main())
