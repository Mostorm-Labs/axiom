"""UR1c2 two-worker window; current accepted dirty source, locked artifacts."""
import argparse,hashlib,importlib.util,json,math,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
TASK='G5-UR1c2-canonical-prepared-body'
IDS=('UR1C2-Q1','UR1C2-N1')
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
shared=module('c2_ledger',HERE/'transient_attribution_runner.py')
reserve,complete,write=shared.reserve,shared.complete,shared.write
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def new_ledger():
    d=shared.new_ledger(IDS,50,1881);d['schema']='g5-ur1c2-two-worker-accounting-v1';return d
def source_identity(full=False,root=ROOT):
    source=module('c2_source',HERE/'ur0/source_identity.py')
    source.EXCLUDED+=('.aegis/results/'+TASK+'/','verification/evidence/performance/g5-ur1c2-canonical-prepared-body/')
    d=source.identity(root)
    if not full:d.pop('files')
    return d
def artifact_identity(root):
    return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file()}
def protected_identity(catalog):
    value={}
    package=ROOT/'.aegis/packages'/TASK
    for p in package.iterdir():
        if p.is_file():value[str(p.relative_to(ROOT))]=sha(p)
    authority=json.loads((package/'authority.lock.json').read_text())
    for name in ['authority_refs','accepted_baseline_refs','existing_verification_helpers_read_only']:
        for row in authority[name]:
            if sha(ROOT/row['path'])!=row['sha256']:raise RuntimeError('protected binding changed: '+row['path'])
            value[row['path']]=row['sha256']
    oracle=ROOT/'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'
    value['oracle']=sha(oracle)
    if value['oracle']!='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105':raise RuntimeError('old oracle changed')
    value['catalog']=sha(catalog)
    old=json.loads((ROOT/'.aegis/results/G5-UR1c1-prepared-body-index/driver-before-workers.json').read_text())
    for path,digest in old['files'].items():
        if path.startswith('/Applications/'):
            value[path]=sha(path)
            if value[path]!=digest:raise RuntimeError('browser changed: '+path)
    playwright=(ROOT/'verification/node_modules/playwright').resolve(strict=True)
    for driver in [playwright,(playwright.parent/'playwright-core').resolve(strict=True)]:
        for path in sorted(driver.rglob('*')):
            if path.is_file():value[str(path)]=sha(path)
    value['node_executable']=sha(Path(shutil.which('node')).resolve(strict=True))
    value['python_executable']=sha(Path(sys.executable).resolve(strict=True))
    for row in json.loads(catalog.read_text())['resources']:
        value[row['path']]=sha(row['path'])
        if value[row['path']]!=row['sha256']:raise RuntimeError('resource changed: '+row['path'])
    return value
def prepare_catalog(output):
    package=json.loads((ROOT/'.aegis/packages'/TASK/'verification.lock.json').read_text())
    old_ref=package['correctness_reference']['saved_reference_catalogs'][0];old=ROOT/old_ref['path']
    if sha(old)!=old_ref['sha256']:raise RuntimeError('bound original catalog changed')
    value=json.loads(old.read_text());value['schema']='ur1c2-exact-input-reference-catalog-v1';value['source_catalog']={'path':str(old),'sha256':sha(old)}
    extra=ROOT/'.aegis/results/G5-UR1c1-web-consumer-reverify-r1';manifest=extra/'payload-manifest.json'
    expected=package['correctness_reference']['additional_saved_outputs'][0]['sha256']
    if sha(manifest)!=expected:raise RuntimeError('bound supplemental payload changed')
    rows={x['path']:x['sha256'] for x in json.loads(manifest.read_text())['files']}
    def bound(path):
        path=Path(path);digest=sha(path)
        if rows[str(path.relative_to(ROOT))]!=digest:raise RuntimeError('saved output changed: '+str(path))
        return {'path':str(path),'sha256':digest}
    for worker in ['UR1C1-R1-Q1','UR1C1-R1-N1']:
        folder=extra/'local-functional'/worker
        for p in sorted(folder.glob('S*-result.json')):
            case=json.loads(p.read_text());bound(p)
            for row in case.get('cold_oracles',[]):
                if row.get('status')!='QUALIFIED' or row.get('reference',{}).get('status')!='REFERENCE_RENDERED':continue
                prefix=folder/(case['id']+'-'+row['stage']);request=Path(str(prefix)+'-reference.request.json');bound(request)
                value['entries'].append({'snapshot':bound(Path(str(prefix)+'.axsnap')),'request':json.loads(request.read_text()),'rgba':bound(Path(str(prefix)+'-reference.rgba')),'edge':bound(Path(str(prefix)+'-reference.edge')),'reference':row['reference']})
    # Independently validate every saved identity before admitting this catalog.
    for e in value['entries']:
        for k in ['snapshot','rgba','edge']:
            if sha(e[k]['path'])!=e[k]['sha256']:raise RuntimeError('saved catalog entry mismatch')
    write(output,value);return value
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
        command=['node',str(HERE/'canonical_prepared_body.mjs'),str(sdk.resolve()),str((output/identity).resolve()),variant,str(oracle.resolve()),str(catalog.resolve())]
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
