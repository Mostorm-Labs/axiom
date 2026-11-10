"""Four serial, one-use windows. No retries, smoke windows or old-ID reuse."""
from pathlib import Path
import hashlib, importlib.util, json, math, subprocess, sys

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
TASK='G5-UR2c-moving-preview-progress-r1'
RESULT=ROOT/'.aegis/results'/TASK
OUTPUT=ROOT/'out'/TASK
IDS=('UR2CMP1-R1','UR2CMP1-Q1','UR2CMP1-N1','UR2CMP1-N2')
LIMITS=(120,240,360,600)

def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value

def read(path):return json.loads(path.read_text())
def write(path,value):
    path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value,indent=2)+'\n')
def sha(path):
    with Path(path).open('rb')as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def artifacts(root):return {str(p.relative_to(root)):sha(p)for p in sorted(root.rglob('*'))if p.is_file()}
def identity(full=False):
    owner=module('moving_source_identity',HERE/'ur0/source_identity.py')
    old=read(ROOT/'.aegis/results/G5-UR2c-live-reference-alignment-r4/harness-source-final.json')
    owner.EXCLUDED=tuple(old['excluded_self_outputs'])+(f'.aegis/results/{TASK}/',f'verification/evidence/performance/{TASK}/')
    value=owner.identity(ROOT)
    if not full:value.pop('files')
    return value

def new_ledger():return dict(schema='g5-moving-erase-four-window-ledger-v1',planned_ids=list(IDS),max_starts=4,cap_seconds=1320,attempts=[],charged_seconds=0,remaining_status='PENDING',remaining_seconds=1320)
def reserve(ledger,window):
    index=len(ledger['attempts'])
    if ledger['remaining_status']!='PENDING'or index>=4 or window!=IDS[index]or any(x['status']!='COMPLETED'for x in ledger['attempts']):raise ValueError('unplanned/concurrent/repeated window')
    if ledger['charged_seconds']+LIMITS[index]>1320:raise ValueError('browser allowance exhausted')
    ledger['attempts'].append(dict(id=window,status='STARTED',reserved_seconds=LIMITS[index],rss_limit_mib=3072))
def complete(ledger,guard,worker,stable):
    if not ledger['attempts']or ledger['attempts'][-1]['status']!='STARTED':raise ValueError('no started window')
    row=ledger['attempts'][-1];elapsed=guard.get('elapsed_s')if isinstance(guard,dict)else None
    trusted=isinstance(elapsed,(int,float))and not isinstance(elapsed,bool)and math.isfinite(elapsed)and 0<=elapsed<=row['reserved_seconds']+5 and guard.get('reason')in('completed','timeout','rss_limit','nonzero_exit')
    charge=math.ceil(elapsed)if trusted else row['reserved_seconds']
    expected='EXPECTED_RED_OBSERVED'if row['id']==IDS[0]else'OBSERVED'
    good=bool(guard and guard.get('reason')=='completed'and guard.get('cleanup_complete')and stable and worker and worker.get('status')==expected)
    row.update(status='COMPLETED'if good else'STOPPED',guard=guard,identity_stable=stable,worker_status=worker.get('status')if worker else'MISSING',charged_seconds=charge,elapsed_trusted=trusted)
    used=sum(x.get('charged_seconds',0)for x in ledger['attempts']);ledger.update(charged_seconds=used,remaining_seconds=1320-used)
    if not good or used>1320:ledger['remaining_status']='MISSING'
    elif len(ledger['attempts'])==4:ledger['remaining_status']='NONE'
    return good

def protected(baseline):
    contract=read(ROOT/'.aegis/packages'/TASK/'execution-contract.json')['implementation'];allowed=set(contract['allowed_existing_paths'])
    bindings=read(RESULT/'package-preflight.json')['bindings'];values={p:h for p,h in bindings.items()if baseline or p not in allowed}
    if baseline:
        for name in allowed:
            if name in bindings:assert sha(ROOT/name)==bindings[name],name
    for name in contract['allowed_existing_paths']+contract['allowed_new_paths']:
        p=ROOT/name
        if p.is_file():values[name]=sha(p)
    for p in (ROOT/'.aegis/packages'/TASK).rglob('*'):
        if p.is_file():values[str(p.relative_to(ROOT))]=sha(p)
    for p,h in values.items():assert sha(ROOT/p)==h,p
    return values

