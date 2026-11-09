"""UR1d exact four-start resource/recovery window. No seed, retry or smoke."""
import argparse
import hashlib
import importlib.util
import json
import math
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
TASK = 'G5-UR1d-resource-recovery'
RESULT = ROOT / '.aegis/results' / TASK
IDS = ('UR1D-CORE-Q1', 'UR1D-CORE-N1', 'UR1D-REC-Q1', 'UR1D-REC-N1')
LIMITS = (180, 180, 240, 240)

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''): h.update(block)
    return h.hexdigest()

def write(path, value):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(value, indent=2) + '\n')

def source_identity(full=False, root=ROOT):
    source = module('ur1d_source', HERE / 'ur0/source_identity.py')
    source.EXCLUDED += (
        '.aegis/results/G5-UR1c2-canonical-prepared-body/',
        'verification/evidence/performance/g5-ur1c2-canonical-prepared-body/',
        '.aegis/results/G5-UR1c2-working-set-repair-r1/',
        'verification/evidence/performance/g5-ur1c2-working-set-repair-r1/',
        '.aegis/results/' + TASK + '/',
        'verification/evidence/performance/g5-ur1d-resource-recovery/')
    value = source.identity(root)
    if not full: value.pop('files')
    return value

def artifacts(root):
    return {str(p.relative_to(root)): sha(p) for p in sorted(Path(root).rglob('*')) if p.is_file()}

def protected(catalog):
    package = ROOT / '.aegis/packages' / TASK
    authority = json.loads((package / 'authority.lock.json').read_text())
    value = {}
    for row in authority['authority_sources'] + [authority[k] for k in
            ('publication_ref', 'accepted_control_review_ref', 'accepted_candidate_result_ref')]:
        if sha(ROOT / row['path']) != row['sha256']: raise RuntimeError('authority changed: ' + row['path'])
        value[row['path']] = row['sha256']
    for p in package.iterdir():
        if p.is_file(): value[str(p.relative_to(ROOT))] = sha(p)
    oracle = ROOT / 'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'
    value['oracle'] = sha(oracle)
    if value['oracle'] != 'ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105':
        raise RuntimeError('independent CPU oracle changed')
    value['catalog'] = sha(catalog)
    for row in json.loads(Path(catalog).read_text())['resources']:
        if sha(row['path']) != row['sha256']: raise RuntimeError('oracle resource changed')
        value[row['path']] = row['sha256']
    original = json.loads((ROOT / '.aegis/results/G5-UR1c1-prepared-body-index/driver-before-workers.json').read_text())
    for p, digest in original['files'].items():
        if p.startswith('/Applications/'):
            if sha(p) != digest: raise RuntimeError('frozen browser changed: ' + p)
            value[p] = digest
    playwright = (ROOT / 'verification/node_modules/playwright').resolve(strict=True)
    for base in [playwright, (playwright.parent / 'playwright-core').resolve(strict=True)]:
        for p in sorted(base.rglob('*')):
            if p.is_file(): value[str(p)] = sha(p)
    for name in ('node', 'python3.12'):
        value[name] = sha(Path(shutil.which(name)).resolve(strict=True))
    for p in sorted(HERE.rglob('*')):
        if p.is_file() and p.suffix in ('.mjs', '.py'): value[str(p.relative_to(ROOT))] = sha(p)
    return value

def new_ledger():
    return {'schema': 'g5-ur1d-four-worker-accounting-v1', 'planned_ids': list(IDS),
        'historical_starts': 54, 'historical_charged_seconds': 2107,
        'new_nominal_seconds_max': 840, 'original_cap_seconds': 8370,
        'attempts': [], 'remaining_status': 'PENDING'}

def reserve(ledger, identity):
    i = len(ledger['attempts'])
    if ledger['remaining_status'] != 'PENDING' or i >= len(IDS) or identity != IDS[i] or any(
            x['status'] == 'STARTED' for x in ledger['attempts']):
        raise ValueError('concurrent, duplicate, closed or unplanned start')
    ledger['attempts'].append({'id': identity, 'status': 'STARTED', 'reserved_seconds': LIMITS[i],
        'charged_seconds': LIMITS[i], 'rss_limit_mib': 3072})

def complete(ledger, guard, worker):
    if not ledger['attempts'] or ledger['attempts'][-1]['status'] != 'STARTED': raise ValueError('no active row')
    row = ledger['attempts'][-1]
    ok = guard is not None and guard['reason'] == 'completed' and guard['cleanup_complete'] and worker is not None and worker['status'] in ('OBSERVED', 'PRODUCT_FAILURE_OBSERVED')
    row.update(status='COMPLETED' if ok else 'STOPPED', guard=guard,
        worker_status=worker['status'] if worker else 'MISSING',
        charged_seconds=math.ceil(guard['elapsed_s']) if guard else row['reserved_seconds'])
    if not ok: ledger['remaining_status'] = 'MISSING'
    elif len(ledger['attempts']) == len(IDS): ledger['remaining_status'] = 'NONE'
    ledger['cumulative_starts'] = 54 + len(ledger['attempts'])
    ledger['cumulative_charged_seconds'] = 2107 + sum(x['charged_seconds'] for x in ledger['attempts'])
    if ledger['cumulative_charged_seconds'] > 8370: ledger['remaining_status'] = 'MISSING'

