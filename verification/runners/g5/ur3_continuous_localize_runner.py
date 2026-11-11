"""One control-bound console localization, appended to the existing UR3 ledger."""
from pathlib import Path
import importlib.util
import json
import math
import sys
from ur3_continuous_runner import ROOT, RESULT, sha, read, write, inputs

SUPPLEMENT=ROOT/'.aegis/packages/G5-UR3-continuous-boundaries-r1/targeted-localization-supplement.json'
SUPPLEMENT_SHA='bd7557634b1e59cc4ed8d0403f4790d819467a60be20236406520a8f1ef1f406'
NAME='G5U3C1-LOCALIZE'

def validate_request(req, ledger):
    if sha(SUPPLEMENT)!=SUPPLEMENT_SHA:
        raise ValueError('localization authority changed')
    primary=read(ROOT/'out/G5-UR3-continuous-boundaries-r1/primary-request.json')
    expected={**primary,'id':NAME,'cases':[c for c in primary['cases'] if c['id']=='live-prefix-d1']}
    if req!=expected:
        raise ValueError('localization must retain the original live-d1 waveform and SDK')
    if any(a['id']==NAME or a['status']=='STARTED' for a in ledger['attempts']):
        raise ValueError('repeated/concurrent localization')
    if len([a for a in ledger['attempts'] if a['id']=='G5U3C1-PRIMARY'])!=1:
        raise ValueError('original PRIMARY accounting missing')
    if sha(Path(req['sdk'])/'axiom.wasm')!='4742609124ef9123951f0e654e008ef1cd55dc09d1f6c01acd74cba8acf54f41':
        raise ValueError('localization SDK changed')
    return read(SUPPLEMENT)['window']

def execute(request_path):
    req=read(request_path)
    ledger_path=RESULT/'sampling-ledger.json'
    ledger=read(ledger_path)
    window=validate_request(req,ledger)
    before=inputs(req)
    out=RESULT/'local-web'/NAME
    out.mkdir(parents=True,exist_ok=False)
    write(out/'freeze.json',dict(request=req,request_sha256=sha(request_path),inputs=before,
        supplement=dict(path=str(SUPPLEMENT),sha256=SUPPLEMENT_SHA),prior_ledger=ledger,
        diagnostic_only=True,paired_performance=False))
    ledger['attempts'].append(dict(id=NAME,status='STARTED',limit_seconds=120,rss_mib=4096,
        supplement_sha256=SUPPLEMENT_SHA,diagnostic_only=True))
    write(ledger_path,ledger)
    spec=importlib.util.spec_from_file_location('guard',Path(__file__).with_name('bounded_worker.py'))
    guard_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard_module)
    guard=guard_module.run(['node',str(Path(__file__).with_name('ur3_continuous_localize.mjs')),
        str(request_path),str(out)],out,120,4096*1024)
    after=inputs(req);write(out/'inputs-after.json',after)
    worker=read(out/'worker-result.json') if (out/'worker-result.json').exists() else {}
    elapsed=guard.get('elapsed_s')
    charged=math.ceil(elapsed) if isinstance(elapsed,(int,float)) and math.isfinite(elapsed) and 0<=elapsed<=125 else 120
    completed=guard['reason']=='completed' and guard['cleanup_complete'] and not guard['remaining_owned_pids'] and before==after and worker.get('status')=='OBSERVED' and len(worker.get('rows',[]))==1
    ledger['attempts'][-1].update(status='COMPLETED' if completed else 'STOPPED',guard=guard,
        source_stable=before==after,charged_seconds=charged,worker_status=worker.get('status'))
    ledger['charged_seconds']+=charged
    ledger['supplemental_authorization']=dict(path=str(SUPPLEMENT),sha256=SUPPLEMENT_SHA,cap_seconds=120)
    write(ledger_path,ledger)
    print(json.dumps(ledger),flush=True)
    return 0 if completed else 1

if __name__=='__main__':
    raise SystemExit(execute(Path(sys.argv[1]).resolve()))
