"""One once-only normal correctness window; frozen R2 product and R3 reader."""
from pathlib import Path
import hashlib, importlib.util, json, math, subprocess, sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
TASK = 'G5-UR2c-live-reference-alignment-r4'
RESULT = ROOT / '.aegis/results' / TASK
OUTPUT = ROOT / 'out' / TASK
IDS = ('UR2CLP4-N1',)
HISTORY = {'starts': 90, 'charged_seconds': 6251, 'cap_seconds': 8370}
SUBJECT = 'd9bd0a26585a4419ea1a25701f5c42132cbb30e6fb149398a6fe117c1a4154dc'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n')


def artifacts(root):
    return {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob('*')) if p.is_file()}


def identity(full=False):
    owner = module('live_reference_source_identity', HERE / 'ur0/source_identity.py')
    old = ROOT / '.aegis/results/G5-UR2c-web-target-readback-r3/harness-source-final.json'
    owner.EXCLUDED = tuple(json.loads(old.read_text())['excluded_self_outputs']) + (
        f'.aegis/results/{TASK}/', f'verification/evidence/performance/{TASK}/')
    value = owner.identity(ROOT)
    value.update(identity_kind='VERIFICATION_HARNESS_REPOSITORY', compiled_subject_source_tree_sha256=SUBJECT)
    if not full:
        value.pop('files')
    return value


def new_ledger():
    return dict(schema='g5-live-reference-one-window-ledger-v1', planned_ids=list(IDS),
                historical_starts=90, historical_charged_seconds=6251, original_cap_seconds=8370,
                attempts=[], remaining_status='PENDING', cumulative_starts=90,
                cumulative_charged_seconds=6251, remaining_seconds=2119)


def reserve(ledger, window_id):
    if window_id != IDS[0] or ledger['attempts'] or ledger['remaining_status'] != 'PENDING':
        raise ValueError('unplanned/concurrent/repeated window')
    if ledger['cumulative_charged_seconds'] + 600 > HISTORY['cap_seconds']:
        raise ValueError('browser allowance exhausted')
    ledger['attempts'].append(dict(id=window_id, status='STARTED', reserved_seconds=600,
                                   charged_seconds=600, rss_limit_mib=3072))


def complete(ledger, guard, worker, stable):
    if len(ledger['attempts']) != 1 or ledger['attempts'][0]['status'] != 'STARTED':
        raise ValueError('no started window')
    elapsed = guard.get('elapsed_s') if isinstance(guard, dict) else None
    trusted = (isinstance(elapsed, (int, float)) and not isinstance(elapsed, bool)
               and math.isfinite(elapsed) and 0 <= elapsed <= 605
               and guard.get('reason') in ('completed', 'timeout', 'rss_limit', 'nonzero_exit'))
    charged = math.ceil(elapsed) if trusted else 600
    good = bool(guard and guard.get('reason') == 'completed' and guard.get('cleanup_complete')
                and stable and worker and worker.get('status') == 'OBSERVED'
                and len(worker.get('cases', [])) == 16
                and all(row.get('status') == 'COMPLETED' for row in worker['cases']))
    ledger['attempts'][0].update(status='COMPLETED' if good else 'STOPPED', guard=guard,
                                 identity_stable=stable, worker_status=worker.get('status') if worker else 'MISSING',
                                 charged_seconds=charged, elapsed_trusted=trusted)
    ledger.update(cumulative_starts=91, cumulative_charged_seconds=6251 + charged,
                  remaining_seconds=2119 - charged, remaining_status='NONE' if good else 'MISSING')
    return good


