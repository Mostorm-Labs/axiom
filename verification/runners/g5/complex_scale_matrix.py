#!/usr/bin/env python3
"""Engineering matrix; every declared tier has an outcome, including blocked ones."""
import argparse,gzip,hashlib,json,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT));sys.path.insert(0,str(Path(__file__).parent))
from bounded_worker import run
from verification.tools.g5_preflight import source_identity

def read(p):return json.loads(p.read_text())
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def stages(p):
 result=[]
 for line in p.read_text(errors='replace').splitlines():
  try:
   j=json.loads(line)
   if isinstance(j,dict) and 'stage' in j:result.append(j)
  except ValueError:pass
 return result

def main():
 a=argparse.ArgumentParser();a.add_argument('binary',type=Path);a.add_argument('sdk',type=Path);a.add_argument('output',type=Path);a.add_argument('--default-sdk',type=Path);a.add_argument('--tiers',default='1000,10000,100000,1000000');a.add_argument('--timeout',type=float,default=180);args=a.parse_args()
 binary=args.binary.resolve();sdk=args.sdk.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
 profile=read(ROOT/'verification/corpus/performance/g5/complex-scale-engineering.json');tiers=[int(x) for x in args.tiers.split(',')]
 if not tiers or any(x not in profile['object_tiers'] for x in tiers):raise ValueError('tiers')
 before=source_identity(ROOT);result={'schema':'g5-complex-scale-matrix-v1','scope':'engineering_only','sourceBefore':before,'profile':profile,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'rows':[]}
 write(out/'matrix.json',result)
 def add(row):
  result['rows'].append(row);write(out/'matrix.json',result);print(json.dumps({k:row.get(k) for k in ['id','scope','classification','reason']}),flush=True)
 fixtures={}
 for axis in ['objects','points']:
  for distribution in profile['object_distributions'] if axis=='objects' else ['sparse']:
   for count in tiers:
    case=f'{axis}-{count}-{distribution}';directory=out/case;directory.mkdir(exist_ok=True)
    w=run([str(binary),'component',axis,str(count),distribution],directory/'component',args.timeout,profile['worker_limits']['native_rss_mib']*1024)
    ss=stages(directory/'component.stdout.log');body=ss[-1] if ss else {}
    ok=w['reason']=='completed' and w['cleanup_complete'] and body.get('status')=='completed' and bool(body.get('checks')) and all(body['checks'].values()) and body.get('objects' if axis=='objects' else 'points')==count
    add({'id':case+'-component','axis':axis,'count':count,'distribution':distribution,'scope':body.get('scope','component'),'classification':'MEASURED' if ok else 'BLOCKED_EVIDENCE','reason':w['reason'] if w['reason']!='completed' else body.get('status'),'worker':w,'observation':body,'stages':ss})
    snapshot=directory/'fixture.axsnap'
    w=run([str(binary),'fixture',axis,str(count),distribution,str(snapshot)],directory/'fixture',args.timeout,profile['worker_limits']['native_rss_mib']*1024)
    ss=stages(directory/'fixture.stdout.log');body=ss[-1] if ss else {}
    valid=w['reason']=='completed' and w['cleanup_complete'] and body.get('status')=='completed' and snapshot.is_file() and body.get('objects' if axis=='objects' else 'points')==count
    if valid:
     body.update({'axis':axis,'count':count,'distribution':distribution,'snapshot_path':str(snapshot),'snapshot_sha256':hashlib.sha256(snapshot.read_bytes()).hexdigest()});write(directory/'fixture.json',body);fixtures[case]=directory/'fixture.json'
    add({'id':case+'-fixture','axis':axis,'count':count,'distribution':distribution,'scope':'semantic-fixture','classification':'MEASURED' if valid else 'BLOCKED_EVIDENCE','reason':w['reason'] if w['reason']!='completed' else body.get('status'),'worker':w,'observation':body,'stages':ss})
 for axis in ['objects','points']:
  for distribution in profile['object_distributions'] if axis=='objects' else ['sparse']:
   for count in tiers:
    case=f'{axis}-{count}-{distribution}';meta=fixtures.get(case)
    for dpr in [1,2] if count==1000 else [2]:
     identity=case+'-sdk-dpr'+str(dpr);directory=out/identity
     if meta is None:
      add({'id':identity,'axis':axis,'count':count,'distribution':distribution,'dpr':dpr,'scope':'root-sdk','classification':'BLOCKED_EVIDENCE','reason':'fixture_unavailable','actual_objects_loaded':None,'points':None,'cache':None});continue
     w=run(['node','verification/runners/g5/complex_scale_browser.mjs',str(sdk),str(directory),'scale',str(meta),str(dpr)],out/(identity+'-worker'),args.timeout,profile['worker_limits']['browser_family_rss_mib']*1024)
     p=directory/'observation.json.gz';body=json.loads(gzip.decompress(p.read_bytes())) if p.is_file() else {}
     ok=w['reason']=='completed' and w['cleanup_complete'] and body.get('status')=='completed' and body.get('sourceStable') and body.get('artifactStable')
     add({'id':identity,'axis':axis,'count':count,'distribution':distribution,'dpr':dpr,'scope':'root-sdk','classification':'MEASURED' if ok else 'BLOCKED_EVIDENCE','reason':w['reason'] if w['reason']!='completed' else body.get('error') or body.get('status'),'worker':w,'actual_objects_loaded':body.get('objects'),'points':body.get('points'),'visible_point_centres':body.get('visible_points'),'runtime_source_points':body.get('canonicalPointsObserved'),'cache':body.get('cache'),'setup':body.get('setup'),'transform':body.get('transform'),'checks':body.get('checks'),'phases':[{'id':p['id'],'summary':p['summary']} for p in body.get('phases',[])]})
 for kind in profile['edit_cases']:
  for side,package in [('diagnostic',sdk)]+([('default',args.default_sdk.resolve())] if args.default_sdk else []):
   identity='edit-'+kind+'-'+side;directory=out/identity;w=run(['node','verification/runners/g5/complex_scale_browser.mjs',str(package),str(directory),kind,'unused','2','default' if side=='default' else 'required'],out/(identity+'-worker'),args.timeout,profile['worker_limits']['browser_family_rss_mib']*1024)
   p=directory/'observation.json.gz';body=json.loads(gzip.decompress(p.read_bytes())) if p.is_file() else {}
   census={}
   for phase in ['before','after']:
    path=directory/(phase+'.axsnap')
    if path.is_file():
     cw=run([str(binary),'inspect',str(path)],directory/(phase+'-inspect'),30,profile['worker_limits']['native_rss_mib']*1024);ss=stages(directory/(phase+'-inspect.stdout.log'))
     if cw['reason']=='completed' and ss:census[phase]=ss[-1]
   after=census.get('after',{});ok=w['reason']=='completed' and w['cleanup_complete'] and body.get('status')=='completed' and body.get('sourceStable') and body.get('artifactStable') and after.get('status')=='completed' and (side=='default' or body.get('canonicalPointsObserved')==after.get('points'))
   add({'id':identity,'kind':kind,'scope':'root-sdk-edit','classification':'MEASURED' if ok else 'BLOCKED_EVIDENCE','reason':w['reason'] if w['reason']!='completed' else body.get('error') or body.get('status'),'worker':w,'objects':body.get('objects'),'canonical_census':census,'dpr':2,'cache':body.get('cache'),'edit':body.get('edit'),'history':{'checks':body.get('history',{}).get('checks'),'undo_steps':len(body.get('history',{}).get('undo',[]))},'checks':body.get('checks'),'phases':[{'id':p['id'],'summary':p['summary']} for p in body.get('phases',[])]})
 result['sourceAfter']=source_identity(ROOT);result['sourceStable']=result['sourceBefore']['tree_sha256']==result['sourceAfter']['tree_sha256'] and result['sourceBefore']['commit']==result['sourceAfter']['commit'];result['classification']='OBSERVED_WITH_BLOCKED_TIERS' if any(r['classification']=='BLOCKED_EVIDENCE' for r in result['rows']) else 'MEASURED';write(out/'matrix.json',result)
 if not result['sourceStable']:raise RuntimeError('source drift')
 print(json.dumps({'stage':'matrix_complete','classification':result['classification'],'rows':len(result['rows'])}),flush=True)
if __name__=='__main__':main()
