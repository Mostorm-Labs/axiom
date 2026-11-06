import hashlib,json,gzip,shutil,subprocess,re
from pathlib import Path
D=Path('verification/evidence/performance/g5-retirement-arbitration');P='g5-retire'
REV='3fcf642bbfdda126246e29dd1b32ec7710154f62';REV=subprocess.check_output(['git','rev-parse',REV],text=True).strip();BASE='5672f6aea1007f5bc654038353a6f5f36851a6e4'
def read(p):return json.loads(Path(p).read_text())
def write(n,v):q=D/n;q.parent.mkdir(parents=True,exist_ok=True);q.write_text(json.dumps(v,indent=2)+'\n')
def copy(src,dest):
 src=Path(src);dst=D/dest;dst.parent.mkdir(parents=True,exist_ok=True)
 if src.suffix=='.log' or src.name.endswith('CMakeCache.txt') or (src.suffix=='.json' and src.stat().st_size>131072):dst=dst.with_suffix(dst.suffix+'.gz');dst.write_bytes(gzip.compress(src.read_bytes(),mtime=0))
 else:shutil.copyfile(src,dst)
def tree(src,dest):
 for f in Path(src).rglob('*'):
  if f.is_file():copy(f,Path(dest)/f.relative_to(src))
D.mkdir(parents=True,exist_ok=True);runs=[]
for row in read('out/'+P+'-extra-matrix.json'):
 runs.append({'id':'dpr'+str(row['dpr'])+'-'+row['mode']+'-'+row['side'],'mode':row['mode'],'dpr':row['dpr'],'case':'selected-zoom','side':row['side'],'path':row['output']})
for mode in ['on','off']:
 for row in read('out/'+P+'-dense-'+mode+'/matrix.json')['results']:
  runs.append({'id':'dpr2-'+mode+'-'+row['case']+'-'+row['side'],'mode':mode,'dpr':2,'case':row['case'],'side':row['side'],'path':'out/'+P+'-dense-'+mode+'/'+row['case']+'-'+row['side']})
correct=True;identities={}
for run in runs:
 root=Path(run['path']);s=read(root/'summary.json');r=s['results'][0];w=read(str(root)+'-worker.worker.json');expected=BASE if run['side']=='before' else REV
 run['checks']={'measured':s['status']=='MEASURED','sourceStable':s['sourceStable'],'artifactStable':s['artifactStable'],'cleanSource':not s['sourceBefore']['dirty'] and s['sourceBefore']['commit']==REV,'buildRevision':r['setup']['build']['sourceRevision']==expected and not r['setup']['build']['dirty'],'worker':w.get('returncode')==0 and w.get('reason')=='completed' and w.get('cleanup_complete') and not w.get('remaining_owned_pids'),'recordChecks':all(r['checks'].values()),'quiescence':r['setupQuiescence']['stopped']}
 # Worker schema binds cleanup and process status separately from measured data.
 run['worker']=w;run['summaryStatus']=s['status'];run['recordChecks']=r['checks'];run['phases']=r['phases'];run['build']=r['setup']['build'];run['initialPixels']=r['setup']['initialPixels']
 run['checks']['physicalSize']=r['setup']['initialPixels']['width']==r['options']['width']*run['dpr'] and r['setup']['initialPixels']['height']==r['options']['height']*run['dpr']
 correct &= all(run['checks'].values())
 identities[run['id']]={'sourceBefore':s['sourceBefore'],'sourceAfter':s['sourceAfter'],'artifacts':s['artifacts']}
 tree(root,Path('runs')/run['id'])
 for suffix in ['-worker.worker.json','-worker.stdout.log','-worker.stderr.log','-driver.log']:
  f=Path(str(root)+suffix)
  if f.exists():copy(f,Path('workers')/(run['id']+suffix))