def freeze(qualification, normal, catalog):
    if (RESULT / 'local-functional/sampling-ledger.json').exists(): raise RuntimeError('window already reserved')
    value = source_identity(True)
    write(RESULT / 'source-identity.json', value)
    write(RESULT / 'worker-freeze.json', {'source_tree_sha256': value['source_tree_sha256'],
        'protected': protected(catalog), 'artifacts': {str(p.resolve()): artifacts(p) for p in (qualification, normal)}})

def execute(qualification, normal, oracle, catalog, output):
    if not output.resolve().is_relative_to(RESULT): raise RuntimeError('own result directory required')
    output.mkdir(parents=True, exist_ok=True)
    ledger_path = output / 'sampling-ledger.json'
    if ledger_path.exists(): raise RuntimeError('no repeat window')
    frozen = json.loads((RESULT / 'worker-freeze.json').read_text())
    guard_module = module('ur1d_guard', HERE / 'bounded_worker.py')
    ledger = new_ledger()
    write(ledger_path, ledger)
    functional_pair = None
    for i, identity in enumerate(IDS):
        sdk = qualification if i % 2 == 0 else normal
        try:
            stable = source_identity()['source_tree_sha256'] == frozen['source_tree_sha256'] and protected(catalog) == frozen['protected'] and all(artifacts(p) == frozen['artifacts'][str(p.resolve())] for p in (qualification, normal))
        except Exception as e:
            stable = False; ledger['identity_error'] = str(e)
        if not stable:
            ledger['remaining_status'] = 'MISSING'; ledger['identity_failure'] = True; write(ledger_path, ledger); break
        reserve(ledger, identity); write(ledger_path, ledger)
        command = ['node', str(HERE / 'resource_recovery.mjs'), 'core' if i < 2 else 'recovery',
            str(sdk.resolve()), str((output / identity).resolve()), 'qualification' if i % 2 == 0 else 'normal',
            str(oracle.resolve()), str(catalog.resolve())]
        try:
            observed = guard_module.run(command, output / identity, LIMITS[i], 3072 * 1024)
        except BaseException:
            complete(ledger, None, None); write(ledger_path, ledger); raise
        p = output / identity / 'worker-result.json'
        worker = json.loads(p.read_text()) if p.exists() else None
        try:
            stable = source_identity()['source_tree_sha256'] == frozen['source_tree_sha256'] and protected(catalog) == frozen['protected'] and artifacts(sdk) == frozen['artifacts'][str(sdk.resolve())]
        except Exception as e:
            stable = False; ledger['identity_error'] = str(e)
        write(output / (identity + '-identity-after.json'), {'stable': stable})
        if not stable: worker = {**(worker or {}), 'status': 'HARD_FAILURE', 'identity_failure': True}
        complete(ledger, observed, worker)
        if worker and worker['status'] == 'PRODUCT_FAILURE_OBSERVED': functional_pair = i // 2
        if functional_pair is not None:
            ledger['functional_failure_observed'] = True
            if i % 2 == 1 and i + 1 < len(IDS): ledger['remaining_status'] = 'MISSING'
        write(ledger_path, ledger)
        print(json.dumps({'id': identity, 'guard': observed['reason'], 'worker': worker['status'] if worker else 'MISSING', 'charged_seconds': ledger['attempts'][-1]['charged_seconds'], 'remaining': ledger['remaining_status']}), flush=True)
        if ledger['remaining_status'] == 'MISSING': break
    write(output / 'summary.json', {'ledger': ledger, 'unrun_ids': list(IDS[len(ledger['attempts']):]),
        'source_tree_sha256': frozen['source_tree_sha256'], 'new_charged_seconds': sum(x['charged_seconds'] for x in ledger['attempts'])})

def main():
    p = argparse.ArgumentParser();p.add_argument('--source-identity', action='store_true');p.add_argument('--freeze', action='store_true');p.add_argument('--execute', action='store_true')
    for name in ('qualification-sdk', 'normal-sdk', 'oracle', 'catalog', 'output'): p.add_argument('--' + name, type=Path)
    a = p.parse_args()
    if a.source_identity: print(json.dumps(source_identity()))
    elif a.freeze and all((a.qualification_sdk, a.normal_sdk, a.catalog)): freeze(a.qualification_sdk, a.normal_sdk, a.catalog)
    elif a.execute and all((a.qualification_sdk, a.normal_sdk, a.oracle, a.catalog, a.output)): execute(a.qualification_sdk, a.normal_sdk, a.oracle, a.catalog, a.output)
    else: p.error('complete explicit operation required')

if __name__ == '__main__': main()
