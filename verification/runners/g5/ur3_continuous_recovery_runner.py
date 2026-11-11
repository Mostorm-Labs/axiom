"""Control-bound four-case correctness repair; never repeats primary timing."""
from pathlib import Path
import importlib.util
import json
import math
import sys
from ur3_continuous_runner import ROOT, RESULT, sha, read, write, inputs
NAME='G5U3C1-RECOVERY-FINAL'
SUPPLEMENT=ROOT/'.aegis/packages/G5-UR3-continuous-boundaries-r1/webgl-extension-recovery-supplement.json'
SUPPLEMENT_SHA='93d6723a209cc234afff7f2bfd169f5c0475f56b61d4a4ee2b0cccfdae12d6ac'
def validate(req,ledger):
    if sha(SUPPLEMENT)!=SUPPLEMENT_SHA:raise ValueError('correctness repair authority changed')
    original=read(ROOT/'out/G5-UR3-continuous-boundaries-r1/recovery-final-request.json')
    amendment=ROOT/'.aegis/packages/G5-UR3-continuous-boundaries-r1/recovery-final-focused-amendment.json'
    if sha(amendment)!='dcbd39febfe221c115c26cb475c04ba33b381a6926d4301f263752ff6791369d':raise ValueError('focus amendment changed')
    allowed=['navigation-recovery-d4','live-prefix-d2','masked-affine-d4']
    expected={**original,'id':NAME,'cases':[x for x in original['cases']if x['id']in allowed]}
    if req!=expected:raise ValueError('foreign repair case/SDK/request')
    if any(x['id']==NAME or x['status']=='STARTED'for x in ledger['attempts']):raise ValueError('repeated/concurrent repair window')
    if sha(Path(req['sdk'])/'axiom.wasm')!=read(RESULT/'recovery-normal-sdk-manifest.json')['files']['axiom.wasm']:raise ValueError('repair SDK changed')
    return req

def execute(request_path):
    req=read(request_path);lp=RESULT/'sampling-ledger.json';ledger=read(lp);validate(req,ledger);before=inputs(req)
    out=RESULT/'local-web'/NAME;out.mkdir(parents=True,exist_ok=False)
    write(out/'freeze.json',dict(request=req,request_sha256=sha(request_path),inputs=before,supplement_sha256=SUPPLEMENT_SHA,prior_ledger=ledger,correctness_only=True))
    ledger['attempts'].append(dict(id=NAME,status='STARTED',limit_seconds=300,rss_mib=4096,supplement_sha256=SUPPLEMENT_SHA,correctness_only=True));write(lp,ledger)
    s=importlib.util.spec_from_file_location('guard',Path(__file__).with_name('bounded_worker.py'));m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
    guard=m.run(['node',str(Path(__file__).with_name('ur3_continuous_recovery_final.mjs')),str(request_path),str(out )],out,300,4096*1024)
    after=inputs(req);write(out/'inputs-after.json',after);worker=read(out/'worker-result.json')if(out/'worker-result.json').exists()else{}
    elapsed=guard.get('elapsed_s');charged=math.ceil(elapsed)if isinstance(elapsed,(float,int))and math.isfinite(elapsed)and 0<=elapsed<=305 else 300
    ok=guard['reason']=='completed'and guard['cleanup_complete']and not guard['remaining_owned_pids']and before==after and worker.get('status')=='OBSERVED'and len(worker.get('rows',[]))==3
    ledger['attempts'][-1].update(status='COMPLETED'if ok else'STOPPED',guard=guard,charged_seconds=charged,source_stable=before==after,worker_status=worker.get('status'));ledger['charged_seconds']+=charged;write(lp,ledger);print(json.dumps(ledger),flush=True);return 0 if ok else 1
if __name__=='__main__':raise SystemExit(execute(Path(sys.argv[1]).resolve()))
