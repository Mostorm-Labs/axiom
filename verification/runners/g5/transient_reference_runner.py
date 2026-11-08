"""Single fixed ABBA local comparison, independent of closed UR0 windows."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
IDS = ('UR1A-A1', 'UR1A-B1', 'UR1A-B2', 'UR1A-A2')
EXCLUDED = ('.aegis/results/G5-UR0b/',
            'verification/evidence/performance/g5-unified-render-ur0b/',
            '.aegis/results/G5-UR1a-transient-reference/',
            'verification/evidence/performance/g5-ur1a-transient-reference/')


def source_identity(full=False):
    spec = importlib.util.spec_from_file_location('ur1a_source_basis', HERE / 'ur0/source_identity.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    observed = module.identity(ROOT)
    files = {k: v for k, v in observed['files'].items() if not k.startswith(EXCLUDED)}
    encoded = json.dumps(files, sort_keys=True, separators=(',', ':')).encode()
    value = {'schema': 'g5-ur1a-source-identity-v1', 'revision': observed['revision'],
             'source_tree_sha256': hashlib.sha256(encoded).hexdigest(), 'excluded_self_outputs': list(EXCLUDED)}
    if full:
        value['files'] = files
    return value


def new_ledger():
    return {'schema': 'g5-ur1a-local-accounting-v1', 'planned_ids': list(IDS),
            'historical_starts': 35, 'historical_charged_seconds': 1276,
            'nominal_seconds_max': 720, 'attempts': [], 'remaining_status': 'PENDING'}


def reserve(ledger, identity):
    rows = ledger['attempts']
    if (ledger['remaining_status'] != 'PENDING' or len(rows) >= 4
            or identity != IDS[len(rows)] or any(r['status'] == 'STARTED' for r in rows)):
        raise ValueError('closed, concurrent, duplicate or out-of-order local attempt')
    rows.append({'id': identity, 'status': 'STARTED', 'reserved_seconds': 180,
                 'rss_limit_mib': 3072, 'charged_seconds': 180})


def complete(ledger, guard):
    if not ledger['attempts'] or ledger['attempts'][-1]['status'] != 'STARTED':
        raise ValueError('no active local attempt')
    row = ledger['attempts'][-1]
    ok = guard is not None and guard['reason'] == 'completed' and guard['cleanup_complete']
    row.update(status='COMPLETED' if ok else 'STOPPED',
               charged_seconds=math.ceil(guard['elapsed_s']) if guard else 180, guard=guard)
    if not ok:
        ledger['remaining_status'] = 'MISSING'
    elif len(ledger['attempts']) == 4:
        ledger['remaining_status'] = 'NONE'


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def execute(a, b, oracle, output):
    output.mkdir(parents=True, exist_ok=True)
    ledger_path = output / 'sampling-ledger.json'
    if ledger_path.exists():
        raise RuntimeError('local comparison already reserved; no implicit retry')
    spec = importlib.util.spec_from_file_location('ur1a_bounded_worker', HERE / 'bounded_worker.py')
    guard = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guard)
    frozen = source_identity(True)
    write(output / 'sampling-source.json', frozen)
    ledger = new_ledger()
    write(ledger_path, ledger)
    for identity in IDS:
        if source_identity()['source_tree_sha256'] != frozen['source_tree_sha256']:
            raise RuntimeError('source changed before local attempt')
        reserve(ledger, identity)
        write(ledger_path, ledger)
        sdk = a if identity.startswith('UR1A-A') else b
        row = output / identity
        command = ['node', str(HERE / 'transient_reference.mjs'), str(sdk.resolve()), str(row), str(oracle.resolve())]
        try:
            result = guard.run(command, output / identity, 180, 3072 * 1024)
        except BaseException:
            complete(ledger, None)
            write(ledger_path, ledger)
            raise
        complete(ledger, result)
        row_result = row / 'worker-result.json'
        if row_result.exists():
            observed = json.loads(row_result.read_text())
            ledger['attempts'][-1]['worker_status'] = observed['status']
        else:
            ledger['attempts'][-1]['worker_status'] = 'MISSING'
        write(ledger_path, ledger)
        print(json.dumps({'id': identity, 'guard': result['reason'], 'charged_seconds': ledger['attempts'][-1]['charged_seconds']}), flush=True)
        if result['reason'] != 'completed' or not result['cleanup_complete']:
            break
    summary = {'schema': 'g5-ur1a-local-summary-v1', 'rows': [],
               'source_tree_sha256': frozen['source_tree_sha256'],
               'scope': 'two samples per version; local exploratory comparison, no universal speed or G5 PASS'}
    for identity in IDS:
        p = output / identity / 'worker-result.json'
        if p.exists():
            row = json.loads(p.read_text())
            summary['rows'].append({'id': identity, 'status': row['status'], 'metrics': row.get('metrics'),
                                    'pixels': row.get('pixels'), 'error': row.get('error')})
        else:
            summary['rows'].append({'id': identity, 'status': 'MISSING'})
    summary['new_starts'] = len(ledger['attempts'])
    summary['new_charged_seconds'] = sum(r['charged_seconds'] for r in ledger['attempts'])
    write(output / 'comparison.json', summary)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source-identity', action='store_true')
    p.add_argument('--a-sdk', type=Path)
    p.add_argument('--b-sdk', type=Path)
    p.add_argument('--oracle', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--execute', action='store_true')
    a = p.parse_args()
    if a.source_identity:
        print(json.dumps(source_identity()))
    elif a.execute and all((a.a_sdk, a.b_sdk, a.oracle, a.output)):
        execute(a.a_sdk, a.b_sdk, a.oracle, a.output)
    else:
        p.error('explicit --execute and all artifact/output paths are required')


if __name__ == '__main__':
    main()
