"""Closed UR2b allowance. Original functional worker/guard remain immutable."""
from pathlib import Path
import hashlib, importlib.util, json, math, subprocess, sys, time

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
TASK = 'G5-UR2b-exact-query-locality-r1'
RESULT = ROOT / '.aegis/results' / TASK
OUTPUT = ROOT / 'out' / TASK
IDS = ('UR2B1-FQ1', 'UR2B1-FN1', 'UR2B1-A1', 'UR2B1-B1', 'UR2B1-B2', 'UR2B1-A2', 'UR2B1-A3', 'UR2B1-B3')
HISTORY = {'starts': 64, 'charged_seconds': 3118, 'cap_seconds': 8370}


def module(name, path):
    s = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(s)
    s.loader.exec_module(m)
    return m


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n')


def identity(full=False):
    m = module('ur2b_source', HERE / 'ur0/source_identity.py')
    m.EXCLUDED += (f'.aegis/results/{TASK}/', 'verification/evidence/performance/g5-ur2b-exact-query-locality-r1/',
                   '.aegis/results/G5-UR1d-physical-loss-repair-r1/demo-server.log')
    v = m.identity(ROOT)
    if not full:
        v.pop('files')
    return v


def artifacts(root):
    return {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def protected():
    previous = module('ur2b_prior_bindings', HERE / 'ur2a_handoff_runner.py')
    v = previous.protected(ROOT / '.aegis/results/G5-UR1d-physical-loss-repair-r1/reference-catalog.json')
    for p in (ROOT / '.aegis/packages' / TASK).iterdir():
        if p.is_file():
            v[str(p.relative_to(ROOT))] = sha(p)
    for p in [OUTPUT / 'identity-adapter.mjs', RESULT / 'build-web.py', OUTPUT / 'native-environment.json', OUTPUT / 'web-environment.json',
              RESULT / 'old-oracle-freeze.json', RESULT / 'cost-RED.json']:
        v[str(p.relative_to(ROOT))] = sha(p)
    for variant in ['normal', 'qualification']:
        p = OUTPUT / ('build-web-' + variant) / 'CMakeCache.txt'
        v[str(p.relative_to(ROOT))] = sha(p)
    for p in [OUTPUT / 'oracle-old/exact-oracle', OUTPUT / 'oracle-old/oracle-source.cpp', OUTPUT / 'corpus/requests.json',
              OUTPUT / 'build-native/runtime/render/axiom_ur2b_exact_candidate', OUTPUT / 'build-native/runtime/render/axiom_ur2b_exact_query_tests',
              OUTPUT / 'build-native/runtime/composition/axiom_ur2b_exact_runtime_tests']:
        v[str(p.relative_to(ROOT))] = sha(p)
    return v


def new_ledger():
    return {'schema': 'g5-ur2b-eight-closed-windows-v1', 'planned_ids': list(IDS), 'historical_starts': 64,
            'historical_charged_seconds': 3118, 'original_cap_seconds': 8370, 'attempts': [], 'remaining_status': 'PENDING',
            'cumulative_starts': 64, 'cumulative_charged_seconds': 3118}


def reserve(ledger, id):
    i = len(ledger['attempts'])
    if ledger['remaining_status'] != 'PENDING' or i >= len(IDS) or id != IDS[i] or any(v['status'] == 'STARTED' for v in ledger['attempts']):
        raise ValueError('unplanned/concurrent/repeated worker')
    seconds = 360 if i < 2 else 180
    ledger['attempts'].append({'id': id, 'status': 'STARTED', 'reserved_seconds': seconds, 'charged_seconds': seconds, 'rss_limit_mib': 3072})


def complete(ledger, guard, worker, stable):
    if not ledger['attempts'] or ledger['attempts'][-1]['status'] != 'STARTED':
        raise ValueError('no started worker')
    good = bool(guard and guard.get('reason') == 'completed' and guard.get('cleanup_complete') and stable and worker and worker.get('status') == 'OBSERVED')
    row = ledger['attempts'][-1]
    row.update(status='COMPLETED' if good else 'STOPPED', guard=guard, worker_status=worker.get('status') if worker else 'MISSING',
               identity_stable=stable, charged_seconds=math.ceil(guard['elapsed_s']) if guard and 'elapsed_s' in guard else row['reserved_seconds'])
    ledger.update(cumulative_starts=64 + len(ledger['attempts']), cumulative_charged_seconds=3118 + sum(v['charged_seconds'] for v in ledger['attempts']))
    if not good or ledger['cumulative_charged_seconds'] > 8370:
        ledger['remaining_status'] = 'MISSING'
    elif len(ledger['attempts']) == len(IDS):
        ledger['remaining_status'] = 'NONE'
    return good


def freeze():
    assert not (RESULT / 'local-web/sampling-ledger.json').exists()
    source = identity(True)
    write(RESULT / 'source-final.json', source)
    sdk_roots = [OUTPUT / 'normal-install/js', OUTPUT / 'qualification-sdk', ROOT / 'out/G5-UR2a-mask-isolation-p36-r1/normal-install/js']
    sdks = {str(p): artifacts(p) for p in sdk_roots}
    assert all(len(v) == 36 for v in sdks.values())
    prior = json.loads((ROOT / '.aegis/results/G5-UR2a-mask-isolation-p36-r1/candidate-builds.json').read_text())
    for v in prior['SDK']['normal']['files']:
        assert sha(ROOT / v['path']) == v['sha256']
    write(RESULT / 'worker-freeze.json', {'schema': 'g5-ur2b-workers-freeze-v1', 'source_tree_sha256': source['source_tree_sha256'],
          'protected': protected(), 'SDK': sdks, 'history': HISTORY, 'ids': list(IDS),
          'baseline_buildinfo': 'Actual bed9+accepted09ed dirty source SDK. Source-equivalent publication2a; never restamped.',
          'adapter': 'Identity subprocess only; original input, pacing, capture, oracle, AA and guard bytes unchanged.'})


def execute():
    frozen = json.loads((RESULT / 'worker-freeze.json').read_text())
    directory = RESULT / 'local-web'
    directory.mkdir(exist_ok=True)
    ledger_path = directory / 'sampling-ledger.json'
    assert not ledger_path.exists()
    ledger = new_ledger()
    write(ledger_path, ledger)
    guard = module('ur2b_guard', HERE / 'bounded_worker.py')
    previous = module('ur2b_owned_metadata', HERE / 'ur2a_handoff_runner.py')

    def stable():
        return (identity()['source_tree_sha256'] == frozen['source_tree_sha256'] and protected() == frozen['protected']
                and all(artifacts(Path(p)) == v for p, v in frozen['SDK'].items()))

    for i, id in enumerate(IDS):
        if not stable():
            ledger.update(remaining_status='MISSING', identity_failure=True)
            write(ledger_path, ledger)
            break
        out = directory / id
        assert not out.exists(), 'closed worker output already exists'
        reserve(ledger, id)
        write(ledger_path, ledger)
        if i < 2:
            sdk = OUTPUT / ('qualification-sdk' if i == 0 else 'normal-install/js')
            cmd = ['node', str(OUTPUT / 'identity-adapter.mjs'), str(sdk), str(out), 'qualification' if i == 0 else 'normal',
                   str(ROOT / 'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'),
                   str(ROOT / '.aegis/results/G5-UR1d-physical-loss-repair-r1/reference-catalog.json')]
        else:
            sdk = ROOT / 'out/G5-UR2a-mask-isolation-p36-r1/normal-install/js' if '-A' in id else OUTPUT / 'normal-install/js'
            cmd = ['node', str(HERE / 'ur2b_exact_query.mjs'), str(sdk), str(out)]
        owned = {}
        original = guard.processes

        def observed():
            table = original()
            previous.observe_owned_processes(table, cmd[1], str(out), owned)
            return table

        guard.processes = observed
        g = None
        started = time.monotonic()
        try:
            g = guard.run(cmd, out, 360 if i < 2 else 180, 3072 * 1024)
        except BaseException as error:
            g = {'reason': 'guard-exception', 'cleanup_complete': False, 'elapsed_s': time.monotonic() - started, 'error': repr(error)}
        finally:
            guard.processes = original
            write(directory / (id + '-owned-processes.json'), {'owned_processes': list(owned.values()), 'source': 'unchanged guard process-table observation; identity-labelled family only'})
        wp = out / 'worker-result.json'
        worker = json.loads(wp.read_text()) if wp.is_file() else None
        identity_ok = stable()
        write(directory / (id + '-identity-after.json'), {'stable': identity_ok})
        good = complete(ledger, g, worker, identity_ok)
        write(ledger_path, ledger)
        print(json.dumps(ledger['attempts'][-1]), flush=True)
        if not good:
            break
    write(directory / 'summary.json', {'ledger': ledger, 'unrun_ids': list(IDS[len(ledger['attempts']):]), 'source_tree_sha256': frozen['source_tree_sha256']})


if __name__ == '__main__':
    if sys.argv[1] == '--source-identity':
        print(json.dumps(identity()))
    elif sys.argv[1] == '--freeze':
        freeze()
    elif sys.argv[1] == '--execute':
        execute()
    else:
        raise ValueError('explicit operation required')
