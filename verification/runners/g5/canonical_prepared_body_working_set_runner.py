"""UR1c2 two-worker window; current accepted dirty source, locked artifacts."""
import argparse,hashlib,importlib.util,json,math,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
TASK='G5-UR1c2-working-set-repair-r1'
IDS=('UR1C2R1-Q1','UR1C2R1-N1')
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
shared=module('c2_ledger',HERE/'transient_attribution_runner.py')
reserve,complete,write=shared.reserve,shared.complete,shared.write
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def new_ledger():
    d=shared.new_ledger(IDS,52,2004);d['schema']='g5-ur1c2r1-two-worker-accounting-v1';return d
def source_identity(full=False,root=ROOT):
    source=module('c2_source',HERE/'ur0/source_identity.py')
    source.EXCLUDED+=('.aegis/results/G5-UR1c2-canonical-prepared-body/','verification/evidence/performance/g5-ur1c2-canonical-prepared-body/','.aegis/results/'+TASK+'/','verification/evidence/performance/g5-ur1c2-working-set-repair-r1/')
    d=source.identity(root)
    if not full:d.pop('files')
    return d
def artifact_identity(root):
    return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file()}
original=module('c2_original',HERE/'canonical_prepared_body_runner.py')
def protected_identity(catalog):
    value=original.protected_identity(catalog)
    package=ROOT/'.aegis/packages'/TASK
    def refs(item):
        if isinstance(item,dict):
            if 'path' in item and 'sha256' in item:
                p=ROOT/item['path']
                if sha(p)!=item['sha256']:raise RuntimeError('repair protected binding changed: '+str(p))
                value[item['path']]=item['sha256']
            for v in item.values():refs(v)
        elif isinstance(item,list):
            for v in item:refs(v)
    for p in package.iterdir():
        if p.is_file():value[str(p.relative_to(ROOT))]=sha(p);refs(json.loads(p.read_text()))
    return value
def prepare_catalog(output):
    # Reconstruct only the original bound reference catalog. Every reused
    # snapshot/request/oracle/resource identity is still checked in full.
    return original.prepare_catalog(output)
def execute(qualification,normal,oracle,catalog,output):
    allowed=(ROOT/'.aegis/results'/TASK).resolve()
    if not output.resolve().is_relative_to(allowed):raise RuntimeError('own result directory required')
    output.mkdir(parents=True,exist_ok=True);ledger_path=output/'sampling-ledger.json'
    if ledger_path.exists():raise RuntimeError('window already reserved; no retry')
    frozen=json.loads((allowed/'worker-freeze.json').read_text())
    source=source_identity(True)
    if source['source_tree_sha256']!=frozen['source_tree_sha256']:raise RuntimeError('source not frozen')
    inputs=protected_identity(catalog)
    if inputs!=frozen['protected']:raise RuntimeError('protected inputs not frozen')
    artifacts={str(p.resolve()):artifact_identity(p) for p in [qualification,normal]}
    if artifacts!=frozen['artifacts']:raise RuntimeError('SDK artifacts not frozen')
    write(output/'source.json',source);write(output/'protected-inputs.json',inputs)
    guard=module('c2_guard',HERE/'bounded_worker.py');ledger=new_ledger();write(ledger_path,ledger)
    for identity,sdk,variant in zip(IDS,[qualification,normal],['qualification','normal']):
        try:stable=protected_identity(catalog)==inputs and source_identity()['source_tree_sha256']==source['source_tree_sha256'] and artifact_identity(sdk)==artifacts[str(sdk.resolve())]
        except Exception as e:stable=False;ledger['preflight_error']=str(e)
        if not stable:ledger['remaining_status']='MISSING';ledger.setdefault('preflight_error','frozen identity mismatch');write(ledger_path,ledger);break
        reserve(ledger,identity);write(ledger_path,ledger)
        command=['node',str(HERE/'canonical_prepared_body_working_set.mjs'),str(sdk.resolve()),str((output/identity).resolve()),variant,str(oracle.resolve()),str(catalog.resolve())]
        try:observed=guard.run(command,output/identity,180,3072*1024)
        except BaseException:complete(ledger,None,None);write(ledger_path,ledger);raise
        p=output/identity/'worker-result.json';worker=json.loads(p.read_text()) if p.exists() else None
        try:stable=protected_identity(catalog)==inputs and source_identity()['source_tree_sha256']==source['source_tree_sha256'] and artifact_identity(sdk)==artifacts[str(sdk.resolve())];error=None
        except Exception as e:stable=False;error=str(e)
        write(output/(identity+'-identity-after.json'),{'stable':stable,'error':error})
        if not stable:worker={**(worker or {}),'status':'HARD_FAILURE','wrapper_identity_failure':True}
        complete(ledger,observed,worker);write(ledger_path,ledger)
        print(json.dumps({'id':identity,'guard':observed['reason'],'status':ledger['attempts'][-1]['status'],'worker':worker['status'] if worker else 'MISSING','charged_seconds':ledger['attempts'][-1]['charged_seconds']}),flush=True)
        if ledger['remaining_status']=='MISSING':break
    write(output/'summary.json',{'ledger':ledger,'source':source['source_tree_sha256'],'new_starts':len(ledger['attempts']),'new_charged_seconds':sum(x['charged_seconds'] for x in ledger['attempts'])})
def main():
    p=argparse.ArgumentParser();p.add_argument('--source-identity',action='store_true');p.add_argument('--prepare-reference-catalog',type=Path);p.add_argument('--execute',action='store_true')
    for name in ['qualification-sdk','normal-sdk','oracle','catalog','output']:p.add_argument('--'+name,type=Path)
    a=p.parse_args()
    if a.source_identity:print(json.dumps(source_identity()))
    elif a.prepare_reference_catalog:prepare_catalog(a.prepare_reference_catalog)
    elif a.execute and all([a.qualification_sdk,a.normal_sdk,a.oracle,a.catalog,a.output]):execute(a.qualification_sdk,a.normal_sdk,a.oracle,a.catalog,a.output)
    else:p.error('explicit operation and complete artifact paths required')
if __name__=='__main__':main()
