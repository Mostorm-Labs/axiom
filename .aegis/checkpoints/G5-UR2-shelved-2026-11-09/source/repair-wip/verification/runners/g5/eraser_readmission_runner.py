"""Exactly two candidate functional families; preserves all closed UR1a windows."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
_spec=importlib.util.spec_from_file_location('post_up_shared_runner',Path(__file__).with_name('transient_attribution_runner.py'))
_shared=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_shared)
prior_ledger, reserve, complete, load_module, write=(_shared.new_ledger,_shared.reserve,_shared.complete,_shared.load_module,_shared.write)

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
IDS=('UR2R1-C1','UR2R1-C2')

def new_ledger():
    value=prior_ledger(IDS,44,1433);value['schema']='g5-post-up-functional-accounting-v1';return value

def source_identity(full=False):
    source=load_module('post_up_source',HERE/'ur0/source_identity.py')
    source.EXCLUDED=source.EXCLUDED+('.aegis/results/G5-UR2a-post-up-repair-r1/','verification/evidence/performance/g5-ur2a-post-up-repair-r1/','.aegis/results/G5-UR2a-post-up-handoff/','verification/evidence/performance/g5-ur2a-post-up-handoff/')
    value=source.identity(ROOT)
    if not full:value.pop('files')
    return value

def artifact_identity(sdk):
    return {str(p.relative_to(sdk)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sdk.rglob('*')) if p.is_file()}

def execute(sdk,oracle,output):
    output.mkdir(parents=True,exist_ok=True);ledger_file=output/'sampling-ledger.json'
    if ledger_file.exists():raise RuntimeError('candidate window already reserved; no retry')
    source=source_identity(True);write(output/'source.json',source)
    artifacts=artifact_identity(sdk);write(output/'artifacts.json',artifacts)
    guard=load_module('post_up_guard',HERE/'bounded_worker.py');ledger=new_ledger();write(ledger_file,ledger)
    for identity,fixture in zip(IDS,('S0','S2')):
        if source_identity()['source_tree_sha256']!=source['source_tree_sha256'] or artifact_identity(sdk)!=artifacts:
            ledger['remaining_status']='MISSING';ledger['preflight_error']='source/artifact mismatch';write(ledger_file,ledger);break
        reserve(ledger,identity);write(ledger_file,ledger)
        command=['node',str(HERE/'eraser_readmission.mjs'),str(sdk.resolve()),str((output/identity).resolve()),fixture,str(oracle.resolve())]
        try:observed=guard.run(command,output/identity,180,3072*1024)
        except BaseException:complete(ledger,None,None);write(ledger_file,ledger);raise
        p=output/identity/'worker-result.json';worker=json.loads(p.read_text()) if p.exists() else None
        complete(ledger,observed,worker);write(ledger_file,ledger)
        print(json.dumps({'id':identity,'fixture':fixture,'status':ledger['attempts'][-1]['status'],'worker':worker['status'] if worker else 'MISSING','seconds':ledger['attempts'][-1]['charged_seconds']}),flush=True)
        if ledger['remaining_status']=='MISSING':break
    write(output/'summary.json',{'schema':'g5-post-up-functional-summary-v1','source':source['source_tree_sha256'],'ledger':ledger,
       'rows':[{'id':identity,'status':json.loads((output/identity/'worker-result.json').read_text())['status']} if (output/identity/'worker-result.json').exists() else {'id':identity,'status':'MISSING'} for identity in IDS]})

def main():
    p=argparse.ArgumentParser();p.add_argument('--source-identity',action='store_true');p.add_argument('--execute',action='store_true')
    for name in ('sdk','oracle','output'):p.add_argument('--'+name,type=Path)
    a=p.parse_args()
    if a.source_identity:print(json.dumps(source_identity()))
    elif a.execute and all((a.sdk,a.oracle,a.output)):execute(a.sdk,a.oracle,a.output)
    else:p.error('explicit --execute and artifact/output paths required')

if __name__=='__main__':main()
