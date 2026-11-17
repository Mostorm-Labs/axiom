from pathlib import Path
import json,sys,collections
stages='owner pump resource navigation canonical render validation cacheBegin bodies materials sceneDraw flush present fullBody'.split()
facts='camera scene semantic surface metrics navigationPending navigationPresented canonicalRequested needsPaint canonicalWork bodiesStatus materialStatus cursor yields fullBodyCalls fullBodyPending coverageSteps geometryAdvances gpuTiles cpuTiles maskPrimitives maskUpdates bodyTileBuilds bodyTileHits rasterGroups cacheHits entries compileInputs used gpuEpochs pendingRelease staging'.split()
def analyze(path):
 d=json.loads(Path(path).read_text());out=[]
 for phase in d['routes']:
  rows=[]
  for source in phase.get('stopTrace',[]):
   row={'at':source['at'],'startNs':int(source['start']),'endNs':int(source['end']),'phases':{k:{'inclusiveMs':int(v[0])/1e6,'selfMs':int(v[1])/1e6,'calls':int(v[2])}for k,v in zip(stages,source['times'])},'facts':{k:int(v)for k,v in zip(facts,source['facts'])}};rows.append(row)
  tail=[r for r in rows if r['at']>=phase['inputEnded']];work={k:sum(r['phases'][k]['selfMs']for r in tail)for k in stages};elapsed=phase['stableAt']-phase['inputEnded'];total=sum(work.values());frames=[v for v in d['frames']if phase['inputEnded']<=v['at']<=phase['stableAt']]
  r={'route':phase['route'],'inputEnded':phase['inputEnded'],'stableAt':phase['stableAt'],'stableElapsedMs':elapsed,'tailOwnerCalls':len(tail),'exclusiveCPURecordedMs':total,'residualElapsedNotCPUms':elapsed-total,'exclusivePhaseMs':work,'counterTotals':{k:sum(v['facts'].get(k,0)for v in tail)for k in['coverageSteps','geometryAdvances','gpuTiles','cpuTiles','maskPrimitives','bodyTileBuilds','bodyTileHits','rasterGroups','fullBodyCalls','fullBodyPending']},'postInputFrames':frames,'traceDropped':phase.get('stopTraceDropped'),'rows':tail};out.append(r)
  print(phase['route'],'elapsed',round(elapsed,2),'CPU',round(total,2),'rows',len(tail),'drop',phase.get('stopTraceDropped'),'work',work)
  for row in tail:print(' ',round(row['at']-phase['inputEnded'],2),round(row['phases']['owner']['inclusiveMs'],2),row['facts'])
 return out
if __name__=='__main__':
 result=analyze(sys.argv[1]);
 if len(sys.argv)>2:Path(sys.argv[2]).write_text(json.dumps(result,indent=2)+'\n')