def execute(window):
    index=IDS.index(window);directory=RESULT/'local-web';directory.mkdir(exist_ok=True);lp=directory/'sampling-ledger.json'
    ledger=read(lp)if lp.exists()else new_ledger();reserve(ledger,window)
    out=directory/window;assert not out.exists()
    baseline=index==0
    if baseline:
        assert read(RESULT/'old-native-red.json')['status']=='EXPECTED_RED_OBSERVED'
        sdk=ROOT/'out/G5-UR2c-transient-display-handoff-r2/normal-install/js';request=dict(id=window,variant='old-normal',sdk=str(sdk),out=str(out),DPR=[4])
    else:
        sdk=OUTPUT/('qualification-sdk'if index==1 else'normal-install/js')
        fixture=read(directory/IDS[0]/'baseline.json');baseline_path=directory/IDS[0]/'baseline.axsnap';assert sha(baseline_path)==fixture['sha256']
        passes=([dict(DPR=4,hold=False,correctness=False),dict(DPR=4,hold=True,correctness=False)]if index==1 else[dict(DPR=d,correctness=p,hold=False)for p in [False,True]for d in [1,2,4]])
        request=dict(id=window,variant='qualification'if index==1 else'normal',sdk=str(sdk),out=str(out),fixture=fixture,baseline=str(baseline_path),passes=passes)
    source=identity(True);write(RESULT/(window+'-source-before.json'),source);bound=protected(baseline);sdk_map=artifacts(sdk);assert len(sdk_map)==36
    rp=OUTPUT/(window+'-request.json');write(rp,request);bound[str(rp.relative_to(ROOT))]=sha(rp)
    freeze=dict(source_tree_sha256=source['source_tree_sha256'],protected=bound,SDK={str(sdk):sdk_map},request=request,phase='old_preproduction'if baseline else'final_candidate')
    write(RESULT/(window+'-freeze.json'),freeze);write(lp,ledger)
    guard=module('moving_guard',HERE/'bounded_worker.py');owner=module('moving_pid_owner',HERE/'ur2a_handoff_runner.py');owned={};original=guard.processes
    cmd=['node',str(HERE/'moving_erase_progress.mjs'),str(rp),str(out)]
    if index==3:cmd=['node',str(HERE/'moving_erase_regression.mjs'),str(sdk),str(out),'normal',str(ROOT/'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'),str(ROOT/'.aegis/results/G5-UR1d-physical-loss-repair-r1/reference-catalog.json'),str(ROOT/'.aegis/results/G5-UR2c-transient-display-handoff-r2/trajectory-plan.json')]
    def observed():
        table=original();owner.observe_owned_processes(table,cmd[1],str(out),owned);return table
    guard.processes=observed;outcome=None
    try:outcome=guard.run(cmd,out,LIMITS[index],3072*1024)
    except BaseException as error:outcome=dict(reason='guard-exception',cleanup_complete=False,error=repr(error))
    finally:
        guard.processes=original;write(directory/(window+'-owned-processes.json'),dict(owned_processes=list(owned.values())))
    wp=out/'worker-result.json';worker=read(wp)if wp.exists()else None
    stable=identity()['source_tree_sha256']==source['source_tree_sha256']and all(sha(ROOT/p)==h for p,h in bound.items())and artifacts(sdk)==sdk_map
    complete(ledger,outcome,worker,stable);write(lp,ledger);write(directory/'summary.json',dict(ledger=ledger,unrun_ids=list(IDS[len(ledger['attempts']):])))
    print(json.dumps(ledger['attempts'][-1]),flush=True)

if __name__=='__main__':
    if sys.argv[1]=='--source-identity':print(json.dumps(identity()))
    elif sys.argv[1]=='--run':execute(sys.argv[2])
    else:raise ValueError('explicit operation required')
