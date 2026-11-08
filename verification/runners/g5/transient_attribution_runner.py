"""Independent five-worker attribution window. No automatic retry or UR0 run."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
IDS=('UR1ATTR-SEED-A0','UR1ATTR-A1','UR1ATTR-B1','UR1ATTR-B2','UR1ATTR-A2')
EXCLUDED=('.aegis/results/G5-UR0b/','verification/evidence/performance/g5-unified-render-ur0b/',
          '.aegis/results/G5-UR1a-transient-reference/','verification/evidence/performance/g5-ur1a-transient-reference/',
          '.aegis/results/G5-UR1a-attribution-r1/','verification/evidence/performance/g5-ur1a-attribution-r1/',
          '.aegis/results/G5-UR1a-attribution-r2/','verification/evidence/performance/g5-ur1a-attribution-r2/')

def load_module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def source_identity(full=False):
    module=load_module('attribution_source',HERE/'ur0/source_identity.py')
    module.EXCLUDED=EXCLUDED
    d=module.identity(ROOT)
    if not full:d.pop('files')
    return d

def new_ledger(ids=IDS,historical_starts=36,historical_seconds=1304):
    return {'schema':'g5-ur1a-attribution-accounting-v1','planned_ids':list(ids),'historical_starts':historical_starts,
            'historical_charged_seconds':historical_seconds,'new_nominal_seconds_max':len(ids)*180,'original_cap_seconds':8370,
            'attempts':[],'remaining_status':'PENDING'}

def reserve(ledger,identity):
    rows=ledger['attempts']
    if (ledger['remaining_status']!='PENDING' or len(rows)>=len(ledger['planned_ids']) or identity!=ledger['planned_ids'][len(rows)]
            or any(r['status']=='STARTED' for r in rows)):
        raise ValueError('closed, concurrent, duplicate or unplanned attempt')
    rows.append({'id':identity,'status':'STARTED','reserved_seconds':180,'charged_seconds':180,'rss_limit_mib':3072})

def complete(ledger,guard,worker):
    if not ledger['attempts'] or ledger['attempts'][-1]['status']!='STARTED':raise ValueError('no active attempt')
    ok=(guard is not None and guard['reason']=='completed' and guard['cleanup_complete'] and worker is not None
        and worker['status'] in ('OBSERVED','PRODUCT_FAILURE_OBSERVED'))
    ledger['attempts'][-1].update(status='COMPLETED' if ok else 'STOPPED',worker_status=worker['status'] if worker else 'MISSING',
                                  charged_seconds=math.ceil(guard['elapsed_s']) if guard else 180,guard=guard)
    if not ok:ledger['remaining_status']='MISSING'
    elif len(ledger['attempts'])==len(ledger['planned_ids']):ledger['remaining_status']='NONE'

def write(p,v):
    p.write_text(json.dumps(v,indent=2)+'\n')

def variant_for(identity):
    return 'B' if identity.split('-')[-1].startswith('B') else 'A'

def execute(a,b,oracle,output,*,ids=IDS,historical_starts=36,historical_seconds=1304,fixtures=None):
    output.mkdir(parents=True,exist_ok=True);ledger_path=output/'sampling-ledger.json'
    if ledger_path.exists():raise RuntimeError('window already reserved; no hidden retry')
    guard=load_module('attribution_guard',HERE/'bounded_worker.py')
    frozen=source_identity(True);write(output/'sampling-source.json',frozen)
    ledger=new_ledger(ids,historical_starts,historical_seconds);write(ledger_path,ledger)
    for identity in ids:
        if source_identity()['source_tree_sha256']!=frozen['source_tree_sha256']:
            ledger['remaining_status']='MISSING';ledger['preflight_failure']='source changed';write(ledger_path,ledger);break
        reserve(ledger,identity);write(ledger_path,ledger)
        variant=variant_for(identity)
        row=output/identity;sdk=b if variant=='B' else a
        command=['node',str(HERE/'transient_attribution.mjs'),variant,str(sdk.resolve()),str(row.resolve()),
                 str((fixtures or output/'fixtures').resolve()),str(oracle.resolve()),'seed' if identity=='UR1ATTR-SEED-A0' else 'measured']
        try:
            observed=guard.run(command,output/identity,180,3072*1024)
        except BaseException:
            complete(ledger,None,None);write(ledger_path,ledger);raise
        p=row/'worker-result.json';worker=json.loads(p.read_text()) if p.exists() else None
        complete(ledger,observed,worker);write(ledger_path,ledger)
        print(json.dumps({'id':identity,'status':ledger['attempts'][-1]['status'],'worker_status':ledger['attempts'][-1]['worker_status'],
                          'guard':observed['reason'],'charged_seconds':ledger['attempts'][-1]['charged_seconds']}),flush=True)
        if ledger['remaining_status']=='MISSING':break
    rows=[]
    for identity in ids:
        p=output/identity/'worker-result.json'
        if not p.exists():rows.append({'id':identity,'status':'MISSING'});continue
        w=json.loads(p.read_text());rows.append({'id':identity,'status':w['status'],'error':w.get('error'),
            'source_stable':w.get('source_stable'),'artifacts_stable':w.get('artifacts_stable'),
            'cases':[{k:c.get(k) for k in ('id','status','metrics','pixels','cold_failure')} for c in w['cases']]})
    summary={'schema':'g5-ur1a-attribution-summary-v1','rows':rows,'new_starts':len(ledger['attempts']),
             'new_charged_seconds':sum(x['charged_seconds'] for x in ledger['attempts']),
             'source_tree_sha256':frozen['source_tree_sha256'],'scope':'two observations per variant; only completed equivalent phases with correct pixels are comparable; no G5/P34 acceptance'}
    write(output/'comparison.json',summary)

def main():
    p=argparse.ArgumentParser();p.add_argument('--source-identity',action='store_true');p.add_argument('--execute',action='store_true')
    for name in ('a-sdk','b-sdk','oracle','output'):p.add_argument('--'+name,type=Path)
    a=p.parse_args()
    if a.source_identity:print(json.dumps(source_identity()))
    elif a.execute and all((a.a_sdk,a.b_sdk,a.oracle,a.output)):execute(a.a_sdk,a.b_sdk,a.oracle,a.output)
    else:p.error('explicit --execute and artifact/output paths required')

if __name__=='__main__':main()
