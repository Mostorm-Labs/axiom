"""Execute the pinned retained subset; compare immutable bytes independently."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tests/fc08/reference'),str(ROOT/'tools/fc08')]
from run_verification import observe,save,load
from wire import Writer,operation,record

def main():
    p=argparse.ArgumentParser();p.add_argument('--profile',choices=['windows-native','web-wasm','headless'],required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve()
    if out.exists() and any(out.iterdir()):raise ValueError('preserve earlier run')
    out.mkdir(parents=True,exist_ok=True)
    plan=load(ROOT/'.aegis/materialized/FC-08/attempts/v0.2-obs01-regressions/inherited-regression-plan.json')
    folder=ROOT/'out/fc08/v02-on/windows-native/tests/fc08';requests=[];observations={};errors=[]
    for row in plan['goldens']:
        path=ROOT/row['input']['path']
        if hashlib.sha256(path.read_bytes()).hexdigest()!=row['input']['sha256']:raise ValueError('immutable input changed')
        requests.append({'id':row['id'],'path':str(path),'sha256':row['input']['sha256'],'root':row['root'],'program':'headless','mode':'Golden'})
    if a.profile=='web-wasm':
        save(out/'requests.json',requests)
        node='C:/Users/Chado/AppData/Local/Axiom/toolchains/emsdk-6.0.6/node/24.19.0_64bit/node.exe'
        argv=[node,ROOT/'tools/fc08/run_browser.mjs','--repo',ROOT,'--build',ROOT/'out/fc08/v02-on/web-wasm',
            '--requests',out/'requests.json','--output',out/'browser']
        r=subprocess.run(list(map(str,argv)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=300)
        save(out/'invocation.json',{'argv':list(map(str,argv)),'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        if r.returncode:raise RuntimeError('browser inherited run failed')
        for call in requests:observations[call['id']]=load(out/'browser'/(call['id']+'.json'))
        for id in ['legacy-replay','legacy-snapshot1-import','legacy-snapshot2-import','legacy-snapshot3-import']:
            observations[id]=load(out/'browser'/(id+'.json'))
    else:
        for call in requests:
            observations[call['id']]=observe([folder/'axiom_fc08_headless_probe.exe',call['mode'],call['root'],call['path']],out/(call['id']+'.raw.json'))
        for n in [1,2,3]:
            id=f'legacy-snapshot{n}-import'
            observations[id]=observe([folder/'axiom_fc08_headless_probe.exe','LegacyImport','DocumentSnapshot',ROOT/f'tests/fc08/corpus/wire/legacy-snapshot{n}.pb'],out/(id+'.raw.json'))
        observations['legacy-replay']=observe([folder/'axiom_fc08_headless_probe.exe','Replay','Operation',ROOT/'tests/fc08/corpus/wire/legacy-insert.pb'],out/'legacy-replay.raw.json')
        if a.profile=='windows-native':
            argv=['ctest','--test-dir',ROOT/'out/fc08/v02-on/windows-native','-R','^FC08','--output-on-failure']
            r=subprocess.run(list(map(str,argv)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
            save(out/'ctest.json',{'argv':list(map(str,argv)),'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
            if r.returncode:errors.append('retained owner regression failed')
    for row in plan['goldens']:
        o=observations[row['id']]
        if o['ok']!=row['accepted']:errors.append(row['id']+': admission mismatch')
        if row['accepted'] and o['canonical_hex']!=row['canonical_hex']:errors.append(row['id']+': immutable canonical bytes differ')
        if not row['accepted'] and o['category']!=row['category']:errors.append(row['id']+': reject category differs')
    # This expected carrier is independently encoded, including the literal
    # byte order of the observation envelope's fixed numeric IDs.
    carrier=operation({'insert_objects':{'objects':[record(20,'Shape')]}},9000)
    carrier['operation_id']={'value':int(9000).to_bytes(16,'little').hex()}
    carrier['document_id']={'value':int(100).to_bytes(16,'little').hex()}
    expected_record=Writer().encode('Operation',carrier).hex()
    for n in [1,2,3]:
        o=observations[f'legacy-snapshot{n}-import']
        if not o['ok'] or o['state']!=2 or o['generation']!=0 or o['ordinal']!=0:errors.append('legacy explicit import '+str(n))
        if o['records']!=([expected_record] if n==1 else []):errors.append('legacy import record preimage '+str(n))
    o=observations['legacy-replay']
    for k,v in {'History_owner':False,'ok':True,'applied':1,'already_applied':1,'generation':1,'ordinal':1,'ledger_entry_present':True,'records':[expected_record]}.items():
        if o[k]!=v:errors.append('headless real replay '+k)
    save(out/'inherited-regression.json',{'profile':a.profile,'plan_sha256':hashlib.sha256((ROOT/'.aegis/materialized/FC-08/attempts/v0.2-obs01-regressions/inherited-regression-plan.json').read_bytes()).hexdigest(),
        'errors':errors,'observations':observations,'status':'BASELINE_MATCHED' if not errors else 'FROZEN_REGRESSION_FAILURE','product_PASS':False})
    print('BASELINE_MATCHED' if not errors else json.dumps(errors))
    return int(bool(errors))
if __name__=='__main__':sys.exit(main())
