"""Two edge-only jobs; all expected pixels stay in the retained old corpus."""
from pathlib import Path
import hashlib,json,gzip,importlib.util,time
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'out/G5-mixed-reference-qualification-r1';RESULT=ROOT/'.aegis/results/G5-mixed-reference-qualification-r1';PACKAGE=ROOT/'.aegis/packages/G5-mixed-reference-qualification-r1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def run():
 ledgerpath=RESULT/'edge-attempt-ledger.json'
 if ledgerpath.exists():raise ValueError('edge starts recorded; never blindly rerun')
 workload=json.loads((PACKAGE/'workload.json').read_text());design=RESULT/'reference-design.json'
 ledger={'native_edge_starts_max':2,'seconds_each_max':120,'RSS_MiB_max':2048,'design_sha256':sha(design),'browser_starts':0,'full_scene_RGBA_starts':0,'attempts':[]}
 spec=importlib.util.spec_from_file_location('bounded',ROOT/'verification/runners/g5/bounded_worker.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 for row in workload['rows']:
  d=OUT/row['row'];d.mkdir(exist_ok=True);f=row['reference'];snapshot=ROOT/f['input.axsnap.gz']['path'];assert sha(snapshot)==f['input.axsnap.gz']['sha256']
  data=gzip.decompress(snapshot.read_bytes());inputpath=d/'input.axsnap';inputpath.write_bytes(data)
  original=json.loads((ROOT/f['request.json']['path']).read_text());q={k:original[k]for k in ['width','height','logical_width','logical_height','camera']};q['snapshot']=str(inputpath);request=d/'edge-request.json';write(request,q)
  record={'row':row['row'],'status':'STARTED','binary_sha256':sha(OUT/'mixed-reference-edges'),'request_sha256':sha(request),'snapshot_sha256':sha(inputpath),'new_source_only':'no actualpixelpath/oldRGBAdiff in input'};ledger['attempts'].append(record);write(ledgerpath,ledger)
  guard=module.run([str(OUT/'mixed-reference-edges'),str(request),str(d/'material')],d/'edge',120,2048*1024);record.update(status='COMPLETED'if guard['reason']=='completed'and guard['cleanup_complete']else'STOPPED',guard=guard)
  if record['status']=='COMPLETED':
   edge=d/'material.edge';data=edge.read_bytes();z=d/'material.edge.gz';z.write_bytes(gzip.compress(data,mtime=0));record['edge']={'path':str(z.relative_to(ROOT)),'sha256':sha(z),'uncompressed_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)};edge.unlink()
  write(ledgerpath,ledger);print(json.dumps(record),flush=True)
  if record['status']!='COMPLETED':break
if __name__=='__main__':run()
