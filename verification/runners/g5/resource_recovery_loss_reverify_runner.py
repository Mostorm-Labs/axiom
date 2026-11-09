"""Narrow P36: exactly two fresh serial Recovery windows, no other browser run."""
import argparse
import json
import math
from pathlib import Path
import importlib.util

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
TASK='G5-UR1d-physical-loss-repair-r1'
RESULT=ROOT/'.aegis/results'/TASK
BUILD=ROOT/'out'/TASK
IDS=('UR1DLOSSR1-Q1','UR1DLOSSR1-N1')
LIMITS=(240,240)
spec=importlib.util.spec_from_file_location('ur1d_original',HERE/'resource_recovery_runner.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
sha,write=base.sha,base.write

def source_identity(full=False,root=ROOT):
    source=base.module('loss_source',HERE/'ur0/source_identity.py')
    prior=json.loads((ROOT/'.aegis/results/G5-UR1d-resource-recovery/source-identity.json').read_text())
    source.EXCLUDED=tuple(prior['excluded_self_outputs'])+(
        '.aegis/results/'+TASK+'/', 'verification/evidence/performance/g5-ur1d-physical-loss-repair-r1/')
    value=source.identity(root)
    if not full:value.pop('files')
    return value

def protected(catalog):
    value=base.protected(catalog)
    package=ROOT/'.aegis/packages'/TASK
    def check(v):
        if isinstance(v,dict):
            if 'path' in v and 'sha256' in v:
                p=package/v['path'] if (package/v['path']).is_file() else ROOT/v['path']
                if sha(p)!=v['sha256']:raise RuntimeError('repair binding changed: '+str(p))
                value[str(p.relative_to(ROOT))]=v['sha256']
            for x in v.values():check(x)
        elif isinstance(v,list):
            for x in v:check(x)
    for p in sorted(package.iterdir()):
        if p.is_file():
            value[str(p.relative_to(ROOT))]=sha(p)
            if p.suffix=='.json':check(json.loads(p.read_text()))
    return value

def new_ledger():
    return {'schema':'g5-ur1d-loss-two-worker-accounting-v1','planned_ids':list(IDS),
        'historical_starts':58,'historical_charged_seconds':2234,
        'new_nominal_seconds_max':480,'original_cap_seconds':8370,'attempts':[],
        'remaining_status':'PENDING'}

def reserve(ledger,identity):
    i=len(ledger['attempts'])
    if ledger['remaining_status']!='PENDING' or i>=len(IDS) or identity!=IDS[i] or any(x['status']=='STARTED' for x in ledger['attempts']):
        raise ValueError('concurrent, duplicate, closed or unplanned recovery start')
    ledger['attempts'].append({'id':identity,'status':'STARTED','reserved_seconds':LIMITS[i],
        'charged_seconds':LIMITS[i],'rss_limit_mib':3072})

def unsafe(worker):
    return any(c.get('queues',{}).get('status')=='INVALID' or any(
        'lost context did not create presentation receipt' in f for f in c.get('failures',[]))
        for c in (worker or {}).get('cases',[]))

def complete(ledger,guard,worker):
    if not ledger['attempts'] or ledger['attempts'][-1]['status']!='STARTED':raise ValueError('no active row')
    row=ledger['attempts'][-1]
    observed=guard is not None and all(k in guard for k in ('reason','returncode','cleanup_complete','remaining_owned_pids','elapsed_s')) and math.isfinite(guard['elapsed_s']) and guard['elapsed_s']>=0
    ok=observed and guard['reason']=='completed' and guard['returncode']==0 and guard['cleanup_complete'] and not guard['remaining_owned_pids'] and worker is not None and worker['status'] in ('OBSERVED','PRODUCT_FAILURE_OBSERVED') and not worker.get('crash') and not unsafe(worker)
    row.update(status='COMPLETED' if ok else 'STOPPED',guard=guard,
        worker_status=worker['status'] if worker else 'MISSING',
        charged_seconds=math.ceil(guard['elapsed_s']) if observed else row['reserved_seconds'])
    if worker and worker['status']=='PRODUCT_FAILURE_OBSERVED':ledger['functional_failure_observed']=True
    if unsafe(worker):ledger['unsafe_resource_or_receipt_failure']=True
    if not ok:ledger['remaining_status']='MISSING'
    elif len(ledger['attempts'])==len(IDS):ledger['remaining_status']='NONE'
    ledger['cumulative_starts']=58+len(ledger['attempts'])
    ledger['cumulative_charged_seconds']=2234+sum(x['charged_seconds'] for x in ledger['attempts'])
    if ledger['cumulative_charged_seconds']>8370:ledger['remaining_status']='MISSING'

def own_sdks(qualification,normal):
    for p in (qualification,normal):
        if not p.resolve().is_relative_to(BUILD):raise RuntimeError('fresh exact task SDK root required')

def freeze(qualification,normal,catalog):
    own_sdks(qualification,normal)
    if (RESULT/'local-functional/sampling-ledger.json').exists():raise RuntimeError('window already reserved')
    value=source_identity(True);write(RESULT/'source-identity.json',value)
    write(RESULT/'worker-freeze.json',{'source_tree_sha256':value['source_tree_sha256'],
        'protected':protected(catalog),'artifacts':{str(p.resolve()):base.artifacts(p) for p in (qualification,normal)}})

def execute(qualification,normal,oracle,catalog,output):
    own_sdks(qualification,normal)
    if output.resolve()!=(RESULT/'local-functional').resolve():raise RuntimeError('exact new result root required')
    output.mkdir(parents=True,exist_ok=True);ledger_path=output/'sampling-ledger.json'
    if ledger_path.exists():raise RuntimeError('no repeat window')
    frozen=json.loads((RESULT/'worker-freeze.json').read_text());guard=base.module('loss_guard',HERE/'bounded_worker.py');ledger=new_ledger();write(ledger_path,ledger)
    def stable():
        return source_identity()['source_tree_sha256']==frozen['source_tree_sha256'] and protected(catalog)==frozen['protected'] and all(base.artifacts(p)==frozen['artifacts'][str(p.resolve())] for p in (qualification,normal))
    for i,identity in enumerate(IDS):
        try:valid=stable()
        except Exception as e:valid=False;ledger['identity_error']=str(e)
        if not valid:
            ledger['identity_failure']=True;ledger['remaining_status']='MISSING';write(ledger_path,ledger);break
        sdk=(qualification,normal)[i];reserve(ledger,identity);write(ledger_path,ledger)
        command=['node',str(HERE/'resource_recovery_loss_reverify.mjs'),str(sdk.resolve()),str((output/identity).resolve()),'qualification' if i==0 else 'normal',str(oracle.resolve()),str(catalog.resolve())]
        try:observed=guard.run(command,output/identity,LIMITS[i],3072*1024)
        except BaseException:
            complete(ledger,None,None);write(ledger_path,ledger);raise
        p=output/identity/'worker-result.json';worker=json.loads(p.read_text()) if p.exists() else None
        try:valid=stable()
        except Exception as e:valid=False;ledger['identity_error']=str(e)
        write(output/(identity+'-identity-after.json'),{'stable':valid})
        if not valid:worker={**(worker or {}),'status':'HARD_FAILURE','identity_failure':True}
        complete(ledger,observed,worker);write(ledger_path,ledger)
        print(json.dumps({'id':identity,'guard':observed['reason'],'worker':worker['status'] if worker else 'MISSING','charged_seconds':ledger['attempts'][-1]['charged_seconds'],'remaining':ledger['remaining_status']}),flush=True)
        if ledger['remaining_status']=='MISSING':break
    write(output/'summary.json',{'ledger':ledger,'unrun_ids':list(IDS[len(ledger['attempts']):]),'source_tree_sha256':frozen['source_tree_sha256'],'new_charged_seconds':sum(x['charged_seconds'] for x in ledger['attempts'])})

def main():
    p=argparse.ArgumentParser();p.add_argument('--source-identity',action='store_true');p.add_argument('--freeze',action='store_true');p.add_argument('--execute',action='store_true')
    for name in ('qualification-sdk','normal-sdk','oracle','catalog','output'):p.add_argument('--'+name,type=Path)
    a=p.parse_args()
    if a.source_identity:print(json.dumps(source_identity()))
    elif a.freeze and all((a.qualification_sdk,a.normal_sdk,a.catalog)):freeze(a.qualification_sdk,a.normal_sdk,a.catalog)
    elif a.execute and all((a.qualification_sdk,a.normal_sdk,a.oracle,a.catalog,a.output)):execute(a.qualification_sdk,a.normal_sdk,a.oracle,a.catalog,a.output)
    else:p.error('complete explicit operation required')

if __name__=='__main__':main()
