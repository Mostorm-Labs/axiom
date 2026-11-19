from pathlib import Path
import json,math,statistics
W=Path(__file__).resolve().parents[2];O=W/'out/G5-settled-navigation-recovery-r1';E=W/'.aegis/results/G5-settled-navigation-recovery-r1'
def summary(v):return {'n':len(v),'median':statistics.median(v),'p95':sorted(v)[math.ceil(.95*len(v))-1],'max':max(v)}if v else None
rows=[];life=[];progress=[]
for role in ['SNR-D-B-ORDER','SNR-D-B-ISOLATED','SNR-D-B-REVERSE','SNR-D-B-PROGRESS','SNR-D-B-PROGRESS-CORRECTIVE']:
 root=O/'web'/role
 for p in sorted(root.glob('DPR2/**/raw.json')):
  x=json.loads(p.read_text());j=x.get('scheduling',{});completed=x['status']=='OBSERVED'and x.get('closed');life.append({'run':role,'path':str(p),'closed':completed,'support':j.get('support'),'lifecycle':j.get('lifecycle'),'dropped':j.get('dropped'),'pendingAtEnd':j.get('pending'),'longtasks':j.get('longtasks')})
  for c in x['cases']:
   if c['status']!='OBSERVED':continue
   pe=next(v for v in c['erases']if v['label']=='primary-long-erase');up=pe['events'][-1]['at'];r=c['route'];allrafs=[v for v in j['raf']if v.get('started')and'ended'in v]
   tail=[v for v in allrafs if up-2000<=v['started']<up];recovery=[v for v in allrafs if r['inputEnded']<=v['started']<=r['stableAt']]
   actualContent=[v for v in r['observation']['colorOps']if v['kind']=='content'and r['inputEnded']<=v['started']<=r['stableAt']]
   inputs=pe['events'];gaps=[]
   for a,b in zip(tail,tail[1:]):
    gap=b['started']-a['ended'];events=[v for v in inputs if a['ended']<=v['at']<=b['started']]
    gaps.append({'gapMs':gap,'previousEnd':a['ended'],'requestAt':b['requestedAt'],'callbackStart':b['started'],'requestedAfterPreviousEndMs':b['requestedAt']-a['ended'],'queueDelayMs':b['started']-b['requestedAt'],'inputEventsInside':len(events)})
   rows.append({'run':role,'case':c['id'],'path':str(p),'closed':completed,'generationBeforePan':r['startState']['generation'],'commitBeforePan':r['startState']['lastCommit'],'metrics':r['metrics'],'navigationMs':r['inputEnded']-r['plannedDown'],'eraseMs':pe['upReturn']-pe['started'],'tailCallbacks':len(tail),'tailCPUms':summary([v['ended']-v['started']for v in tail]),'tailRequestDelayMs':summary([v['started']-v['requestedAt']for v in tail]),'tailNonCallbackGapsMs':summary([v['gapMs']for v in gaps]),'largestTailGaps':sorted(gaps,key=lambda v:-v['gapMs'])[:12],'recoveryCallbacks':len(recovery),'recoveryCPUms':summary([v['ended']-v['started']for v in recovery]),'recoveryCPUTotalMs':sum(v['ended']-v['started']for v in recovery),'recoveryRequestDelayMs':summary([v['started']-v['requestedAt']for v in recovery]),'actualDefaultContentWritesAfterInput':len(actualContent),'lastContentMsAfterInput':max((v['ended']-r['inputEnded']for v in actualContent),default=None),'journalDropped':j.get('dropped'),'nativeDropped':c['dropped'],'inputCounts':{'primaryMoves':sum(v['type']=='move'for v in pe['events']),'navigationMoves':len(r['inputs'])}})
   if 'PROGRESS'in role:
    records=[v for v in c['diagnostic']if v['marker']=='AXIOM_SNR_PROGRESS '];recs=[v for v in records if r['inputEnded']<=v['at']<=r['stableAt']];before=[v for v in recs if v['phase']=='prepare-before'];after=[v for v in recs if v['phase']=='prepare-after'];reset=[v for v in recs if v['phase']=='reset-before'];pairs=[]
    for i,a in enumerate(before):
     b=next((v for v in after if v['at']>=a['at']and v['cache']==a['cache']and v['job']==a['job']),None)
     if b:pairs.append({'at':a['at'],'job':a['job'],'frame':a['frame'],'semantic':a['semantic'],'scene':a['scene'],'camera':a['camera'],'cursorBefore':a['cursor'],'cursorAfter':b['cursor'],'rasterBefore':a['pending_raster'],'rasterAfter':b['pending_raster'],'primitiveBefore':a['primitive'],'primitiveAfter':b['primitive'],'tileBefore':[a['tile_x'],a['tile_y']],'tileAfter':[b['tile_x'],b['tile_y']],'stepsDelta':b['coverage_steps']-a['coverage_steps'],'bodyTilesDelta':b['body_tiles']-a['body_tiles'],'maskUpdatesDelta':b['mask_updates']-a['mask_updates'],'rasterGroupsDelta':b['raster_groups']-a['raster_groups'],'pendingBytes':b['pending_bytes'],'usedBytes':b['used_bytes'],'gpuEpochs':b['gpu_epochs']})
    progress.append({'run':role,'rawPath':str(p),'case':c['id'],'records':len(records),'recoveryRecords':len(recs),'pairs':pairs,'resets':reset,'rawBasis':'Each record includes provider/target/resource/targetEpoch/frame/semanticScene/camera/surfaceMetrics plus cursor. Final source maps object/mask identity; no approximate document state inferred from camera only.','frameOutcomes':[v for v in c['diagnostic']if v['marker']=='AXIOM_SNR_LIFECYCLE 'and v['event']=='frame-outcome'and r['inputEnded']<=v['at']<=r['stableAt']],'retirement':[v for v in c['diagnostic']if v['marker']=='AXIOM_GPU_RETIREMENT 'and r['inputEnded']<=v['at']<=r['stableAt']]})
(E/'scheduling-timeline.json').write_text(json.dumps({'status':'OBSERVED_BASELINE_DIAGNOSTICS','rows':rows,'ordinaryPerformance':False,'limit':'Requested callback delay is not exclusively Runtime CPU. No implicit environment waiver from unreproduced old delay.'},indent=2)+'\n')
(E/'lifecycle-and-resources.json').write_text(json.dumps({'rows':life,'physicalGPURelease':'unavailable','logicalSurfaceTrafficNotLeakProof':True},indent=2)+'\n')
(E/'preparation-progress.json').write_text(json.dumps({'rows':progress,'status':'OBSERVED'if progress else'PROGRESS_WINDOW_NOT_YET_RUN','noDefaultWriteDoesNotMeanNoOffscreenProgress':True},indent=2)+'\n')
for v in rows:print(v['run'],v['case'],'tail',v['tailCallbacks'],'reqmax',v['tailRequestDelayMs']['max']if v['tailRequestDelayMs']else None,'recovery',v['metrics']['convergenceMs'],'callbacks',v['recoveryCallbacks'])
