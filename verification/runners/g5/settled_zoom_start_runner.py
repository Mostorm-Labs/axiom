from pathlib import Path
import json,hashlib,math,time,subprocess,importlib.util,sys
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'out/G5-settled-zoom-start-repair-r1';RES=ROOT/'.aegis/results/G5-settled-zoom-start-repair-r1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def freeze(q):
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard','--','runtime','sdk','cmake','verification/runners/g5','verification/tests'],cwd=ROOT,text=True).split('\0')
 return{'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'source':{n:sha(ROOT/n)for n in sorted(set(names))if n and(ROOT/n).is_file()and Path(n).suffix in['.cpp','.hpp','.h','.mjs','.mts','.py','.json','.cmake','.txt']},'SDK':{str(p.relative_to(q['sdk'])):sha(p)for p in sorted(Path(q['sdk']).rglob('*'))if p.is_file()},'request':sha(OUT/'requests'/f"{q['id']}.json")}
def run(p):
 q=json.loads(Path(p).read_text());ledger=json.loads((RES/'sampling-ledger.json').read_text());assert len(ledger['Chrome'])<4 and ledger['charged_seconds']+q['seconds']<=900
 before=freeze(q);write(RES/(q['id']+'-before.json'),before);a={'id':q['id'],'status':'STARTED','unix':time.time()};ledger['Chrome'].append(a);write(RES/'sampling-ledger.json',ledger)
 s=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m);guard=m.run(['node',str(ROOT/'verification/runners/g5/settled_zoom_start_web.mjs'),str(Path(p).resolve()),str(OUT/'web'/q['id'])],OUT/'web'/q['id'],q['seconds'],4096*1024)
 after=freeze(q);write(RES/(q['id']+'-after.json'),after);a.update(guard=guard,drift=[k for k in before if before[k]!=after[k]],charged_seconds=math.ceil(guard['elapsed_s']),status='COMPLETED'if guard['reason']=='completed'and guard['cleanup_complete']and before==after else'FAILED');ledger['charged_seconds']+=a['charged_seconds'];write(RES/'sampling-ledger.json',ledger);print(json.dumps(a,indent=2));return a['status']!='COMPLETED'
if __name__=='__main__':raise SystemExit(run(sys.argv[1]))
