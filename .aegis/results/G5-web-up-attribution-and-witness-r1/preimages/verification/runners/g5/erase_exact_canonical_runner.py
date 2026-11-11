"""Frozen-window guard for erase exact/canonical work. Never touches user tabs."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
RESULT = ROOT / '.aegis/results/G5-erase-exact-canonical-r1'
WINDOWS = {'G5EC1-BASELINE': (180, 1), 'G5EC1-DIAGNOSTIC': (180, 1),
           'G5EC1-FINAL-PAIRS': (360, 6), 'G5EC1-STRESS': (180, 1),
           'G5EC1-STRESS-CONTINUE-R1': (153, 3), 'G5EC1-STRESS-CONTINUE-R2': (151, 3)}


def sha(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''):
            h.update(b)
    return h.hexdigest()


def read(p):
    return json.loads(Path(p).read_text())


def write(p, v):
    Path(p).parent.mkdir(parents=True, exist_ok=True)
    Path(p).write_text(json.dumps(v, indent=2) + '\n')


def reserve(ledger, name):
    if name not in WINDOWS or any(x['id'] == name for x in ledger['attempts']) or any(x['status'] == 'STARTED' for x in ledger['attempts']):
        raise ValueError('foreign, repeated or concurrent window')
    if ledger['charged_seconds'] + WINDOWS[name][0] > 900:
        raise ValueError('cumulative allowance exhausted')
    ledger['attempts'].append({'id': name, 'status': 'STARTED', 'reserved_seconds': WINDOWS[name][0], 'rss_mib': 4096})


def charge(guard, limit):
    t = guard.get('elapsed_s')
    return math.ceil(t) if isinstance(t, (int, float)) and not isinstance(t, bool) and math.isfinite(t) and 0 <= t <= limit + 5 else limit


def classify(guard, worker, stable, expected):
    ok = stable and guard.get('reason') == 'completed' and guard.get('returncode') == 0 and guard.get('cleanup_complete') is True and guard.get('remaining_owned_pids') == []
    ok = ok and worker.get('status') == 'OBSERVED' and len(worker.get('rows', [])) == expected
    ok = ok and all(x.get('status') == 'QUALIFIED' for x in worker.get('rows', []))
    return 'COMPLETED' if ok else 'STOPPED'


def inputs(request):
    files = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=ROOT).decode().split('\0')
    source = {f: sha(ROOT / f) for f in sorted(set(files)) if f and (ROOT / f).is_file() and
              f.startswith(('runtime/', 'sdk/', 'cmake/', 'verification/runners/g5/', 'verification/tests/')) and
              Path(f).suffix in ('.cpp', '.hpp', '.h', '.mjs', '.py', '.cmake', '.json', '.txt')}
    sdk = {}
    for v in request['variants']:
        p = Path(v['sdk'])
        sdk[v['name']] = {str(q.relative_to(p)): sha(q) for q in sorted(p.rglob('*')) if q.is_file()}
        if len(sdk[v['name']]) != 36:
            raise ValueError('normal SDK inventory mismatch')
    for v in (request['fixture'], request['oracle']):
        if sha(v['path']) != v['sha256']:
            raise ValueError('fixture/oracle mismatch')
    old = read(ROOT / '.aegis/results/G5-desktop-render-budget-r1/normal-artifacts.json')
    if 'old' in sdk and sdk['old'] != old['files']:
        raise ValueError('frozen4317 normal SDK changed')
    return {'source': source, 'SDK': sdk, 'fixture': request['fixture'], 'oracle': request['oracle']}


def execute(request_path):
    request = read(request_path)
    if request['id'] == 'G5EC1-STRESS-CONTINUE-R1':
        authorization = ROOT / '.aegis/results/G5-erase-exact-canonical-control-r1/stress-harness-continuation-r1.json'
        if sha(authorization) != '2b37524a38453990cc95aeccfcdb5a5cbdd507acf0e771ae9175c2105330744e':
            raise ValueError('continuation authorization mismatch')
    if request['id'] == 'G5EC1-STRESS-CONTINUE-R2':
        authorization = ROOT / '.aegis/results/G5-erase-exact-canonical-control-r1/stress-harness-continuation-r2.json'
        if sha(authorization) != 'eaa87b66af8a3cc4c0e1bcf7e0897127e4262dc7b944bc8814becfdbe5394aad':
            raise ValueError('final continuation authorization mismatch')
    p = RESULT / 'sampling-ledger.json'
    ledger = read(p) if p.exists() else {'schema': 'g5-erase-exact-canonical-ledger-v1', 'charged_seconds': 0, 'cap_seconds': 900, 'attempts': []}
    reserve(ledger, request['id'])
    before = inputs(request)
    out = RESULT / 'local-web' / request['id']
    if out.exists():
        raise ValueError('existing output; window cannot restart')
    out.mkdir(parents=True)
    write(out / 'freeze.json', {'inputs': before, 'request': request, 'request_sha256': sha(request_path)})
    write(p, ledger)
    spec = importlib.util.spec_from_file_location('guard', Path(__file__).with_name('bounded_worker.py'))
    guard_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guard_module)
    driver = 'erase_undo_readiness.mjs' if request.get('undoReadiness') else 'erase_exact_canonical.mjs'
    guard = guard_module.run(['node', str(Path(__file__).with_name(driver)), str(request_path), str(out)],
                             out, WINDOWS[request['id']][0], 4096 * 1024)
    try:
        after = inputs(request)
    except Exception as e:
        after = {'error': repr(e)}
    write(out / 'inputs-after.json', after)
    worker = read(out / 'worker-result.json') if (out / 'worker-result.json').exists() else {}
    expected = WINDOWS[request['id']][1]
    if request.get('undoReadiness') and worker.get('rows'):
        lengths = [v.get('moves') for v in worker['rows']]
        if request.get('stressCandidate') and lengths == [1600, 1600, None]:
            expected = 3
        if lengths == [800, 1600] or lengths == request.get('undoLengths') or (lengths == [800] and worker['rows'][0]['localization']['up_ms'] >= 10000):
            expected = len(lengths)
    n = charge(guard, WINDOWS[request['id']][0])
    ledger['attempts'][-1].update(status=classify(guard, worker, before == after, expected),
                                  guard=guard, source_stable=before == after, charged_seconds=n)
    ledger['charged_seconds'] += n
    write(p, ledger)
    print(json.dumps(ledger), flush=True)
    return 0 if ledger['attempts'][-1]['status'] == 'COMPLETED' else 1


if __name__ == '__main__':
    raise SystemExit(execute(Path(sys.argv[1]).resolve()))
