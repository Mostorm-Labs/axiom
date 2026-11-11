"""Control-bound four-case correctness repair; never repeats primary timing."""
from pathlib import Path
import importlib.util
import json
import math
import sys
from ur3_continuous_runner import ROOT, RESULT, sha, read, write, inputs
NAME='G5U3C1-CORRECTNESS-REPAIR'
SUPPLEMENT=ROOT/'.aegis/packages/G5-UR3-continuous-boundaries-r1/correctness-harness-supplement.json'
SUPPLEMENT_SHA='30beb4a7c69890dd698605831b039130398830af7e288a3e76960cf1c25fbab2'
def validate(req,ledger):
    if sha(SUPPLEMENT)!=SUPPLEMENT_SHA:raise ValueError('correctness repair authority changed')
    original=read(ROOT/'out/G5-UR3-continuous-boundaries-r1/correctness-final-request.json')
    allowed=read(SUPPLEMENT)['window']['cases']
    expected={**original,'id':NAME,'cases':[x for x in original['cases']if x['id']in allowed]}
    if req!=expected:raise ValueError('foreign repair case/SDK/request')
    if any(x['id']==NAME or x['status']=='STARTED'for x in ledger['attempts']):raise ValueError('repeated/concurrent repair window')
    if sha(Path(req['sdk'])/'axiom.wasm')!='b69207acfabe5754e2e283ded2abb0fb654d04e0d053f53524c87c6d14de7f63':raise ValueError('repair SDK changed')
    return req

def execute(request_path):
    req=read(request_path);lp=RESULT/'sampling-ledger.json';ledger=read(lp);validate(req,ledger);before=inputs(req)
    out=RESULT/'local-web'/NAME;out.mkdir(parents=True,exist_ok=False)
    write(out/'freeze.json',dict(request=req,request_sha256=sha(request_path),inputs=before,supplement_sha256=SUPPLEMENT_SHA,prior_ledger=ledger,correctness_only=True))
    ledger['attempts'].append(dict(id=NAME,status='STARTED',limit_seconds=420,rss_mib=4096,supplement_sha256=SUPPLEMENT_SHA,correctness_only=True));write(lp,ledger)
    s=importlib.util.spec_from_file_location('guard',Path(__file__).with_name('bounded_worker.py'));m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
    guard=m.run(['node',str(Path(__file__).with_name('ur3_continuous_correctness_completion.mjs')),str(request_path),str(out)],out,420,4096*1024)
    after=inputs(req);write(out/'inputs-after.json',after);worker=read(out/'worker-result.json')if(out/'worker-result.json').exists()else{}
    elapsed=guard.get('elapsed_s');charged=math.ceil(elapsed)if isinstance(elapsed,(float,int))and math.isfinite(elapsed)and 0<=elapsed<=425 else 420
    ok=guard['reason']=='completed'and guard['cleanup_complete']and not guard['remaining_owned_pids']and before==after and worker.get('status')=='OBSERVED'and len(worker.get('rows',[]))==4
    ledger['attempts'][-1].update(status='COMPLETED'if ok else'STOPPED',guard=guard,charged_seconds=charged,source_stable=before==after,worker_status=worker.get('status'));ledger['charged_seconds']+=charged;write(lp,ledger);print(json.dumps(ledger),flush=True);return 0 if ok else 1
if __name__=='__main__':raise SystemExit(execute(Path(sys.argv[1]).resolve()))
