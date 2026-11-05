#!/usr/bin/env python3
"""Additional total-point document observation; single-stroke limits unchanged."""
import argparse,gzip,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
if not (ROOT/'verification').is_dir():ROOT=Path.cwd()
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'verification/runners/g5'))
from bounded_worker import run
from complex_scale_matrix import read,write,stages
from verification.tools.g5_preflight import source_identity

def main():
 a=argparse.ArgumentParser();a.add_argument('binary',type=Path);a.add_argument('sdk',type=Path);a.add_argument('output',type=Path);args=a.parse_args()
 out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);binary=args.binary.resolve();sdk=args.sdk.resolve()
 profile=read(ROOT/'verification/corpus/performance/g5/document-point-total.json');before=source_identity(ROOT)
 result={'scope':'engineering_document_total_points','profile':profile,'sourceBefore':before,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'rows':[]}
 w=run([str(binary),'fixture','points',str(profile['points']),'sparse',str(out/'fixture.axsnap'),str(profile['points_per_stroke'])],out/'fixture',180,2048*1024)
 ss=stages(out/'fixture.stdout.log');body=ss[-1] if ss else {};valid=w['reason']=='completed' and w['cleanup_complete'] and body.get('status')=='completed' and body.get('points')==profile['points'] and body.get('objects')==profile['objects']
 result['rows'].append({'scope':'semantic-fixture','worker':w,'observation':body,'classification':'MEASURED' if valid else 'BLOCKED_EVIDENCE'})
 if valid:
  body.update({'axis':'points','count':profile['points'],'distribution':'multiple-valid-strokes','snapshot_path':str(out/'fixture.axsnap'),'snapshot_sha256':hashlib.sha256((out/'fixture.axsnap').read_bytes()).hexdigest()});write(out/'fixture.json',body)
  w=run(['node','verification/runners/g5/complex_scale_browser.mjs',str(sdk),str(out/'sdk'),'scale',str(out/'fixture.json'),'2'],out/'browser',180,3072*1024)
  p=out/'sdk/observation.json.gz';b=json.loads(gzip.decompress(p.read_bytes())) if p.exists() else {}
  ok=w['reason']=='completed' and w['cleanup_complete'] and b.get('status')=='completed' and b.get('sourceStable') and b.get('artifactStable')
  result['rows'].append({'scope':'root-sdk','worker':w,'classification':'MEASURED' if ok else 'BLOCKED_EVIDENCE','objects':b.get('objects'),'points':b.get('points'),'visible_point_centres':b.get('visible_points'),'runtime_source_points':b.get('canonicalPointsObserved'),'checks':b.get('checks'),'cache':b.get('cache'),'setup':b.get('setup'),'error':b.get('error'),'phases':[{'id':p['id'],'summary':p['summary']} for p in b.get('phases',[])]})
 else:result['rows'].append({'scope':'root-sdk','classification':'BLOCKED_EVIDENCE','reason':'fixture_unavailable'})
 result['sourceAfter']=source_identity(ROOT);result['sourceStable']=result['sourceBefore']['commit']==result['sourceAfter']['commit'] and result['sourceBefore']['tree_sha256']==result['sourceAfter']['tree_sha256'];result['classification']='MEASURED' if result['sourceStable'] and all(r['classification']=='MEASURED' for r in result['rows']) else 'BLOCKED_EVIDENCE'
 write(out/'result.json',result);print(json.dumps({'scope':result['scope'],'classification':result['classification'],'sourceStable':result['sourceStable']}))
 if not result['sourceStable']:raise RuntimeError('source drift')
if __name__=='__main__':main()
