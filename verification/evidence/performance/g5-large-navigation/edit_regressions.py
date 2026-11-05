from pathlib import Path
import sys,json,gzip
ROOT=Path.cwd();sys.path.insert(0,str(ROOT/'verification/runners/g5'))
from bounded_worker import run
rows=[]
for kind in ['transform','erase','text','table','resource']:
 for side in ['on','off']:
  identity=kind+'-'+side;out=ROOT/'out/g5-large-edits'/identity
  w=run(['node','verification/runners/g5/complex_scale_browser.mjs',str(ROOT/('out/g5-large-'+side+'/js')),str(out),kind,'unused','2','required' if side=='on' else 'default'],ROOT/('out/g5-large-edits/'+identity+'-worker'),180,3072*1024)
  p=out/'observation.json.gz';b=json.loads(gzip.decompress(p.read_bytes())) if p.exists() else {}
  ok=w['reason']=='completed' and w['cleanup_complete'] and b.get('status')=='completed' and b.get('sourceStable') and b.get('artifactStable')
  rows.append({'id':identity,'kind':kind,'side':side,'classification':'MEASURED' if ok else 'BLOCKED_EVIDENCE','worker':w,'checks':b.get('checks')})
  (ROOT/'out/g5-large-edits/matrix.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps({'id':identity,'classification':rows[-1]['classification']}),flush=True)
