from pathlib import Path
import json,gzip,hashlib,shutil,subprocess,platform,re
R=Path.cwd();S=R/'out/g5-erase-final';D=R/'verification/evidence/performance/g5-erase-recovery'
def load(p):return json.loads(gzip.decompress(p.read_bytes()))if p.suffix=='.gz'else json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def copy(p,d):
 d.parent.mkdir(parents=True,exist_ok=True)
 if p.suffix=='.log':d=d.with_suffix('.log.gz');d.write_bytes(gzip.compress(p.read_bytes(),mtime=0))
 else:shutil.copyfile(p,d)
bodies={p.parent.name:load(p)for p in S.glob('*dpr*/observation.json.gz')};assert len(bodies)==8,len(bodies)
code=subprocess.check_output(['git','rev-parse','b47781cc'],text=True).strip();runner=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
summary={}
for name,b in bodies.items():
 expected='REPRODUCED_REJECTION_BUG'if name=='baseline-off-dpr2'else'MEASURED';assert b['status']==expected,(name,b.get('error'));assert b['sourceStable']and b['artifactStable'];assert b['sourceBefore']['commit']==b['sourceAfter']['commit']==runner and not b['sourceBefore']['dirty']and not b['sourceAfter']['dirty']
 if 'post-edit'in name:
  summary[name]={'status':b['status'],'build':b['setup']['build'],'edits':len(b['edits']),'navigation':[{'id':x['id'],'contentGapMs':x['feedback']['contentGapMs'],'finalReceiptMs':x['feedback']['lastInputToLastCoarseReceiptMs']}for x in b['navigation']]};continue
 expectedCode='5ab41d5f8782958f8bdcbf329f81929da55c2f03'if name.startswith('baseline')else code;assert b['setup']['build']['sourceRevision']==expectedCode and not b['setup']['build']['dirty'];assert len(b['sweeps'])==6
 if name.startswith('candidate'):assert all(b['recovery']['checks'].values())and not b['recovery']['retryErrors']and b['close']['closed'];assert not b['errors']and not b['inputErrors']
 summary[name]={'status':b['status'],'build':b['setup']['build'],'dpr':b['dpr'],'upMs':[s['upMs']for s in b['sweeps']],'settledMs':[s['settledMs']for s in b['sweeps']],'cpuTaskDeltaSeconds':[s['cpuAfter']['TaskDuration']-s['cpuBefore']['TaskDuration']for s in b['sweeps']],'history':b['historyChecks'],'noop':b['noopChecks'],'cancel':[c['checks']for c in b['cancels']],'recovery':b['recovery']['checks'],'expectedFailureErrors':b['recovery']['failureErrors'],'retryErrors':b['recovery']['retryErrors'],'feedback':b['feedback'],'runtime':b['runtime'],'allSweepChecks':all(all(s['checks'].values())for s in b['sweeps'])}
pairs={}
def checkpoints(b):return[b['initial'],*[s[k]for s in b['sweeps']for k in ['beforeCheckpoint','afterCheckpoint']],b['undo'],b['redo']]
for a,c in [('baseline-off-dpr2','candidate-off-dpr2'),('candidate-off-dpr2','candidate-on-dpr2'),('candidate-off-dpr2','candidate-off-dpr2-repeat'),('candidate-off-dpr2','candidate-off-dpr1'),('candidate-off-dpr2','candidate-off-dpr4')]:
 x,y=checkpoints(bodies[a]),checkpoints(bodies[c]);v={'exportPixels':all(i['image']['sha256']==j['image']['sha256']for i,j in zip(x,y)),'snapshotBytes':all(i['snapshot']['sha256']==j['snapshot']['sha256']for i,j in zip(x,y)),'checkpoints':len(x)};pairs[a+'__'+c]=v;assert all(v[k]for k in ['exportPixels','snapshotBytes']),(a,c,v)
x,y=bodies['baseline-post-edit-dpr4'],bodies['candidate-post-edit-dpr4'];pairs['post-edit-dpr4']={'exportPixels':all(i['after']['image']['sha256']==j['after']['image']['sha256']for i,j in zip(x['edits'],y['edits'])),'navigationPixels':all(i['raw']['pixels']['sha256']==j['raw']['pixels']['sha256']for i,j in zip(x['navigation'],y['navigation']))};assert all(pairs['post-edit-dpr4'].values())
rejectedPixels=load(S/'rejected-pixels.json');assert rejectedPixels['status']=='MEASURED'and all(rejectedPixels['checks'].values());
capacity=load(S/'capacity.json');assert not capacity['errors']and all(capacity['checks'].values()),capacity.get('checks')
native=load(S/'native-comparison.json');assert not native['new_failed_names'];nBefore=load(S/'geometry-before-native.json');nAfter=load(S/'candidate-native.json');assert all(nAfter['checks'].values());workers=[load(p)for p in S.glob('*.worker.json')];assert len(workers)==12 and all(w['reason']=='completed'and w['cleanup_complete']for w in workers),(len(workers),workers)
D.mkdir(exist_ok=True)
copy(R/'out/g5-erase-reject-pixels.mjs',D/'method/g5-erase-reject-pixels.mjs')
snapshotIndex={}
for p in S.rglob('*'):
 if not p.is_file():continue
 if p.suffix=='.axsnap':
  h=sha(p);d=D/'raw/snapshots'/(h+'.axsnap.gz');d.parent.mkdir(parents=True,exist_ok=True)
  if not d.exists():d.write_bytes(gzip.compress(p.read_bytes(),mtime=0))
  snapshotIndex[str(p.relative_to(S))]={'sha256':h,'bytes':p.stat().st_size,'stored':str(d.relative_to(D))}
 else:copy(p,D/'raw'/p.relative_to(S))