def protected():
    package = ROOT / '.aegis/packages' / TASK
    allowed = set(json.loads((package / 'execution-contract.json').read_text())['implementation']['allowed_existing_paths'])
    previous = json.loads((ROOT / '.aegis/results/G5-UR2c-web-target-readback-r3/worker-freeze.json').read_text())
    values = {p: h for p, h in previous['protected'].items() if p not in allowed}
    for p, h in json.loads((RESULT / 'package-preflight.json').read_text())['bindings'].items():
        if p not in allowed:
            values[p] = h
    for row in json.loads((RESULT / 'offline-pairs.json').read_text())['rows']:
        for key in ('before', 'after'):
            values[row[key]['path']] = row[key]['sha256']
    archives = ROOT / '.aegis/results/G5-UR2c-web-target-readback-r3/archives.json'
    for row in json.loads(archives.read_text()).values():
        for key in ('archive', 'members'):
            values[row[key]['path']] = row[key]['sha256']
    for p in allowed | {'verification/runners/g5/live_reference_alignment_runner.py'}:
        values[p] = sha(ROOT / p)
    for name in ['red-result.json', 'old-reference-red.log', 'design-preflight.json', 'other-checks.json', 'offline-pairs.json', 'scope-integrity.json']:
        p = RESULT / name
        values[str(p.relative_to(ROOT))] = sha(p)
    values[str((OUTPUT / 'identity-adapter.mjs').relative_to(ROOT))] = sha(OUTPUT / 'identity-adapter.mjs')
    for p, expected in values.items():
        assert sha(ROOT / p) == expected, p
    return values


def freeze():
    assert not (RESULT / 'local-web/sampling-ledger.json').exists()
    source = identity(True)
    write(RESULT / 'harness-source-final.json', source)
    parent = ROOT / 'out/G5-UR2c-transient-display-handoff-r2'
    sdks = {str(p): artifacts(p) for p in [parent / 'normal-install/js', parent / 'qualification-sdk']}
    assert all(len(v) == 36 for v in sdks.values())
    write(RESULT / 'worker-freeze.json', dict(schema='g5-live-reference-worker-freeze-v1',
          harness_source_tree_sha256=source['source_tree_sha256'], compiled_subject_source_tree_sha256=SUBJECT,
          protected=protected(), SDK=sdks, ids=list(IDS), history=HISTORY, SDK_rebuilt=False, native_rebuilt=False))


def execute():
    frozen = json.loads((RESULT / 'worker-freeze.json').read_text())
    directory = RESULT / 'local-web'
    directory.mkdir(exist_ok=True)
    ledger_path = directory / 'sampling-ledger.json'
    assert not ledger_path.exists()
    ledger = new_ledger()
    write(ledger_path, ledger)
    guard = module('live_reference_guard', HERE / 'bounded_worker.py')
    prior = module('live_reference_process_owner', HERE / 'ur2a_handoff_runner.py')

    def stable():
        return (identity()['source_tree_sha256'] == frozen['harness_source_tree_sha256']
                and all(sha(ROOT / p) == h for p, h in frozen['protected'].items())
                and all(artifacts(Path(p)) == v for p, v in frozen['SDK'].items()))

    if not stable():
        ledger.update(remaining_status='MISSING', identity_failure=True)
        write(ledger_path, ledger)
        return
    out = directory / IDS[0]
    assert not out.exists()
    reserve(ledger, IDS[0])
    write(ledger_path, ledger)
    cmd = ['node', str(OUTPUT / 'identity-adapter.mjs'),
           str(ROOT / 'out/G5-UR2c-transient-display-handoff-r2/normal-install/js'), str(out), 'normal',
           str(ROOT / 'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'),
           str(ROOT / '.aegis/results/G5-UR1d-physical-loss-repair-r1/reference-catalog.json'),
           str(ROOT / '.aegis/results/G5-UR2c-transient-display-handoff-r2/trajectory-plan.json')]
    original = guard.processes
    owned = {}

    def observed():
        table = original()
        prior.observe_owned_processes(table, cmd[1], str(out), owned)
        return table

    guard.processes = observed
    outcome = None
    try:
        outcome = guard.run(cmd, out, 600, 3072 * 1024)
    except BaseException as error:
        outcome = dict(reason='guard-exception', cleanup_complete=False, error=repr(error))
    finally:
        guard.processes = original
        write(directory / (IDS[0] + '-owned-processes.json'), dict(owned_processes=list(owned.values())))
    path = out / 'worker-result.json'
    worker = json.loads(path.read_text()) if path.is_file() else None
    complete(ledger, outcome, worker, stable())
    write(ledger_path, ledger)
    write(directory / 'summary.json', dict(ledger=ledger, unrun_ids=[], harness_source_tree_sha256=frozen['harness_source_tree_sha256']))
    print(json.dumps(ledger['attempts'][0]), flush=True)


if __name__ == '__main__':
    action = sys.argv[1]
    if action == '--source-identity':
        print(json.dumps(identity()))
    elif action == '--freeze':
        freeze()
    elif action == '--execute':
        execute()
    else:
        raise ValueError('explicit operation required')
