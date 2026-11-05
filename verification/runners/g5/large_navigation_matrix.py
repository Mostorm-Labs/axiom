#!/usr/bin/env python3
"""Paired public SDK cases for the bounded navigation repair, no private API."""
import argparse,gzip,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT));sys.path.insert(0,str(Path(__file__).parent))
from bounded_worker import run
from complex_scale_matrix import read,write
from verification.tools.g5_preflight import source_identity

def main():
 a=argparse.ArgumentParser();a.add_argument('on',type=Path);a.add_argument('off',type=Path);a.add_argument('output',type=Path);args=a.parse_args()
 out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);before=source_identity(ROOT);result={'sourceBefore':before,'rows':[]}
 cases=[('million',ROOT/'out/g5-step4-point-total/fixture.json',2),('single100k',ROOT/'out/g5-step4-matrix/points-100000-sparse/fixture.json',1),('dense10k',ROOT/'out/g5-step4-matrix/objects-10000-dense/fixture.json',1)]
 for name,meta,repeats in cases:
  fixture=read(meta);assert hashlib.sha256(Path(fixture['snapshot_path']).read_bytes()).hexdigest()==fixture['snapshot_sha256']
  for side,package in [('on',args.on),('off',args.off)]:
   for repeat in range(repeats):
    identity=f'{name}-{side}-{repeat}';directory=out/identity
    w=run(['node','verification/runners/g5/complex_scale_browser.mjs',str(package.resolve()),str(directory),'scale',str(meta),'2','required' if side=='on' else 'default'],out/(identity+'-worker'),180,3072*1024)
    p=directory/'observation.json.gz';body=json.loads(gzip.decompress(p.read_bytes())) if p.exists() else {}
    ok=w['reason']=='completed' and w['cleanup_complete'] and body.get('status')=='completed' and body.get('sourceStable') and body.get('artifactStable')
    result['rows'].append({'id':identity,'case':name,'side':side,'repeat':repeat,'fixture_sha256':fixture['snapshot_sha256'],'worker':w,'classification':'MEASURED' if ok else 'BLOCKED_EVIDENCE','checks':body.get('checks'),'objects':body.get('objects'),'points':body.get('points'),'cache':body.get('cache'),'phases':[{'id':p['id'],'summary':p['summary'],'pixels':p['raw']['pixels']} for p in body.get('phases',[])]})
    write(out/'matrix.json',result);print(json.dumps({'id':identity,'classification':result['rows'][-1]['classification'],'reason':w['reason']}),flush=True)
 result['sourceAfter']=source_identity(ROOT);result['sourceStable']=before['commit']==result['sourceAfter']['commit'] and before['tree_sha256']==result['sourceAfter']['tree_sha256'];write(out/'matrix.json',result)
 if not result['sourceStable']:raise RuntimeError('source drift')
if __name__=='__main__':main()
