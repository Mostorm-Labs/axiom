"""Finite 2 Chrome / 1 IAB page, 480s aggregate; all failures consume a slot."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'out/G5-pending-navigation-owner-progress-r1'
RES=ROOT/'.aegis/results/G5-pending-navigation-owner-progress-r1'

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def freeze(q):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','runtime','sdk','verification/runners/g5','verification/tests'],cwd=ROOT,text=True).split('\0')
 return {'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
   'source':{n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and Path(n).suffix in['.cpp','.hpp','.h','.mjs','.mts','.py','.json','.cmake','.txt']},
   'SDK':{lane['name']:{str(p.relative_to(lane['sdk'])):sha(p)for p in sorted(Path(lane['sdk']).rglob('*'))if p.is_file()}for lane in q['lanes']},
   'request':sha(OUT/'requests'/f"{q['id']}.json")}
def run(p):
 q=json.loads(Path(p).read_text());ledger=json.loads((RES/'sampling-ledger.json').read_text());assert len(ledger['Chrome'])<2 and ledger['charged_seconds']+q['seconds']<=480
 before=freeze(q);write(RES/(q['id']+'-before.json'),before);attempt={'id':q['id'],'status':'STARTED','unix':time.time()};ledger['Chrome'].append(attempt);write(RES/'sampling-ledger.json',ledger)
 spec=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');bounded=importlib.util.module_from_spec(spec);spec.loader.exec_module(bounded)
 guard=bounded.run(['node',str(ROOT/'verification/runners/g5/pending_navigation_progress_web.mjs'),str(Path(p).resolve()),str(OUT/'web'/q['id'])],OUT/'web'/q['id'],q['seconds'],3*1024*1024)
 after=freeze(q);write(RES/(q['id']+'-after.json'),after);attempt.update(guard=guard,drift=[k for k in before if before[k]!=after[k]],charged_seconds=math.ceil(guard['elapsed_s']),status='COMPLETED'if guard['reason']=='completed'and guard['cleanup_complete']and before==after else'FAILED');ledger['charged_seconds']+=attempt['charged_seconds'];write(RES/'sampling-ledger.json',ledger);print(json.dumps(attempt,indent=2));return attempt['status']!='COMPLETED'
if __name__=='__main__':raise SystemExit(run(sys.argv[1]))