write(D/'raw/snapshot-index.json',snapshotIndex)
for p in R.glob('out/g5-erase-*'):
 if p.is_file()and p.suffix in ['.log','.py','.json']:copy(p,D/'logs'/p.name)
for p in (R/'out/g5-erase-geometry-before').iterdir():
 if p.name in ['commands.json','selection_region_geometry.cpp','build.log']:copy(p,D/'method/geometry-before'/p.name)
for p in ['out/g5-erase-withdrawn-eager-clip.cpp']:
 q=R/p
 if q.exists():copy(q,D/'exploratory'/q.name)
for label,prefix in [('pilot-async','g5-erase-pilot-baseline-data'),('unseeded','g5-erase-baseline-unseeded-data')]:
 q=R/'out'/prefix/'observation.json.gz'
 if q.exists():copy(q,D/'exploratory'/label/'observation.json.gz')
write(D/'summary.json',summary);write(D/'paired-content.json',pairs);write(D/'native-performance.json',{'before':nBefore,'after':nAfter});write(D/'packages.json',load(R/'out/g5-erase-packages.json'))
invariants={}
for p in ['runtime/semantic/src/codec.cpp','runtime/semantic/src/snapshot.cpp','semantic-sdk.lock.json','r1-full-skia-sdk.lock.json','sdk/version.json','sdk/web/axiom.d.mts','sdk/windows/include/axiom/api.h']:
 v={'sha256':sha(R/p),'unchangedFromAnchor':subprocess.check_output(['git','show','797345f1:'+p])==(R/p).read_bytes()};assert v['unchangedFromAnchor'];invariants[p]=v
write(D/'execution-inputs.json',{'codeRevision':code,'verificationRevision':runner,'testAndPackageRevision':subprocess.check_output(['git','rev-parse','518b2730'],text=True).strip(),'package':{'path':'docs/planning/G5_ERASE_RECOVERY_2026_11_07.json','sha256':sha(R/'docs/planning/G5_ERASE_RECOVERY_2026_11_07.json')},'sourceBefore':bodies['candidate-off-dpr2']['sourceBefore'],'sourceAfter':bodies['candidate-off-dpr2']['sourceAfter'],'platform':platform.platform(),'machine':platform.machine(),'nativeBinarySha256':sha(R/'out/host/sdk/common/axiom_native_transport_host'),'geometryOnlyBeforeBinarySha256':sha(R/'out/g5-erase-geometry-before/axiom_native_transport_host'),'unchanged':invariants,'scopes':{'baselineWeb':'Clean5ab41d5f package;complete previous production path','nativeBefore':'Geometry-isolated comparator:797345 geometry plus identical candidate objects for everything else. Original discovery native timing has no saved baseline binary hash and is diagnostic only.','webStream':'4samples/RAF,4ms sample timestamps;native synchronous corpus uses10ms;do not compare absolute native/Web latency.','pilot':'Old runner missed asynchronous onError microtasks and overlapped native regression;BLOCKED_EVIDENCE,not final performance.','withdrawn':'Eager whole-component clipping worsened first erase;withdrawn and kept diagnostic.'}})
write(D/'result.json',{'status':'READY_FOR_CONTROL_REVIEW','officialGatePassClaimed':False,'codeRevision':code,'verificationRevision':runner,'scope':'Bounded local partial-erase performance and rejected input recovery;not universal CPU%,physical input,otherplatform,release or G5/P34 acceptance','native':native,'nativePerformanceEndpoint':nAfter['checks'],'checks':{'focusedCTest':3,'g5Node':39,'runtimeBoundaries':119,'pointerCollector':True,'webPackageBoundary':True,'webDenseWorkers':6,'mixedPostEditWorkers':2,'capacityRootSdkWorker':1,'rejectedPixelRootSdkWorker':1,'nativeWorkers':2,'allWorkerCleanup':True},'pairedContent':pairs,'budgetsMiB':{'optional':32,'shared':96},'remaining':['Existing native failures enumerated in raw/native-comparison.json','Real physical pointer/capture and original unsaved document not qualified by synthetic corpus','Finite eraser geometry,canonical rendering and navigation CPU work remain;no guarantee of zeroCPU or all sizes']})
print(json.dumps({'destination':str(D),'status':'READY_FOR_CONTROL_REVIEW','native':native['passed'],'workers':len(workers)}))
