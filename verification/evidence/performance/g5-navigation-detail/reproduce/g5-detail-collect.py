import hashlib,json,gzip,shutil,subprocess,re
from pathlib import Path
D=Path('verification/evidence/performance/g5-navigation-detail');P='g5-detail-final'
REV='c8bc77ec';REV=subprocess.check_output(['git','rev-parse',REV],text=True).strip();BASE='90ef6b691e7d980ea8ed59195b5311eb7e3da4d9'
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
guards={'cold15Percent':cold['afterWaitMs']<=.85*cold['beforeWaitMs'],'clarity10Points':cold['beforeLowDensity']-cold['afterLowDensity']>=.10,'navigation150Ms':all(p['afterGapMs']<=150 for c in comparisons for p in c['phases'])}
native=read('out/'+P+'-native-comparison.json');gpu=read('out/'+P+'-gpu-results.json');packages=read('out/'+P+'-package-checks.json');demo=read('out/'+P+'-demo-smoke.json')
regression=not native['new_failures'] and len(gpu)==9 and all(r['status']=='PASS' for r in gpu) and packages['passed'] and demo['passed'] and '# tests 31' in Path('out/'+P+'-node.log').read_text() and '# fail 0' in Path('out/'+P+'-node.log').read_text() and 'passed (118 contract files)' in Path('out/'+P+'-boundaries.log').read_text()
write('runs.json',runs);write('source-artifact-bindings.json',identities);write('comparison.json',comparisons);write('on-off-parity.json',offPairs)
write('result.json',{'status':'READY_FOR_CONTROL_REVIEW' if correct and regression and all(guards.values()) else 'BLOCKED','blocker':None if correct and regression and all(guards.values()) else ('FROZEN_VERIFICATION_FAILURE' if correct and regression else 'EVIDENCE_OR_REGRESSION_FAILURE'),'sourceRevision':REV,'baselineRevision':BASE,'measurementBindingsAndPixels':correct,'regression':regression,'guards':guards,'dpr4Cold':cold,'native':native,'gpuModes':[r['mode'] for r in gpu],'boundedBrowserRuns':len(runs),'budgetMiB':[32,96],'officialGatePassClaimed':False,'measuredDemo':'http://127.0.0.1:4234/example.html','scope':'Local Chrome synthetic root SDK navigation and native/private GPU conformance. Not physical sleep/input-to-photon,unique GPU memory,all-platform evidence or100K/1M qualification.'})
for f in Path('out').glob(P+'-*'):
 if f.is_file() and not any(n in f.name for n in ['-dpr','-dense-']):copy(f,Path('checks')/f.name)
for n in ['g5-detail-final-matrix.py','g5-detail-build.py','g5-detail-final-checks.py','g5-detail-collect.py','g5-dense-native-targets.json','g5-dense-native-test-names.txt','g5-detail-share-red-final-fixture.log','g5-detail-replay-red.log','g5-detail-residency-red.log','g5-detail-obsolete-red-final.log','g5-detail-a2-red.log']:
 copy(Path('out')/n,Path('reproduce' if n.endswith('.py') else 'checks')/n)
for name in ['g5-detail-pilot-before','g5-detail-pilot-after']:
 tree(Path('out')/name,Path('exploratory')/name)
 for suffix in ['-worker.worker.json','-worker.stdout.log','-worker.stderr.log']:
  f=Path('out/'+name+suffix)
  if f.exists():copy(f,Path('exploratory')/(name+suffix))
for row in read('out/g5-detail-a1-extra-matrix.json'):
 tree(row['output'],Path('pre-a2-903761e4')/Path(row['output']).name)
 for suffix in ['-worker.worker.json','-worker.stdout.log','-worker.stderr.log']:
  f=Path(row['output']+suffix)
  if f.exists():copy(f,Path('pre-a2-903761e4')/(Path(row['output']).name+suffix))
copy('out/g5-detail-a1-extra-matrix.json','pre-a2-903761e4/matrix.json')
write('retained-candidate.json',read('out/g5-detail-rollback.json'))
copy('out/g5-detail-next-step-review.md','next-step-review.md')
for f in Path('.aegis/packages/G5-NAVIGATION-DETAIL').iterdir():
 if f.is_file():copy(f,Path('package')/f.name)
print(json.dumps(read(D/'result.json'),indent=2))

# The current branch withdrew A2;retain a separate exact binding and do not
# relabel c8 measurements as performance of the restored candidate.
for f in Path('out').glob('g5-detail-retained-*'):
 if f.is_file():copy(f,Path('retained-checks')/f.name)
tree('out/g5-detail-retained-root','retained-root')
for name in ['g5-detail-a1-native-comparison.json','g5-detail-a1-native.log','g5-detail-a1-gpu-results.json']:
 copy(Path('out')/name,Path('pre-a2-903761e4')/name)

r=read(D/'result.json');r['withdrawnExperiment']=True;r['activeCandidate']=read('out/g5-detail-rollback.json');r['activePerformanceAcceptance']='NOT_ACCEPTED';write('result.json',r)
