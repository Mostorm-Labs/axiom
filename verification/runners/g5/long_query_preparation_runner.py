"""Exactly the two frozen table-proof windows; no repeated IDs or browser attach."""
from pathlib import Path
import hashlib, importlib.util, json, math, subprocess, sys

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = ROOT / '.aegis/packages/G5-long-query-preparation-r1'
RESULT = ROOT / '.aegis/results/G5-long-query-preparation-r1'
PACKAGE_SHA = 'b9cc0465e8f7900145396653a14f216d0a366e55450faabf124f92619df7a523'
LIMITS = {'G5LQP1-PAIRS':360,'G5LQP1-DIAGNOSTIC':180,'G5LQP1-FEEDBACK':120}
ROSTERS = {'G5LQP1-PAIRS':['old-burst','candidate-burst','old-paced','candidate-paced','old-growth','candidate-growth'],'G5LQP1-DIAGNOSTIC':['candidate-burst-diagnostic','candidate-paced-diagnostic'],'G5LQP1-FEEDBACK':['candidate-mixed-feedback']}

def sha(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''):
            h.update(b)
    return h.hexdigest()

def read(p): return json.loads(Path(p).read_text())
def write(p, v):
    Path(p).parent.mkdir(parents=True, exist_ok=True)
    Path(p).write_text(json.dumps(v, indent=2) + '\n')
def inventory(p):
    return {str(q.relative_to(p)): sha(q) for q in sorted(Path(p).rglob('*')) if q.is_file()}

def reserve(ledger, req):
    name = req['id']
    if name not in LIMITS or [c['id'] for c in req['cases']] != ROSTERS[name]:
        raise ValueError('foreign window or case roster')
    if any(c.get('dpr') != 2 or c.get('css') != [1431, 1010] or c.get('seed') != 324508639 for c in req['cases']):
        raise ValueError('frozen dimensions/entropy changed')
    if len(ledger['attempts']) >= 3 or any(x['id'] == name or x['status'] == 'STARTED' for x in ledger['attempts']):
        raise ValueError('foreign/repeated/concurrent window')
    if ledger['charged_seconds'] + LIMITS[name] > 660:
        raise ValueError('660 second allowance exhausted')
    if any(a.get('hard_stop') for a in ledger['attempts']):
        raise ValueError('prior hard stop forbids remaining windows')
    expected_order = list(LIMITS)
    if name != expected_order[len(ledger['attempts'])]:
        raise ValueError('window order changed')
    ledger['attempts'].append(dict(id=name, status='STARTED', limit_seconds=LIMITS[name], rss_mib=4096))

def inputs(req):
    names = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=ROOT).decode().split('\0')
    source = {n: sha(ROOT/n) for n in sorted(set(names)) if n and (ROOT/n).is_file()
              and n.startswith(('runtime/', 'sdk/', 'cmake/', 'verification/runners/g5/', 'verification/tests/'))
              and Path(n).suffix in ('.cpp', '.h', '.hpp', '.mjs', '.cmake', '.json', '.py', '.txt')}
    if sha(req['fixture']['path']) != req['fixture']['sha256']:
        raise ValueError('fixture identity changed')
    sdks = {}
    for p in req['SDK_manifests']:
        m = read(ROOT/p)
        actual = inventory(Path(m['root']))
        if actual != m['files']:
            raise ValueError('immutable SDK inventory mismatch')
        sdks[m['root']] = actual
    if set(c['sdk'] for c in req['cases']) - set(sdks):
        raise ValueError('unbound SDK root')
    return dict(source=source, SDKs=sdks, fixture=req['fixture'], driver=sha(ROOT/req['driver']),
                HEAD=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip())

def execute(request_path):
    if sha(PACKAGE/'package.json') != PACKAGE_SHA:
        raise ValueError('frozen package changed')
    plan = read(RESULT/'sampling-plan.json')
    req = read(request_path)
    if plan['requests'].get(req['id']) != sha(request_path):
        raise ValueError('request differs from offline freeze')
    before = inputs(req)
    if before != read(RESULT/'inputs-frozen.json'):
        # Both windows may have different SDK sets. The source/HEAD remain exact.
        frozen = read(RESULT/'inputs-frozen.json')
        if before['source'] != frozen['source'] or before['HEAD'] != frozen['HEAD']:
            raise ValueError('source changed since offline freeze')
    lp = RESULT/'sampling-ledger.json'
    ledger = read(lp) if lp.exists() else dict(schema='g5-long-query-three-windows-v1', cap_seconds=660, charged_seconds=0, attempts=[])
    reserve(ledger, req)
    out = RESULT/'local-web'/req['id']
    out.mkdir(parents=True, exist_ok=False)
    write(out/'freeze.json', dict(request=req, request_sha256=sha(request_path), inputs=before))
    write(lp, ledger)
    spec = importlib.util.spec_from_file_location('guard', Path(__file__).with_name('bounded_worker.py'))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    guard = m.run(['node', str(ROOT/req['driver']), str(request_path), str(out)], out, LIMITS[req['id']], 4096*1024)
    after = inputs(req)
    write(out/'inputs-after.json', after)
    worker = read(out/'worker-result.json') if (out/'worker-result.json').exists() else {}
    elapsed = guard.get('elapsed_s')
    charged = math.ceil(elapsed) if isinstance(elapsed, (int, float)) and math.isfinite(elapsed) and 0 <= elapsed <= LIMITS[req['id']]+5 else LIMITS[req['id']]
    hard = guard['reason'] not in ('completed', 'nonzero_exit') or not guard['cleanup_complete'] or bool(guard['remaining_owned_pids']) or before != after
    ok = not hard and worker.get('status') == 'CAPTURED' and len(worker.get('rows', [])) == len(req['cases'])
    ledger['attempts'][-1].update(status='COMPLETED' if ok else 'STOPPED', guard=guard, source_stable=before == after,
                                  charged_seconds=charged, worker_status=worker.get('status'), hard_stop=hard)
    ledger['charged_seconds'] += charged
    write(lp, ledger)
    print(json.dumps(ledger), flush=True)
    return 0 if ok else 1

if __name__ == '__main__':
    raise SystemExit(execute(Path(sys.argv[1]).resolve()))
