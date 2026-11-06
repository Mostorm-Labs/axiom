# Corroborative oracle-noise diagnosis;never substitutes for the frozen A/B matrix.
from pathlib import Path
import os,json,subprocess
P='g5-retire-aa';sdk='out/g5-material-retained-on/js'
opt={'width':636,'height':923,'points':2048,'lines':5,'shapes':2,'scribble':True,'scribblePhase':.63,'zoom':1.59,'drawLast':True,'uiReads':True}
env={**os.environ,'AXIOM_NAV_SELECTED':'0','AXIOM_NAV_MODES':'wheel','AXIOM_NAV_PAN_ONLY':'1','AXIOM_NAV_QUIESCE':'1','AXIOM_NAV_DPR':'2','AXIOM_NAV_OPTIONS':json.dumps(opt)}
rows=[]
for pair in range(3):
 for side in ['first','second']:
  dest=f'out/{P}-{pair}-{side}'
  with open(dest+'-driver.log','w')as f:r=subprocess.run(['python3.12','verification/runners/g5/bounded_worker.py','--prefix',dest+'-worker','--timeout','180','--rss-mib','3072','--','node','verification/runners/g5/navigation_lifecycle.mjs',sdk,dest,'1000','2'],env=env,stdout=f,stderr=subprocess.STDOUT)
  assert r.returncode==0
  s=json.loads(Path(dest+'/summary.json').read_text());assert s['status']=='MEASURED'and s['sourceStable']and s['artifactStable']
  rows.append({'pair':pair,'side':side,'output':dest,'summary':s['results'][0],'sourceBefore':s['sourceBefore'],'artifacts':s['artifacts']})
  Path('out/'+P+'-runs.json').write_text(json.dumps(rows,indent=2)+'\n');print(pair,side,'MEASURED',flush=True)
comparisons=[]
for pair in range(3):
 a=next(r for r in rows if r['pair']==pair and r['side']=='first');b=next(r for r in rows if r['pair']==pair and r['side']=='second')
 assert a['artifacts']==b['artifacts']
 for p,q in zip(a['summary']['phases'],b['summary']['phases']):
  assert p['id']==q['id']and p['pixels']['sha256']==q['pixels']['sha256']
  s,t=p['summary'],q['summary'];low=lambda x:x['navigation']['quality']['timeWeightedLowResolutionFraction']
  v={'pair':pair,'phase':p['id'],'beforeWaitMs':s['settle_ms'],'afterWaitMs':t['settle_ms'],'beforeGapMs':s['input_display_submission_gap_ms']['max'],'afterGapMs':t['input_display_submission_gap_ms']['max'],'beforeLowDensity':low(s),'afterLowDensity':low(t)}
  v['guards']={'wait':v['afterWaitMs']<=v['beforeWaitMs']+max(16.7,.1*v['beforeWaitMs']),'gap':v['afterGapMs']<=v['beforeGapMs']+max(16.7,.1*v['beforeGapMs']),'clarity':low(t)<=low(s)+.02}
  comparisons.append(v)
Path('out/'+P+'-result.json').write_text(json.dumps({'scope':'Three predeclared A/A pairs with identical baseline artifact and workload;diagnosis only,no replacement of frozenA/B verdict.','comparisons':comparisons,'falseRegressionReports':[c for c in comparisons if not all(c['guards'].values())]},indent=2)+'\n')
print('A/A false regression reports',sum(not all(c['guards'].values())for c in comparisons),flush=True)
