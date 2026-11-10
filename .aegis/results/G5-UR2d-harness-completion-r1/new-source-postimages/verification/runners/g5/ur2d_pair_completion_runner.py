"""One authorized UR2d verification-completion process; no historic ID reuse."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
RESULT=ROOT/'.aegis/results/G5-UR2d-harness-completion-r1'
WINDOW='U2D1-PAIR-COMPLETE-R1'
PRIOR_SECONDS=105
LIMIT_SECONDS=240
TOTAL_CAP=960
EXPECTED_ORACLE='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105'
ALLOWED_CHANGED={
    'verification/runners/g5/ur2d_selection_up.mjs',
    'verification/tests/g5_ur2d_selection_up.test.mjs',
}

def sha(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):
            digest.update(block)
    return digest.hexdigest()

def read(path):
    return json.loads(Path(path).read_text())

def write(path,value):
    path=Path(path)
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,indent=2)+'\n')

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

def artifacts(root):
    root=Path(root)
    return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file()}

def request_roots(request):
    if request.get('id')!=WINDOW or request.get('repetitions')!=[1,2]:
        raise ValueError('only missing repetitions1/2 on the authorized ID may run')
    variants=request.get('variants')
    if not isinstance(variants,list) or [v.get('name') for v in variants]!=['old','candidate']:
        raise ValueError('ordered old/candidate SDK pair required')
    roots={v['name']:Path(v['sdk']).resolve(strict=True) for v in variants}
    if roots['old']==roots['candidate']:
        raise ValueError('old and candidate SDK identities must be independent')
    if [f.get('name') for f in request.get('fixtures',[])]!=['S0','S2']:
        raise ValueError('exact S0/S2 fixture pair required')
    return roots

def reserve(ledger,window):
    if window!=WINDOW or ledger['attempts'] or ledger.get('charged_seconds')!=PRIOR_SECONDS:
        raise ValueError('unplanned, repeated or concurrent completion process')
    if ledger['charged_seconds']+LIMIT_SECONDS>TOTAL_CAP:
        raise ValueError('original cumulative allowance exhausted')
    ledger['attempts'].append({'id':window,'status':'STARTED','reserved_seconds':LIMIT_SECONDS,'rss_mib':3072,'repetitions':[1,2]})

def charge(guard):
    elapsed=guard.get('elapsed_s') if isinstance(guard,dict) else None
    trusted=isinstance(elapsed,(int,float)) and not isinstance(elapsed,bool) and math.isfinite(elapsed) and 0<=elapsed<=LIMIT_SECONDS+5
    return math.ceil(elapsed) if trusted else LIMIT_SECONDS

def classify(guard,worker,stable):
    if not (guard and guard.get('reason')=='completed' and guard.get('returncode')==0 and
            guard.get('cleanup_complete') is True and guard.get('remaining_owned_pids')==[] and stable and
            worker and worker.get('status')=='OBSERVED'):
        return 'STOPPED'
    runs=worker.get('runs',[])
    if [(r.get('repetition'),r.get('variant')) for r in runs]!=[(1,'old'),(1,'candidate'),(2,'old'),(2,'candidate')]:
        return 'STOPPED'
    for run in runs:
        rows=run.get('rows',[])
        if [row.get('fixture') for row in rows]!=['S0','S2']:
            return 'STOPPED'
        for row in rows:
            if row.get('errors') or row.get('roundtrip',{}).get('status')!='QUALIFIED':
                return 'STOPPED'
    return 'COMPLETED'

def source_identity():
    names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=ROOT).decode().split('\0')
    files={name:sha(ROOT/name) for name in sorted(set(names)) if name and (ROOT/name).is_file() and
           name.startswith(('runtime/','sdk/','schema/','cmake/','verification/runners/g5/','verification/tests/')) and
           (ROOT/name).suffix in ('.cpp','.hpp','.h','.mjs','.py','.json','.cmake','.txt')}
    return {'files':files,'sha256':hashlib.sha256(json.dumps(files,sort_keys=True,separators=(',',':')).encode()).hexdigest()}

def frozen_inputs(request):
    roots=request_roots(request)
    parent=ROOT/'.aegis/results/G5-UR2d-selection-up-continue-r1'
    source=read(parent/'source-manifest.json')
    for name,ref in source['files'].items():
        if name not in ALLOWED_CHANGED and sha(ROOT/name)!=ref['sha256']:
            raise ValueError('frozen source changed: '+name)
    pair_ref=request['pair0']
    if sha(pair_ref['path'])!=pair_ref['sha256']:
        raise ValueError('pair0 raw identity changed')
    freeze=read(parent/'local-web/U2D1-COMPARE/freeze.json')
    if request['fixtures']!=freeze['request']['fixtures']:
        raise ValueError('fixture/request changed from pair0')
    sdk={name:artifacts(root) for name,root in roots.items()}
    if sdk!=freeze['artifacts'] or any(len(files)!=36 for files in sdk.values()):
        raise ValueError('old/candidate SDK changed from pair0')
    for fixture in request['fixtures']:
        if sha(fixture['path'])!=fixture['sha256']:
            raise ValueError('fixture bytes changed')
    oracle=request.get('oracle',{})
    if oracle.get('sha256')!=EXPECTED_ORACLE or sha(oracle['path'])!=EXPECTED_ORACLE:
        raise ValueError('foreign or missing old CPU oracle')
    preserved=read(RESULT/'preserved-evidence-before.json')
    for name,digest in preserved.items():
        if sha(ROOT/name)!=digest:
            raise ValueError('old evidence modified: '+name)
    return {'source':source_identity(),'SDK':sdk,'oracle':oracle,'fixtures':request['fixtures'],
            'old_evidence':preserved,'pair0':pair_ref,'driver_sha256':sha(HERE/'ur2d_selection_up.mjs')}

def execute(request_path):
    request=read(request_path)
    # All admission checks precede recording/spawning any browser process.
    before=frozen_inputs(request)
    offline=read(RESULT/'offline-harness-red-green.json')
    if offline.get('status')!='OFFLINE_RED_GREEN_VERIFIED':
        raise ValueError('offline AA repair proof missing')
    path=RESULT/'sampling-ledger.json'
    ledger=read(path) if path.exists() else {'schema':'g5-ur2d-single-completion-ledger-v1','prior_starts':4,
          'prior_charged_seconds':PRIOR_SECONDS,'charged_seconds':PRIOR_SECONDS,'cap_seconds':TOTAL_CAP,
          'attempts':[],'preserved_ledger':'.aegis/results/G5-UR2d-selection-up-continue-r1/sampling-ledger.json'}
    out=RESULT/'local-web'/WINDOW
    if out.exists():
        raise ValueError('completion output already exists; no retry or ID reuse')
    reserve(ledger,request['id'])
    out.mkdir(parents=True,exist_ok=False)
    write(out/'freeze.json',{'inputs':before,'request':request,'request_sha256':sha(request_path)})
    write(path,ledger)
    command=['node',str(HERE/'ur2d_selection_up.mjs'),str(request_path),str(out)]
    guard=module('ur2d_pair_guard',HERE/'bounded_worker.py')
    try:
        outcome=guard.run(command,out,LIMIT_SECONDS,3072*1024)
    except BaseException as error:
        outcome={'reason':'guard_exception','error':repr(error),'cleanup_complete':False,'remaining_owned_pids':[]}
    worker=read(out/'worker-result.json') if (out/'worker-result.json').is_file() else None
    try:
        after=frozen_inputs(request)
        stable=before==after
    except Exception as error:
        after={'error':repr(error)}
        stable=False
    write(out/'inputs-after.json',after)
    status=classify(outcome,worker,stable)
    charged=charge(outcome)
    ledger['attempts'][0].update(status=status,guard=outcome,worker_status=worker.get('status') if worker else 'MISSING',
            source_artifact_stable=stable,charged_seconds=charged)
    ledger['charged_seconds']+=charged
    write(path,ledger)
    print(json.dumps(ledger['attempts'][0]),flush=True)
    return 0 if status=='COMPLETED' else 1

if __name__=='__main__':
    raise SystemExit(execute(Path(sys.argv[1]).resolve()))
