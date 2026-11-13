"""Finite corner correctness browser; exact pre/post source/SDK identity."""
from pathlib import Path
import hashlib,json,subprocess,time,math,importlib.util,sys
ROOT=Path(__file__).resolve().parents[3];RES=ROOT/'.aegis/results/G5-corner-join-repair-r1';OUT=ROOT/'out/G5-corner-join-repair-r1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def freeze(q):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','runtime','sdk','cmake','verification/runners/g5','verification/tests'],text=True,cwd=ROOT).split('\0')
 return{'source':{n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and Path(n).suffix in['.cpp','.hpp','.h','.mjs','.mts','.py','.cmake','.json','.txt']},'SDK':{str(p.relative_to(q['sdk'])):sha(p)for p in sorted(Path(q['sdk']).rglob('*'))if p.is_file()},'request':sha(OUT/'request.json')}
def main():
 q=json.loads((OUT/'request.json').read_text());ledger=json.loads((RES/'sampling-ledger.json').read_text());assert len(ledger['attempts'])<2 and ledger['charged_seconds']+240<=480
 before=freeze(q);write(RES/'browser-before.json',before);a={'id':q['id'],'status':'STARTED','unix':time.time()};ledger['attempts'].append(a);write(RES/'sampling-ledger.json',ledger)
 s=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
 guard=m.run(['node',str(ROOT/'verification/runners/g5/corner_join_web.mjs'),str(OUT/'request.json'),str(OUT/'web'/q['id'])],OUT/'web'/q['id'],240,4096*1024)
 after=freeze(q);write(RES/'browser-after.json',after);a.update(guard=guard,charged_seconds=math.ceil(guard['elapsed_s']),drift=[k for k in before if before[k]!=after[k]],status='COMPLETED'if guard['reason']=='completed'and before==after and guard['cleanup_complete']else'FAILED');ledger['charged_seconds']+=a['charged_seconds'];write(RES/'sampling-ledger.json',ledger);print(json.dumps(a,indent=2));return a['status']!='COMPLETED'
if __name__=='__main__':raise SystemExit(main())
