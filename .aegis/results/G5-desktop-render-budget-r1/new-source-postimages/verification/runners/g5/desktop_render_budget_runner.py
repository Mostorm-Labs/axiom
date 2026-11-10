"""Single bounded, serial desktop-area observation; existing user pages untouched."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
RESULT = ROOT / '.aegis/results/G5-desktop-render-budget-r1'
WINDOW = 'G5MB1-AREA-COMPARE'


def sha(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1048576), b''):
            h.update(chunk)
    return h.hexdigest()


def read(p):
    return json.loads(Path(p).read_text())


def write(p, value):
    Path(p).parent.mkdir(parents=True, exist_ok=True)
    Path(p).write_text(json.dumps(value, indent=2) + '\n')


def artifacts(root):
    root = Path(root)
    return {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def source():
    names = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=ROOT).decode().split('\0')
    return {p: sha(ROOT / p) for p in sorted(set(names)) if p and (ROOT / p).is_file() and
            p.startswith(('runtime/', 'sdk/', 'cmake/', 'verification/runners/g5/', 'verification/tests/')) and
            Path(p).suffix in ('.cpp', '.hpp', '.h', '.cmake', '.mjs', '.py', '.json', '.txt')}


def reserve(ledger, request):
    variants = request.get('variants', [])
    if request.get('id') != WINDOW or ledger.get('attempts') or [v.get('name') for v in variants] != ['old', 'candidate']:
        raise ValueError('one ordered budget comparison only')
    if Path(variants[0]['sdk']).resolve() == Path(variants[1]['sdk']).resolve():
        raise ValueError('distinct SDK identities required')
    ledger['attempts'] = [{'id': WINDOW, 'status': 'STARTED', 'reserved_seconds': 300, 'rss_mib': 4096}]


def charge(guard):
    v = guard.get('elapsed_s')
    return math.ceil(v) if isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) and 0 <= v <= 305 else 300


def classify(guard, worker, stable):
    if not (stable and guard.get('reason') == 'completed' and guard.get('cleanup_complete') is True and
            guard.get('returncode') == 0 and guard.get('remaining_owned_pids') == [] and
            worker.get('status') == 'OBSERVED' and not worker.get('errors')):
        return 'STOPPED'
    expected = [(w, kind, v) for w in (600, 1431) for kind in ('shape', 'stroke') for v in ('old', 'candidate')]
    rows = worker.get('rows', [])
    if [(r.get('width'), r.get('kind'), r.get('variant')) for r in rows] != expected:
        return 'STOPPED'
    if any(r.get('status') != 'OBSERVED' or r.get('errors') or r.get('roundtrip', {}).get('status') != 'QUALIFIED' for r in rows):
        return 'STOPPED'
    return 'COMPLETED'


def inputs(request):
    old = read(ROOT / '.aegis/results/G5-UR2d-selection-up-continue-r1/final-normal-artifacts.json')
    new = read(RESULT / 'normal-artifacts.json')
    maps = {v['name']: artifacts(v['sdk']) for v in request['variants']}
    if maps != {'old': old['files'], 'candidate': new['files']}:
        raise ValueError('SDK mismatch')
    for value in (request['fixture'], request['oracle']):
        if sha(value['path']) != value['sha256']:
            raise ValueError('fixture/oracle mismatch')
    return {'SDK': maps, 'source': source(), 'fixture': request['fixture'], 'oracle': request['oracle']}


def execute(request_path):
    request = read(request_path)
    ledger_path = RESULT / 'sampling-ledger.json'
    ledger = read(ledger_path) if ledger_path.exists() else {'schema': 'g5-desktop-memory-ledger-v1', 'attempts': [], 'charged_seconds': 0}
    reserve(ledger, request)
    out = RESULT / 'local-web' / WINDOW
    if out.exists():
        raise ValueError('existing output; no rerun')
    before = inputs(request)
    out.mkdir(parents=True)
    write(out / 'freeze.json', {'request': request, 'request_sha256': sha(request_path), 'inputs': before})
    write(ledger_path, ledger)
    spec = importlib.util.spec_from_file_location('guard', Path(__file__).with_name('bounded_worker.py'))
    guard_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guard_module)
    guard = guard_module.run(['node', str(Path(__file__).with_name('desktop_render_budget.mjs')), str(request_path), str(out)], out, 300, 4096 * 1024)
    worker = read(out / 'worker-result.json') if (out / 'worker-result.json').exists() else {}
    after = inputs(request)
    write(out / 'inputs-after.json', after)
    ledger['attempts'][0].update(status=classify(guard, worker, before == after), guard=guard,
                                 source_artifact_stable=before == after, charged_seconds=charge(guard))
    ledger['charged_seconds'] = charge(guard)
    write(ledger_path, ledger)
    print(json.dumps(ledger), flush=True)
    return 0 if ledger['attempts'][0]['status'] == 'COMPLETED' else 1


if __name__ == '__main__':
    raise SystemExit(execute(Path(sys.argv[1]).resolve()))
