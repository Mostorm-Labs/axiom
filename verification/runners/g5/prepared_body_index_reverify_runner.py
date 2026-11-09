"""Verifier-only continuation; never reopen an old window or output directory."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
TASK='G5-UR1c1-web-consumer-reverify-r1'
IDS=('UR1C1-R1-Q1','UR1C1-R1-N1')
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
shared=module('reverify_shared',HERE/'transient_attribution_runner.py')
reserve,complete,write=shared.reserve,shared.complete,shared.write
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def new_ledger():
    value=shared.new_ledger(IDS,48,1770);value['schema']='g5-prepared-body-functional-reverification-v1';return value
def source_identity(full=False,root=ROOT):
    source=module('reverify_source',HERE/'ur0/source_identity.py')
    source.EXCLUDED+=('.aegis/results/'+TASK+'/','verification/evidence/performance/g5-ur1c1-web-consumer-reverify-r1/')
    value=source.identity(root)
    if not full:value.pop('files')
    return value
def artifact_identity(sdk):
    return {str(p.relative_to(sdk)):sha(p) for p in sorted(sdk.rglob('*')) if p.is_file()}
def protected_identity(catalog):
    authority=json.loads((ROOT/'.aegis/packages'/TASK/'authority.lock.json').read_text())
    contract=json.loads((ROOT/'.aegis/packages'/TASK/'verification.lock.json').read_text())['reference_contract']
    value={x['path']:sha(ROOT/x['path']) for x in authority['protected_product_and_native_test_files']}
    for row in authority['protected_product_and_native_test_files']:
        if value[row['path']]!=row['sha256']:raise RuntimeError('protected product mismatch: '+row['path'])
    if sha(ROOT/contract['oracle_path'])!=contract['oracle_sha256']:raise RuntimeError('oracle mismatch')
    value['oracle']=contract['oracle_sha256'];value['reference_catalog']=sha(catalog)
    old=json.loads((ROOT/'.aegis/results/G5-UR1c1-prepared-body-index/driver-before-workers.json').read_text())
    for path,digest in old['files'].items():
        if path.startswith('/Applications/'):
            current=sha(path)
            if current!=digest:raise RuntimeError('browser driver mismatch')
            value[path]=current
    return value
def prepare_catalog(output):
    old=ROOT/'.aegis/results/G5-UR1c1-prepared-body-index'
    lock=json.loads((ROOT/'.aegis/packages'/TASK/'verification.lock.json').read_text())['reference_contract']
    payload={r['path']:r for r in json.loads((old/'payload-manifest.json').read_text())['files']}
    def bound(path):
        path=Path(path);key=str(path.relative_to(ROOT));digest=sha(path)
        if payload[key]['sha256']!=digest:raise RuntimeError('saved reference identity mismatch: '+key)
        return {'path':str(path),'sha256':digest}
    entries=[]
    for folder in lock['saved_reference_roots']:
        worker=ROOT/folder
        for result_path in sorted(worker.glob('S*-result.json')):
            case=json.loads(result_path.read_text());bound(result_path)
            for row in case.get('cold_oracles',[]):
                if row['status']!='QUALIFIED' or row['reference']['status']!='REFERENCE_RENDERED':continue
                prefix=worker/(case['id']+'-'+row['stage'])
                request=Path(str(prefix)+'-reference.request.json');bound(request)
                entries.append({'snapshot':bound(Path(str(prefix)+'.axsnap')),'request':json.loads(request.read_text()),
                    'rgba':bound(Path(str(prefix)+'-reference.rgba')),'edge':bound(Path(str(prefix)+'-reference.edge')),'reference':row['reference']})
    old_source=json.loads((old/'source-identity.json').read_text())['files'];resources=[]
    for name,row in old_source.items():
        if name.startswith('assets/') or name in ['semantic-sdk.lock.json','r1-full-skia-sdk.lock.json']:
            if sha(ROOT/name)!=row['sha256']:raise RuntimeError('resource identity mismatch: '+name)
            resources.append({'path':str(ROOT/name),'sha256':row['sha256']})
    builds=json.loads((old/'candidate-builds.json').read_text())
    for kind in ['normal','qualification']:
        p=Path(builds[kind]['root'])/'axiom.data';digest=sha(p)
        if digest!=builds[kind]['files']['axiom.data']:raise RuntimeError('SDK resource identity mismatch')
        resources.append({'path':str(p),'sha256':digest})
    oracle=ROOT/lock['oracle_path'];assert sha(oracle)==lock['oracle_sha256']
    archived=[r for group in json.loads((old/'archive-manifest.json').read_text())['archives'] for r in group['members'] if r['path']==lock['oracle_path']]
    assert len(archived)==1 and archived[0]['sha256']==lock['oracle_sha256']
    value={'schema':'ur1c1-exact-input-reference-catalog-v1','oracle':{'path':str(oracle),'sha256':lock['oracle_sha256']},'resources':resources,'entries':entries,'original_payload_sha256':sha(old/'payload-manifest.json')}
    write(output,value);return value
def execute(qualification_sdk,normal_sdk,oracle,catalog,output):
    allowed=(ROOT/'.aegis/results'/TASK).resolve()
    if not output.resolve().is_relative_to(allowed):raise RuntimeError('continuation must use its own output directory')
    output.mkdir(parents=True,exist_ok=True);ledger_file=output/'sampling-ledger.json'
    if ledger_file.exists():raise RuntimeError('window already reserved; no retry')
    builds=json.loads((ROOT/'.aegis/results/G5-UR1c1-prepared-body-index/candidate-builds.json').read_text())
    for sdk,kind in [(qualification_sdk,'qualification'),(normal_sdk,'normal')]:
        if sdk.resolve()!=Path(builds[kind]['root']) or artifact_identity(sdk)!=builds[kind]['files']:raise RuntimeError('matching SDK required')
    inputs=protected_identity(catalog);source=source_identity(True);write(output/'source.json',source);write(output/'protected-inputs.json',inputs)
    artifacts={str(s.resolve()):artifact_identity(s) for s in [qualification_sdk,normal_sdk]};write(output/'artifacts.json',artifacts)
    guard=module('reverify_guard',HERE/'bounded_worker.py');ledger=new_ledger();write(ledger_file,ledger)
    for identity,sdk,variant in zip(IDS,[qualification_sdk,normal_sdk],['qualification','normal']):
        try:
            stable=protected_identity(catalog)==inputs and source_identity()['source_tree_sha256']==source['source_tree_sha256']
        except Exception as error:stable=False;ledger['preflight_error']=str(error)
        if not stable:
            ledger['remaining_status']='MISSING';ledger.setdefault('preflight_error','protected/source identity mismatch');write(ledger_file,ledger);break
        reserve(ledger,identity);write(ledger_file,ledger)
        command=['node',str(HERE/'prepared_body_index_reverify.mjs'),str(sdk.resolve()),str((output/identity).resolve()),variant,str(oracle.resolve()),str(catalog.resolve())]
        try:observed=guard.run(command,output/identity,180,3072*1024)
        except BaseException:complete(ledger,None,None);write(ledger_file,ledger);raise
        p=output/identity/'worker-result.json';worker=json.loads(p.read_text()) if p.exists() else None
        try:
            after=protected_identity(catalog);stable=after==inputs and artifact_identity(sdk)==artifacts[str(sdk.resolve())];error=None
        except Exception as failure:after=None;stable=False;error=str(failure)
        write(output/(identity+'-protected-after.json'),{'stable':stable,'inputs':after,'error':error})
        if not stable:worker={**(worker or {}),'status':'HARD_FAILURE','wrapper_identity_failure':True}
        complete(ledger,observed,worker);write(ledger_file,ledger)
        print(json.dumps({'id':identity,'status':ledger['attempts'][-1]['status'],'worker':worker['status'] if worker else 'MISSING','seconds':ledger['attempts'][-1]['charged_seconds']}),flush=True)
        if ledger['remaining_status']=='MISSING':break
    write(output/'summary.json',{'ledger':ledger,'source':source['source_tree_sha256']})
def main():
    p=argparse.ArgumentParser();p.add_argument('--source-identity',action='store_true');p.add_argument('--prepare-reference-catalog',type=Path);p.add_argument('--execute',action='store_true')
    for name in ['qualification-sdk','normal-sdk','oracle','catalog','output']:p.add_argument('--'+name,type=Path)
    a=p.parse_args()
    if a.source_identity:print(json.dumps(source_identity()))
    elif a.prepare_reference_catalog:print(json.dumps({'references':len(prepare_catalog(a.prepare_reference_catalog)['entries'])}))
    elif a.execute and all([a.qualification_sdk,a.normal_sdk,a.oracle,a.catalog,a.output]):execute(a.qualification_sdk,a.normal_sdk,a.oracle,a.catalog,a.output)
    else:p.error('explicit continuation action/paths required')
if __name__=='__main__':main()
