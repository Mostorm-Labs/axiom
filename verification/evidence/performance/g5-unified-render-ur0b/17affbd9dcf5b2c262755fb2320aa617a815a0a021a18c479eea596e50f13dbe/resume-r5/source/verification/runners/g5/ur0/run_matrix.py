"""Frozen serial runner; unsuccessful runs are never replaced by retries."""
import argparse,hashlib,importlib.util,json,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
HERE=Path(__file__).resolve().parent

def plan_rows(matrix):
    return [{'run_id':f"{row['row_id']}-r{repetition}",'row_id':row['row_id'],'repetition':repetition,
             'budget_mib':row['budget_mib'],'case':row['case'],'status':'PLANNED','reason':None}
            for row in matrix['rows'] for repetition in range(1,row['repetitions']+1)]

def stop_tier(rows,budget,reason):
    for row in rows:
        if row['status']=='PLANNED' and row['budget_mib']==budget:row.update(status='MISSING',reason=reason)

def write(path,value):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value,indent=2)+'\n')

def load(path):return json.loads(Path(path).read_text())

def bounded(command,prefix,timeout):
    spec=importlib.util.spec_from_file_location('ur0_bounded_worker',HERE.parent/'bounded_worker.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module.run(command,Path(prefix),timeout,3072*1024)

def worker(sdk,out,mode,branch,toggle,limit):
    prefix=Path(str(out)+'-guard');out=Path(out)
    if out.exists():raise RuntimeError('refuse to replace an existing worker outcome: '+str(out))
    result=bounded(['node',str(HERE/'worker.mjs'),str(Path(sdk).resolve()),str(out.resolve()),mode,branch,toggle],prefix,limit)
    summary=None
    if (out/'worker-result.json').exists():
        subprocess.run(['node',str(HERE/'compile_run.mjs'),str(out)],cwd=ROOT,check=True)
        summary=load(out/'compiled-summary.json')
    hard_timeout=result['reason']=='timeout' or (result['returncode']==124 and (result.get('last_stage') or {}).get('stage')=='phase-timeout')
    return {'guard':result,'hard_timeout':hard_timeout,'summary':summary,'raw_directory':str(out),'guard_record':str(prefix.with_suffix('.worker.json'))}

def overhead(sdk,out,matrix):
    path=out/'overhead-plan.json'
    if path.exists():raise RuntimeError('qualification cohort already exists; preserve it rather than select another success')
    rows=[{'run_id':f'overhead-p{i+1}-{mode}','pair':i+1,'mode':mode,'status':'PLANNED'} for i,order in enumerate(matrix['overhead_qualification']['order']) for mode in order]
    write(path,rows);outcomes=[];blocked=None
    for row in rows:
        if blocked:row.update(status='MISSING',reason=blocked);write(path,rows);continue
        print(json.dumps({'stage':row['run_id']}),flush=True)
        run=worker(sdk,out/'raw-runs'/row['run_id'],'fixture','navigation','OFF' if row['mode']=='A' else 'ON',90)
        row.update(status=run['guard']['reason'].upper(),outcome=run);outcomes.append(row);write(path,rows)
        summary=run['summary']
        if run['hard_timeout'] or run['guard']['reason']=='rss_limit' or not run['guard']['cleanup_complete']:blocked='frozen guard/cleanup stop at '+row['run_id']
        elif not summary or summary['status']!='OBSERVED' or not summary['source_stable'] or not summary['artifacts_stable']:blocked='unqualified execution identity at '+row['run_id']
        elif row['mode']=='B' and (summary['critical_loss'] or summary['identity_errors']):blocked='critical observation/identity failure at '+row['run_id']
    pairs=[]
    for n in range(1,7):
        found={r['mode']:r.get('outcome',{}).get('summary') for r in rows if r['pair']==n}
        pair={}
        for mode,key in [('A','off'),('B','on')]:
            s=found.get(mode)
            if s and s.get('common'):pair[key]={**s['common'],'critical_loss':s['critical_loss']}
        pair['pair']=n
        if found.get('A') and found.get('B'):
            a,b=found['A'],found['B'];pair['same_artifact']=a['artifact_identity']==b['artifact_identity']
            pair['same_document']=a['final_snapshot_sha256']==b['final_snapshot_sha256']
            pair['same_terminal']=a['terminal']['activeContacts']==b['terminal']['activeContacts']==0
            pair['same_product_error_codes']=[e['error'] for e in a['product_errors']]==[e['error'] for e in b['product_errors']]
        pairs.append(pair)
    write(out/'overhead-pairs.json',pairs)
    script="import{qualifyOverhead}from'./verification/runners/g5/ur0/fixture_plan.mjs';import fs from'node:fs';console.log(JSON.stringify(qualifyOverhead(JSON.parse(fs.readFileSync(process.argv[1])))));"
    decision=json.loads(subprocess.check_output(['node','--input-type=module','-e',script,str(out/'overhead-pairs.json')],cwd=ROOT,text=True))
    if any(not p.get(k,False) for p in pairs for k in ['same_artifact','same_document','same_terminal','same_product_error_codes']):decision.update(status='FAILED',boundary_reason='paired artifact/document/terminal/product result differs or is missing')
    decision['blocked_execution']=blocked;decision['pairs_ref']='overhead-pairs.json';decision['automatic_rerun']=False
    write(out/'overhead-qualification.json',decision);return decision

def main():
    p=argparse.ArgumentParser();p.add_argument('--phase',choices=['qualify','baseline'],required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--sdk',type=Path,required=True);p.add_argument('--sdk32',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    package=load(a.package)
    if package['task_id']!='G5-UR0b' or package['status']!='AUTHORIZED_UR0b_OBSERVATION_AND_BASELINE':raise RuntimeError('not the active authorized package')
    for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','evidence_contract_ref','scope_contract_ref']:
        ref=package[key];data=(a.package.parent/ref['path']).read_bytes()
        if hashlib.sha256(data).hexdigest()!=ref['sha256']:raise RuntimeError('changed frozen binding '+key)
    matrix=load(ROOT/'docs/planning/g5-unified-render/ur0a/matrix.json');out=a.output;out.mkdir(parents=True,exist_ok=True)
    ledger_path=out/'matrix-ledger.json';ledger=load(ledger_path) if ledger_path.exists() else plan_rows(matrix);write(ledger_path,ledger)
    if a.phase=='qualify':
        decision=overhead(a.sdk,out,matrix)
        if decision['status']!='QUALIFIED':
            for row in ledger:
                if row['status']=='PLANNED':row.update(status='MISSING',reason='UR0b-OVERHEAD did not qualify; primary sampling not admitted')
            write(ledger_path,ledger);write(out/'execution-stop.json',{'status':'FROZEN_VERIFICATION_FAILURE','blocking_obligation':'UR0b-OVERHEAD','qualification':decision,'primary_timing_adopted':False,'scope':'no policy/threshold change or retry selection'})
            return 2
        write(out/'qualification-progress.json',{'status':'OVERHEAD_QUALIFIED','not_terminal_UR0b':True,'other_required_proofs':'private exports/install, fixture/pixels, real-time, regressions and final source bundle remain separate blocking checks'})
        return 0
    decision=load(out/'overhead-qualification.json')
    if decision['status']!='QUALIFIED':raise RuntimeError('baseline not admitted without same-binary overhead qualification')
    if not a.sdk32:raise RuntimeError('both frozen SDK budget variants required')
    started=time.monotonic()
    for row in ledger:
        if row['status']!='PLANNED':continue
        if time.monotonic()-started>5400:
            for r in ledger:
                if r['status']=='PLANNED':r.update(status='MISSING',reason='frozen main batch deadline')
            break
        tier=row['budget_mib'];sdk=a.sdk32 if tier['navigation']==32 else a.sdk
        legacy=row['case'].startswith('legacy');branch=row['case'] if legacy else row['case'].removeprefix('immediate_')
        print(json.dumps({'stage':row['run_id']}),flush=True)
        run=worker(sdk,out/'raw-runs'/row['run_id'],'legacy' if legacy else 'main',branch,'ON',180)
        row.update(status=run['guard']['reason'].upper(),outcome=run)
        if run['hard_timeout'] or run['guard']['reason']=='rss_limit':stop_tier(ledger,tier,'frozen tier stop at '+row['run_id'])
        write(ledger_path,ledger)
    write(ledger_path,ledger);return 0

if __name__=='__main__':raise SystemExit(main())
