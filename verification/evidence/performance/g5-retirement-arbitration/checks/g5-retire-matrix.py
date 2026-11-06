import os,json,subprocess,gzip
from pathlib import Path
P='g5-retire'
options={'width':636,'height':923,'points':2048,'lines':5,'shapes':2,'scribble':True,'scribblePhase':.63,'zoom':1.59,'drawLast':True,'uiReads':True}
results=[]
for dpr,mode in [(4,'on'),(3,'on'),(4,'off')]:
 for side in ['before','after']:
  sdk=('out/g5-material-retained-' if side=='before' else 'out/'+P+'-')+mode+'/js'
  dest='out/'+P+'-dpr'+str(dpr)+'-'+mode+'-'+side
  env={**os.environ,'AXIOM_NAV_SELECTED':'1','AXIOM_NAV_MODES':'wheel','AXIOM_NAV_PAN_ONLY':'0','AXIOM_NAV_QUIESCE':'1','AXIOM_NAV_DPR':str(dpr),'AXIOM_NAV_OPTIONS':json.dumps(options)}
  with open(dest+'-driver.log','w') as log:p=subprocess.run(['python3.12','verification/runners/g5/bounded_worker.py','--prefix',dest+'-worker','--timeout','180','--rss-mib','3072','--','node','verification/runners/g5/navigation_lifecycle.mjs',sdk,dest,'1000','2'],env=env,stdout=log,stderr=subprocess.STDOUT)
  s=json.loads(Path(dest+'/summary.json').read_text()) if Path(dest+'/summary.json').exists() else {}
  record=json.loads(gzip.decompress(Path(dest+'/wheel.json.gz').read_bytes())) if Path(dest+'/wheel.json.gz').exists() else {}
  samples=[]
  for phase in record.get('phases',[]):
   ts=[r['value'] for r in record.get('rows',[]) if r['phase']==phase['id'] and r['kind']=='frame' and r.get('segment')=='settle']
   ns=[r['value'] for r in record.get('rows',[]) if r['kind']=='navigation' and r['phase']==phase['id']]
   samples.append({'id':phase['id'],'summary':phase['summary'],'checks':phase['raw']['checks'],'pixels':phase['raw'].get('pixels'),'maxExactAttemptMs':max([r.get('stages',{}).get('attempt',{}).get('inclusive_ns',0)/1e6 for r in ts],default=None),'maxSharedChargeBytes':max([r.get('budget_used_bytes',0) for r in ns],default=0),'maxNavChargeBytes':max([r.get('navigation_tile_bytes',0) for r in ns],default=0)})
  row={'dpr':dpr,'mode':mode,'side':side,'sdk':sdk,'output':dest,'status':s.get('status'),'exit':p.returncode,'phases':samples,'error':record.get('error')}
  results.append(row);Path('out/'+P+'-extra-matrix.json').write_text(json.dumps(results,indent=2)+'\n')
  print(json.dumps({'dpr':dpr,'mode':mode,'side':side,'status':row['status'],'exit':p.returncode,'cold':samples[-1] if samples else None}),flush=True)
  if p.returncode or row['status']!='MEASURED':raise SystemExit(1)
for mode in ['on','off']:
 dest='out/'+P+'-dense-'+mode
 with open(dest+'-driver.log','w') as log:p=subprocess.run(['node','verification/runners/g5/dense_navigation_matrix.mjs','out/g5-material-retained-'+mode+'/js','out/'+P+'-'+mode+'/js',dest,mode],stdout=log,stderr=subprocess.STDOUT)
 print('dense',mode,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