comparisons=[]
for a in runs:
 if a['side']!='before':continue
 b=next(r for r in runs if r['case']==a['case'] and r['mode']==a['mode'] and r['dpr']==a['dpr'] and r['side']=='after')
 ps=[]
 for p,q in zip(a['phases'],b['phases']):
  quality=lambda x:((x['summary'].get('navigation') or {}).get('quality') or {}).get('timeWeightedLowResolutionFraction')
  same=p['id']==q['id'] and p['pixels']['sha256']==q['pixels']['sha256'];correct &= same
  ps.append({'phase':p['id'],'sameFinalPixels':same,'beforeWaitMs':p['summary']['settle_ms'],'afterWaitMs':q['summary']['settle_ms'],'beforeGapMs':p['summary']['input_display_submission_gap_ms']['max'],'afterGapMs':q['summary']['input_display_submission_gap_ms']['max'],'beforeLowDensity':quality(p),'afterLowDensity':quality(q)})
 comparisons.append({'case':a['case'],'dpr':a['dpr'],'mode':a['mode'],'phases':ps})
# Diagnostics ON/OFF use identical resources and deterministic final pixels.
offPairs=[]
for a in runs:
 if a['mode']!='on':continue
 bs=[b for b in runs if b['case']==a['case'] and b['side']==a['side'] and b['dpr']==a['dpr'] and b['mode']=='off']
 if bs:
  b=bs[0];same=all(p['id']==q['id'] and p['pixels']['sha256']==q['pixels']['sha256'] for p,q in zip(a['phases'],b['phases']));correct &= same;offPairs.append({'on':a['id'],'off':b['id'],'sameFinalPixels':same})
c=next(c for c in comparisons if c['dpr']==4 and c['mode']=='on');cold=next(p for p in c['phases'] if p['phase']=='after-freeze')
guards={'clarityNoRegression2Points':all(p['afterLowDensity']<=p['beforeLowDensity']+.02 for c in comparisons if c['mode']=='on' for p in c['phases']),
        'waitNonRegression':all(p['afterWaitMs']<=p['beforeWaitMs']+max(16.7,.10*p['beforeWaitMs'])for c in comparisons for p in c['phases']),
        'inputGapNonRegression':all(p['afterGapMs']<=p['beforeGapMs']+max(16.7,.10*p['beforeGapMs'])for c in comparisons for p in c['phases'])}

native=read('out/'+P+'-native-comparison.json');gpu=read('out/'+P+'-gpu-results.json');packages=read('out/'+P+'-package-checks.json');demo=read('out/'+P+'-demo-smoke.json')
regression=not native['new_failures'] and len(gpu)==10 and all(r['status']=='PASS' for r in gpu) and packages['passed'] and demo['passed'] and '# tests 31' in Path('out/'+P+'-node.log').read_text() and '# fail 0' in Path('out/'+P+'-node.log').read_text() and 'passed (118 contract files)' in Path('out/'+P+'-boundaries.log').read_text()
write('runs.json',runs);write('source-artifact-bindings.json',identities);write('comparison.json',comparisons);write('on-off-parity.json',offPairs)
write('result.json',{'status':'READY_FOR_CONTROL_REVIEW' if correct and regression and all(guards.values()) else 'BLOCKED','blocker':None if correct and regression and all(guards.values()) else ('FROZEN_VERIFICATION_FAILURE' if correct and regression else 'EVIDENCE_OR_REGRESSION_FAILURE'),'sourceRevision':REV,'baselineRevision':BASE,'measurementBindingsAndPixels':correct,'regression':regression,'guards':guards,'dpr4Cold':cold,'native':native,'gpuModes':[r['mode'] for r in gpu],'boundedBrowserRuns':len(runs),'budgetMiB':[32,96],'officialGatePassClaimed':False,'measuredDemo':'http://127.0.0.1:4239/example.html','scope':'Local Chrome synthetic root SDK navigation and native/private GPU conformance. Not physical sleep/input-to-photon,unique GPU memory,all-platform evidence or100K/1M qualification.'})

for f in Path('out').glob(P+'-*'):
 if f.is_file() and not any(n in f.name for n in ['-dpr','-dense-']):copy(f,Path('checks')/f.name)
for f in Path('.aegis/packages/G5-RETIREMENT-ARBITRATION').iterdir():
 if f.is_file():copy(f,Path('package')/f.name)
for n in ['g5-retire-matrix.py','g5-retire-build.py','g5-retire-checks.py','g5-retire-collect.py','g5-dense-native-targets.json','g5-dense-native-test-names.txt']:
 copy(Path('out')/n,Path('reproduce')/n)
print(json.dumps(read(D/'result.json'),indent=2))
