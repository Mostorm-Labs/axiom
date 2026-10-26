"""Fixed FC08 verification-materialization runner; never implements the product.

Exit 0: the requested materialization observations were collected. This does
NOT mean FC08 product positives pass. Consult product_status on every row.
Exit 1: broken fixture/binding/environment/oracle, not a product capability gap.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tests/fc08/reference'),str(ROOT/'tools/fc08')]
from fixtures import budget_recipe
from oracles import evaluate,c01,c02

def sha(data):return hashlib.sha256(data).hexdigest()
def load(path):return json.loads(path.read_text(encoding='utf-8'))
def save(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def checked(ref):
    path=ROOT/ref['path']
    if sha(path.read_bytes())!=ref['sha256']:raise ValueError('artifact hash mismatch: '+str(path))
    return path
def observe(argv,output):
    started=time.time()
    r=subprocess.run(list(map(str,argv)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
    save(output,{'argv':list(map(str,argv)),'started_unix':started,'duration_seconds':time.time()-started,
        'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
    if r.returncode:raise RuntimeError(f'actual test host failed: {argv[0]} exit{r.returncode}')
    line=next((s for s in reversed(r.stdout.splitlines()) if s.startswith('{')),None)
    if line is None:raise RuntimeError('real test host produced no JSON observation')
    return json.loads(line)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--design',type=Path,required=True)
    p.add_argument('--case',required=True,help='all, discover, one logical ID or exact variant ID')
    p.add_argument('--profile',choices=['windows-native','web-wasm','headless'],required=True)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();design=load(args.design);manifest=load(ROOT/design['corpus_manifest'])
    rows=manifest['variants']
    selected=rows if args.case in ['all','discover'] else [r for r in rows if args.case in [r['case_id'],r['variant_id']]]
    if not selected:raise ValueError('empty discovery or unknown case')
    out=args.output.resolve()
    if out.exists() and any(out.iterdir()):raise ValueError('preserve earlier run; choose a fresh output directory')
    out.mkdir(parents=True,exist_ok=True)
    save(out/'discovery.json',{'design_sha256':sha(args.design.read_bytes()),'manifest_sha256':sha((ROOT/design['corpus_manifest']).read_bytes()),
        'profile':args.profile,'variant_ids':[r['variant_id'] for r in selected],'product_PASS':False})
    if args.case=='discover':return 0
    calls=[];prepared=[]
    for index,row in enumerate(selected):
        scenario=load(checked(row['input']));expected=load(checked(row['expected']))
        if not expected['predicates']:raise ValueError('empty oracle: '+row['variant_id'])
        if any(v['op'] not in ['equal','unchanged','quality','c01','c02','forest','capabilities','size'] for v in expected['predicates']):raise ValueError('unbound predicate')
        blob_paths=[]
        cached=None
        for j,wire in enumerate(row['wire']):
            if 'path' in wire:path=checked(wire)
            else:
                if cached is None:cached=budget_recipe(wire['recipe'])[0]
                root,bytes_=next((root,b) for label,root,b in cached if label==wire['label'])
                if root!=wire['root'] or len(bytes_)!=wire['size'] or sha(bytes_)!=wire['sha256']:raise ValueError('recipe identity mismatch')
                # Large deterministic bytes stay in the ignored execution area;
                # checked-in recipe+digest and raw invocation are durable.
                path=ROOT/'out/fc08/runner-inputs'/args.profile/f'{index}-{j}.pb'
                path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(bytes_)
            call_id=f'fixture-{index:03}-{j}'
            blob_paths.append({'id':call_id,'root':wire['root'],'path':str(path),'sha256':wire['sha256'],
                'program':'headless' if args.profile=='headless' else 'baseline','mode':'Ingress'})
        calls.extend(blob_paths);prepared.append((row,scenario,expected,blob_paths))
    if args.profile=='web-wasm':
        request_path=out/'browser-requests.json';save(request_path,calls)
        node=Path('C:/Users/Chado/AppData/Local/Axiom/toolchains/emsdk-6.0.6/node/24.19.0_64bit/node.exe')
        argv=[node,ROOT/'tools/fc08/run_browser.mjs','--repo',ROOT,'--build',ROOT/'out/fc08/v02-on/web-wasm',
            '--requests',request_path,'--output',out/'browser']
        started=time.time();r=subprocess.run(list(map(str,argv)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=1800)
        save(out/'browser-invocation.json',{'argv':list(map(str,argv)),'duration_seconds':time.time()-started,'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        if r.returncode:raise RuntimeError('real browser execution failed; inspect browser-invocation.json')
        actual_discovery=load(out/'browser/discovery.json')
    else:
        folder=ROOT/'out/fc08/v02-on/windows-native/tests/fc08'
        if args.profile=='headless':
            actual_discovery=observe([folder/'axiom_fc08_headless_probe.exe','--discover'],out/'actual-discovery.raw.json')
        else:actual_discovery=observe([folder/'axiom_fc08_baseline_probe.exe','--discover'],out/'actual-discovery.raw.json')
    if args.profile!='headless' and actual_discovery['public_consumer'].get('container_typed_api') is not False:
        raise RuntimeError('baseline changed; missing typed binding cannot be assumed')
    inherited=None
    if any(r['case_id'] in ['FC08-W07','FC08-K01','FC08-P04'] for r in selected):
        argv=[sys.executable,ROOT/'tools/fc08/run_inherited.py','--profile',args.profile,'--output',out/'inherited']
        r=subprocess.run(list(map(str,argv)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=600)
        save(out/'inherited-invocation.json',{'argv':list(map(str,argv)),'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        if r.returncode:raise RuntimeError('retained regression did not qualify; inspect inherited-invocation.json')
        inherited=load(out/'inherited/inherited-regression.json')
    results=[]
    for row,scenario,expected,blob_paths in prepared:
        observations=[]
        for call in blob_paths:
            if args.profile=='web-wasm':obs=load(out/'browser'/(call['id']+'.json'))
            elif args.profile=='headless':obs=observe([folder/'axiom_fc08_headless_probe.exe','Ingress',call['root'],call['path']],out/(call['id']+'.raw.json'))
            else:obs=observe([folder/'axiom_fc08_baseline_probe.exe',call['root'],call['path']],out/(call['id']+'.raw.json'))
            observations.append({'call_id':call['id'],'observation':obs,'input_sha256':call['sha256']})
        source_api='runtime/foundation/include/canvas/runtime/runtime_facade.hpp'
        product_status='CAPABILITY_ABSENT'
        reason='Section/1, operations17/18 and/or accepted typed container ingress are absent in the baseline'
        if row['case_id'] in ['FC08-T07','FC08-H05']:
            reason+='; reachable existing-operation checkpoint bindings are separately exercised by live-hooks, not future F/U admission'
        if row['case_id']=='FC08-H06':
            product_status='NOT_REACHED_BASELINE';reason='No resumable admitted-plan/F-U revalidation boundary; do not implement it inside qualification instrumentation'
        if row['variant_id']=='FC08-G05/quality-at':
            product_status='ORACLE_CONTROL_ONLY';reason='Exact bound predicate sensitivity, not a constructed-pose product positive'
            if evaluate(expected['predicates'],scenario) is not None:raise RuntimeError('quality exact-bound control failed')
        if row['case_id'] in ['FC08-W07','FC08-K01','FC08-P04']:
            if inherited is None or inherited['status']!='BASELINE_MATCHED':raise RuntimeError('required inherited run not bound')
            product_status='INHERITED_BASELINE_MATCHED';reason='Actual pinned inherited codec/import/replay/owner regression; not Section product qualification'
            if args.profile=='headless' and row['case_id']=='FC08-P04' and not row['variant_id'].endswith('touched-codec'):
                product_status='NOT_APPLICABLE_HEADLESS';reason='Retained UI/History/selection realization is bound on real Windows and browser; semantic-only headless deliberately has no History owner'
        if row['case_id']=='FC08-W01':
            product_status='SOURCE_DESCRIPTOR_BINDING_ONLY';reason='Exact source/isolated descriptor preservation, not runtime Section product execution'
        baseline_assertions=[]
        if row['case_id']=='FC08-H04' and row['variant_id'].split('/')[-1] in ['key-32','key-33']:
            accepted=observations[0]['observation'].get('accepted',observations[0]['observation'].get('ok'))
            wanted=row['variant_id'].endswith('key-32')
            baseline_assertions=[{'predicate':'inherited OrderKey SIZE/DOMAIN','matched':accepted==wanted}]
            if accepted!=wanted:raise RuntimeError('inherited OrderKey predicate regression')
        results.append({'case_id':row['case_id'],'variant_id':row['variant_id'],'input_sha256':row['input']['sha256'],
            'expected_sha256':row['expected']['sha256'],'materialization_status':'INPUT_ORACLE_DISCOVERED_AND_BOUND',
            'product_status':product_status,'product_positive_passed':False,'reason':reason,'future_seam':row['future_seam'],
            'actual_absence_binding':source_api if args.profile!='headless' else 'runtime/semantic/include/canvas/semantic/operation.hpp',
            'wire_observations':observations,'baseline_assertions':baseline_assertions})
    save(out/'results.json',{'profile':args.profile,'design_sha256':sha(args.design.read_bytes()),'actual_discovery':actual_discovery,
        'classification':'verification materialization; actual baseline diagnostics do not qualify future product positives',
        'results':results,'product_P32_authorized':False,'Gate_PASS':False})
    print('Materialization observations complete; future product statuses remain explicitly nonpassing')
    return 0

if __name__=='__main__':
    try:sys.exit(main())
    except Exception as error:
        print(type(error).__name__+': '+str(error),file=sys.stderr);sys.exit(1)
