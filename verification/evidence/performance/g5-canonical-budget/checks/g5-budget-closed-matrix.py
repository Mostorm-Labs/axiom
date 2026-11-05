import os,json,subprocess,gzip
from pathlib import Path
options={'width':636,'height':923,'points':2048,'lines':5,'shapes':2,'scribble':True,'scribblePhase':.63,'zoom':1.59,'drawLast':True,'uiReads':True}
profiles={'baseline':('out/g5-gpu-ready-on/js',32,96),'default':('out/g5-budget-closed-on/js',32,96),'expanded':('out/g5-budget-closed-expanded-on/js',64,192)}
results=[]
for dpr in [4,3,2]:
 for profile in ['baseline','default','expanded']:
  sdk,cache,hard=profiles[profile];dest='out/g5-budget-closed-dpr'+str(dpr)+'-'+profile
  env={**os.environ,'AXIOM_NAV_SELECTED':'1','AXIOM_NAV_MODES':'wheel','AXIOM_NAV_PAN_ONLY':'0','AXIOM_NAV_QUIESCE':'1','AXIOM_NAV_DPR':str(dpr),'AXIOM_NAV_OPTIONS':json.dumps(options)}
  command=['python3.12','verification/runners/g5/bounded_worker.py','--prefix',dest+'-worker','--timeout','180','--rss-mib','3072','--','node','verification/runners/g5/navigation_lifecycle.mjs',sdk,dest,'1000','2']
  with open(dest+'-driver.log','w') as log:p=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
  summary=json.loads(Path(dest+'/summary.json').read_text()) if Path(dest+'/summary.json').exists() else {}
  record=json.loads(gzip.decompress(Path(dest+'/wheel.json.gz').read_bytes())) if Path(dest+'/wheel.json.gz').exists() else {}
  rows=record.get('rows',[]);samples=[]
  for phase in record.get('phases',[]):
   timings=[r['value'] for r in rows if r['phase']==phase['id'] and r['kind']=='frame' and r.get('segment')=='settle']
   nav=[r['value'] for r in rows if r['kind']=='navigation' and r['phase']==phase['id']]
   samples.append({'id':phase['id'],'summary':phase['summary'],'checks':phase['raw']['checks'],'pixels':phase['raw'].get('pixels'),
    'maxExactAttemptMs':max([r.get('stages',{}).get('attempt',{}).get('inclusive_ns',0)/1e6 for r in timings],default=None),
    'maxSharedChargeBytes':max([r.get('budget_used_bytes',0) for r in nav],default=0),
    'maxNavChargeBytes':max([r.get('navigation_tile_bytes',0) for r in nav],default=0),
    'attemptOutcomes':{kind:sum(r.get('outcome')==kind for r in timings) for kind in set(r.get('outcome') for r in timings)}})
  row={'dpr':dpr,'profile':profile,'cacheMiB':cache,'sharedHardMiB':hard,'sdk':sdk,'output':dest,'status':summary.get('status'),'workerExit':p.returncode,'phases':samples,'error':record.get('error')}
  results.append(row);Path('out/g5-budget-closed-matrix.json').write_text(json.dumps(results,indent=2)+'\n')
  print(json.dumps({'dpr':dpr,'profile':profile,'status':row['status'],'exit':p.returncode,'phases':[{k:p[k] for k in ['id','maxExactAttemptMs','maxSharedChargeBytes']}|{'wait':p['summary']['settle_ms']} for p in samples],'error':row.get('error')}),flush=True)
  if p.returncode or row['status']!='MEASURED':raise SystemExit(1)
