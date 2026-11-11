from pathlib import Path
import importlib.util
import json
import sys

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('parent', HERE / 'erase_exact_canonical_runner.py')
parent = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parent)
ID = 'G5EC1-READINESS-ALL'


def admit(attempts, charged):
    return charged + 300 <= 1200 and not any(v['id'] == ID or v['status'] == 'STARTED' for v in attempts)


def execute(request_path):
    request = parent.read(request_path)
    continuation = request['id'] == 'G5EC1-READINESS-CONTINUE-R1'
    if request['id'] != ID and not continuation:
        raise ValueError('foreign readiness window')
    seconds = 240 if continuation else 300
    if continuation:
        authorization = parent.ROOT / '.aegis/results/G5-erase-exact-canonical-control-r1/readiness-harness-continuation-r1.json'
        if parent.sha(authorization) != '5f6c01699df327bc4ab03fcdb4fdcfd43eaf5d08ea294792d1b32bbc811dd04b':
            raise ValueError('readiness continuation authority mismatch')
        if request.get('cases') != [dict(kind=k, variant='candidate') for k in ('zoom', 'rectangle', 'sticky', 'table', 'image')]:
            raise ValueError('only exact missing five cases authorized')
    ledger_path = parent.RESULT / 'sampling-ledger.json'
    ledger = parent.read(ledger_path)
    if (not continuation and not admit(ledger['attempts'], ledger['charged_seconds'])) or (continuation and (
            ledger['charged_seconds'] + seconds > 1200 or any(v['id'] == request['id'] or v['status'] == 'STARTED' for v in ledger['attempts']))):
        raise ValueError('repeated or exhausted readiness allowance')
    out = parent.RESULT / 'local-web' / request['id']
    out.mkdir(parents=True, exist_ok=False)
    before = parent.inputs(request)
    parent.write(out / 'freeze.json', {'request': request, 'inputs': before})
    ledger['attempts'].append(dict(id=request['id'], status='STARTED', reserved_seconds=seconds, rss_mib=4096))
    ledger['combined_cap_seconds'] = 1200
    parent.write(ledger_path, ledger)
    spec = importlib.util.spec_from_file_location('guard', HERE / 'bounded_worker.py')
    guard = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guard)
    result = guard.run(['node', str(HERE / 'post_erase_interaction_readiness.mjs'), str(request_path), str(out)], out, seconds, 4096 * 1024)
    after = parent.inputs(request)
    parent.write(out / 'inputs-after.json', after)
    worker = parent.read(out / 'worker-result.json') if (out / 'worker-result.json').exists() else {}
    charge = parent.charge(result, seconds)
    ledger['attempts'][-1].update(status=parent.classify(result, worker, before == after, 5 if continuation else 11),
        guard=result, source_stable=before == after, charged_seconds=charge)
    ledger['charged_seconds'] += charge
    parent.write(ledger_path, ledger)
    print(json.dumps(ledger), flush=True)
    return 0 if ledger['attempts'][-1]['status'] == 'COMPLETED' else 1


if __name__ == '__main__':
    raise SystemExit(execute(Path(sys.argv[1]).resolve()))
