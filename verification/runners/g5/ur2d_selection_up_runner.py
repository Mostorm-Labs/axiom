"""Four serial, one-use local UR2d windows; never consumes UR2c evidence."""
from pathlib import Path
import importlib.util, json, hashlib, math, subprocess, sys

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
RESULT=ROOT/'.aegis/results/G5-UR2d-selection-up-continue-r1'
IDS=('U2D1-A-F','U2D1-B-F','U2D1-COMPARE')

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(data,indent=2)+'\n')

def identity():
    # Actual executable inputs only. Result/server logs are not source inputs.
    names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=ROOT).decode().split('\0')
    files={n:sha(ROOT/n) for n in sorted(set(names)) if n and (ROOT/n).is_file() and
           n.startswith(('runtime/','sdk/','schema/','cmake/','verification/runners/g5/','verification/tests/')) and
           (ROOT/n).suffix in ('.cpp','.hpp','.h','.py','.mjs','.json','.txt','.cmake')}
    return {'files':files,'sha256':hashlib.sha256(json.dumps(files,sort_keys=True).encode()).hexdigest()}

def reserve(ledger,id):
    i=len(ledger['attempts'])
    authorized=ledger.get('compare_continuation',{}).get('status')=='AUTHORIZED_SINGLE_REMAINING_COMPARE'
    closed_bf=authorized and i==2 and id=='U2D1-COMPARE' and [x['id']for x in ledger['attempts']]==list(IDS[:2]) and ledger['attempts'][0]['status']=='COMPLETED' and ledger['attempts'][1]['status']=='STOPPED'
    if i>=3 or id!=IDS[i] or (any(x['status']!='COMPLETED' for x in ledger['attempts'])and not closed_bf):
        raise ValueError('unplanned, repeated or concurrent UR2d window')
    if ledger['charged_seconds']+240>960:
        raise ValueError('UR2d allowance exhausted')
    ledger['attempts'].append({'id':id,'status':'STARTED','reserved_seconds':240,'rss_mib':3072})

def classify(guard,worker,source_stable):
    return ('COMPLETED' if guard and guard.get('reason')=='completed' and
            guard.get('returncode')==0 and guard.get('cleanup_complete') is True and
            guard.get('remaining_owned_pids')==[] and source_stable and
            worker and worker.get('status')=='OBSERVED' else 'STOPPED')

def charge(guard):
    elapsed=guard.get('elapsed_s') if isinstance(guard,dict) else None
    trusted=isinstance(elapsed,(int,float)) and not isinstance(elapsed,bool) and math.isfinite(elapsed) and 0<=elapsed<=245
    return math.ceil(elapsed) if trusted else 240

def sdk_roots(data):
    variants=data.get('variants')
    if data['id']=='U2D1-COMPARE':
        if not isinstance(variants,list) or len(variants)!=2 or {v.get('name') for v in variants}!={'old','candidate'}:
            raise ValueError('COMPARE requires exact old/candidate pair')
        return {v['name']:Path(v['sdk']) for v in variants}
    return {'candidate':Path(data['sdk'])}

def artifact_maps(data):
    return {name:{str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file()}
            for name,root in sdk_roots(data).items()}

def execute(request):
    spec=importlib.util.spec_from_file_location('ur2d_guard',HERE/'bounded_worker.py')
    guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard)
    path=RESULT/'sampling-ledger.json'
    ledger=json.loads(path.read_text()) if path.exists() else {'schema':'g5-ur2d-continuation-ledger-v1','attempts':[],'charged_seconds':40,'cap_seconds':960,'prior_closed':{'id':'U2D1-OLD-F','status':'FAIL_CLOSED_RSS_GUARD','charged_seconds':40,'ledger':'.aegis/results/G5-UR2d-selection-up-r1/sampling-ledger.json'}}
    data=json.loads(request.read_text());reserve(ledger,data['id']);write(path,ledger)
    out=RESULT/'local-web'/data['id'];out.mkdir(parents=True,exist_ok=False)
    before=identity();artifacts=artifact_maps(data)
    write(out/'freeze.json',{'source':before,'artifacts':artifacts,'request':data,'request_sha256':sha(request),'driver_sha256':sha(HERE/'ur2d_selection_up.mjs')})
    command=['node',str(HERE/'ur2d_selection_up.mjs'),str(request),str(out)]
    outcome=guard.run(command,out,240,3072*1024)
    after=identity();stable=after==before and artifacts==artifact_maps(data)
    write(out/'source-after.json',after)
    worker=json.loads((out/'worker-result.json').read_text()) if (out/'worker-result.json').exists() else None
    charged=charge(outcome);status=classify(outcome,worker,stable)
    ledger['attempts'][-1].update(status=status,guard=outcome,worker_status=worker.get('status') if worker else 'MISSING',source_stable=stable,charged_seconds=charged)
    ledger['charged_seconds']+=charged;write(path,ledger)
    print(json.dumps(ledger['attempts'][-1]),flush=True)
    return 0 if status=='COMPLETED' else 1

if __name__=='__main__':
    raise SystemExit(execute(Path(sys.argv[1]).resolve()))
