import gzip,json,statistics,sys
from pathlib import Path
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path('out/g5-feedback-v2-pilot-r2')
def read(p):
 return json.loads(p.read_text())if p.exists()else json.loads(gzip.decompress(Path(str(p)+'.gz').read_bytes()))
rows=read(root/'matrix.json')['results'];runs=[];phases=[]
for row in rows:
 p=root/row['id'];s=read(p/'summary.json');record=json.loads(gzip.decompress((p/'wheel.json.gz').read_bytes()));v=[]
 for phase in record['phases']:
  m=phase['summary'];f=m['feedbackV2'];r=m['runtimeSubmitV2'];q=m['navigation']['quality'];raw=phase['raw'];worst=r['worstGap'];records=raw['nativeRecords'];cbs=raw['rafCallbacks'];ev=raw['inputEvents']
  contexts=[]
  for cb in cbs:
   if cb['id'] not in worst['rafIds']:continue
   calls=[]
   for n in records:
    if n.get('rafId')!=cb['id']:continue
    prefix,text=n['text'].split(' ',1);d=json.loads(text)
    calls.append({'kind':prefix,'at':n['at'],'elapsed_ms':d.get('elapsed_ns',0)/1e6,'outcome':d.get('outcome'),'presented':d.get('presented'),'coverage':d.get('coverage'),'knots':d.get('knots'),'queries':d.get('queries'),'counts':d.get('counts'),'stages':d.get('stages'),'navigationDetail':{k:v for k,v in d.items()if k in ['query_ns','initialization_ns','partition_ns','contour_ns','material_ns','other_draw_ns','prepared_builds','prepared_hits','gpu_contours','coarse_masks','budget_yields','steps','cancelled']}})
   contexts.append({'id':cb['id'],'source':cb['source'],'started':cb['started'],'ended':cb['ended'],'durationMs':cb['ended']-cb['started'],'native':calls})
  pval={'run':row['id'],'case':row['case'],'kind':row['kind'],'block':row['block'],'label':row['label'],'phase':phase['id'],'legacyGapMs':m['input_display_submission_gap_ms']['max'],'preInputWaitMs':f['preInputWaitMs'],'legacyInputEndOffsetMs':f['legacyInputEndOffsetMs'],'firstAcceptedMs':r['firstAcceptedSubmitMs'],'steadyAcceptedGapMs':r['steadyAcceptedGapMs']['max'],'acceptedGapMs':r['acceptedGapMs']['max'],'lastEventToFirstCoarseReceiptMs':f['lastInputToCoarseReceiptMs'],'lastEventToLastCoarseReceiptMs':f['lastInputToLastCoarseReceiptMs'],'lowDensity':q['timeWeightedLowResolutionFraction'],'quiescence':raw['quiescence'],'runtimeWorstGap':worst,'callbackContexts':contexts,'exactAttempts':m['exactAttempts'],'exactStages':m['exactStages'],'exactCoverageWork':m['exactCoverageWork'],'longTasks':raw['longTasks'],'pixels':raw['pixels'],'checks':raw['checks']}
  phases.append(pval);v.append(pval)
 endpoints={k:max(p[k] for p in v) for k in ['firstAcceptedMs','steadyAcceptedGapMs','lastEventToLastCoarseReceiptMs','lowDensity']}
 runs.append({**row,'path':p.name,'sourceRevision':s['sourceBefore']['commit'],'sourceDirty':s['sourceBefore']['dirty'],'sourceStable':s['sourceStable'],'artifactStable':s['artifactStable'],'build':record['setup']['build'],'initialObjects':record['setup']['state']['objects'],'checks':record['checks'],'worker':json.loads(Path(str(p)+'-worker.worker.json').read_text()),'endpoints':endpoints})
blocks=[]
for kind in ['AA','AB']:
 for case in ['pan','zoom']:
  for b in range(2):
   group=sorted([r for r in runs if(r['kind'],r['case'],r['block'])==(kind,case,b)],key=lambda r:r['position'])
   if len(group)!=4:continue
   endpoints={}
   for k in group[0]['endpoints']:
    vals=[r['endpoints'][k]for r in group];a=(vals[0]+vals[3])/2;c=(vals[1]+vals[2])/2;margin=.02 if k=='lowDensity'else max(16.7,a*.1)
    endpoints[k]={'values':vals,'baseline':a,'candidate':c,'effect':c-a,'margin':margin,'centeredEffect':c-a-margin}
   blocks.append({'kind':kind,'case':case,'block':b,'interpretation':'same-artifact middle-minus-outer order noise'if kind=='AA'else'ABBA source effect estimate;corroborative pilot only','runs':[r['id']for r in group],'endpoints':endpoints})
result={'schema':'g5-feedback-timing-v2-pilot-analysis','scope':'Actual-input/browser-time Runtime local accepted submit;not physical/camera-qualified. No unchanged empty-view content-call gap is promoted to continuity failure.','workers':len(runs),'methodRevision':runs[0]['sourceRevision'],'runs':runs,'phases':phases,'blocks':blocks,'comparisonStatus':'INCONCLUSIVE','minimumIndependentBlocks':8,'observedBlocksPerCase':2,'priorBlockedUnchanged':True,'rankedWorstLocalSubmitGaps':[{k:p[k]for k in ['run','phase','acceptedGapMs','runtimeWorstGap','callbackContexts','longTasks']}for p in sorted(phases,key=lambda p:p['acceptedGapMs'],reverse=True)[:12]]}
(Path(sys.argv[2]) if len(sys.argv)>2 else Path('out/g5-feedback-v2-analysis.json')).write_text(json.dumps(result,indent=2)+'\n')
print('workers',len(runs),'blocks',len(blocks))
for kind in ['AA','AB']:
 for case in ['pan','zoom']:
  b=[r for r in blocks if(r['kind'],r['case'])==(kind,case)];print(kind,case,{k:[round(r['endpoints'][k]['effect'],3)for r in b]for k in ['firstAcceptedMs','steadyAcceptedGapMs','lastEventToLastCoarseReceiptMs','lowDensity']})
for p in result['rankedWorstLocalSubmitGaps'][:6]:
 w=p['runtimeWorstGap'];print(p['run'],p['phase'],'gap',round(p['acceptedGapMs'],2),'callback_wall',round(w['observedCallbackCoveredMs'],2),'outside',round(w['outsideObservedCallbacksMs'],2),'longtask',round(w['longTaskOverlapMs'],2))
