from pathlib import Path
import json,gzip,hashlib,subprocess,shutil,platform
R=Path.cwd();S=R/'out/g5-dpr4-final';D=R/'verification/evidence/performance/g5-dpr4-critical-replay'
def load(p):return json.loads(gzip.decompress(p.read_bytes()))if p.suffix=='.gz'else json.loads(p.read_text())
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,d):
 d.parent.mkdir(parents=True,exist_ok=True)
 if p.suffix=='.log':d=d.with_suffix('.log.gz');d.write_bytes(gzip.compress(p.read_bytes(),mtime=0))
 else:shutil.copyfile(p,d)
bodies={p.parent.name:load(p)for p in S.glob('*dpr*/observation.json.gz')};assert len(bodies)==10
summary={}
for name,b in bodies.items():
 assert b['status']=='MEASURED'and b['sourceStable']and b['artifactStable']and not b['sourceBefore']['dirty']and not b['sourceAfter']['dirty']
 assert b['sourceBefore']['commit']==b['sourceAfter']['commit']=='5ab41d5f8782958f8bdcbf329f81929da55c2f03'
 build=b['setup']['build'];expected='5ab41d5f8782958f8bdcbf329f81929da55c2f03'if name.startswith('candidate')else'c05d3a2f54fcb44c771d797bbe3255f46dcac962';assert build['sourceRevision']==expected and not build['dirty']
 rows=[]
 for r in b['navigation']:
  f=r['feedback'];n=[json.loads(x['text'][24:])for x in r['raw']['nativeRecords']if x['text'].startswith('AXIOM_NAVIGATION_TIMING ')];frames=[json.loads(x['text'][19:])for x in r['raw']['nativeRecords']if x['text'].startswith('AXIOM_FRAME_TIMING ')]
  rows.append({'id':r['id'],'contentGapMs':f['contentGapMs'],'firstContentResponseMs':f['firstContentResponseMs'],'finalReceiptWaitMs':f['lastInputToLastCoarseReceiptMs'],'acceptedGapMs':r['runtime']['acceptedGapMs'],'acceptedStatus':r['runtime']['status'],'canvasPixels':r['raw']['pixels'],'checks':r.get('checks',r['raw'].get('checks')),'preparedBytes':max((x['prepared_bytes']for x in n),default=None),'preparedDenied':sum(x['prepared_denied']for x in n)if n else None,'coverageIncomplete':sum(not x['coverage']for x in n)if n else None,'maxCanonicalAttemptMs':max((x['stages']['attempt']['inclusive_ns']/1e6 for x in frames),default=None),'canonicalCpuCoverageMs':sum(x['stages'].get('cpu_coverage',{}).get('exclusive_ns',0)for x in frames)/1e6 if frames else None})
 pan=next(r for r in rows if r['id']=='idle-selected-pan');endpoint=not name.startswith('candidate')or b['dpr']!=4 or pan['contentGapMs']['max']<150;assert endpoint
 summary[name]={'status':b['status'],'build':build,'dpr':b['dpr'],'browser':b['browser'],'edits':len(b['edits']),'navigationPhases':len(rows),'rows':rows,'cache':b['cache'],'restore':{k:b['restore'][k]for k in ['firstReceiptWaitMs','completeMs','checks','surfaceDiagnostic']}if b.get('restore')else None,'finalExport':b['final']['image'],'endpoint':endpoint,'raw':'raw/'+name+'/observation.json.gz'}
 if b['cache']:assert b['cache']['peakUsedBytes']<=96*1024*1024
pairs={}
for a,c in [('baseline-off-dpr4','candidate-off-dpr4'),('baseline-on-dpr4','candidate-on-dpr4'),('baseline-on-dpr4','candidate-on-dpr4-repeat'),('candidate-on-dpr4','candidate-off-dpr4'),('candidate-on-dpr1','candidate-off-dpr1'),('candidate-on-dpr2','candidate-off-dpr2')]:
 x,y=bodies[a],bodies[c];v={'edits':all(i['after']['image']['sha256']==j['after']['image']['sha256']for i,j in zip(x['edits'],y['edits'])),'navigationCanvas':len(x['navigation'])==len(y['navigation'])and all(i['id']==j['id']and i['raw']['pixels']['sha256']==j['raw']['pixels']['sha256']for i,j in zip(x['navigation'],y['navigation'])),'finalExport':x['final']['image']['sha256']==y['final']['image']['sha256']};assert all(v.values());pairs[a+'__'+c]=v
prior=R/'verification/evidence/performance/g5-post-main-followup/raw'
nonreg=[]
for side in ['on','off']:
 for dpr in [1,2]:
  name=f'candidate-{side}-dpr{dpr}';a=load(prior/f'{side}-dpr{dpr}/observation.json.gz');old=max(r['feedback']['contentGapMs']['max']for r in a['navigation']);new=max(r['contentGapMs']['max']for r in summary[name]['rows']);assert new<=old+30;nonreg.append({'id':name,'oldObservedMaxMs':old,'newObservedMaxMs':new,'allowedEngineeringMarginMs':30,'note':'single-worker local guard,not statistical nonregression'})
native=load(S/'native-comparison.json');assert native['passed']==1010 and not native['new_failed_names']
workers=[load(p)for p in S.rglob('*.worker.json')];assert len(workers)==12 and all(w['reason']=='completed'and w['cleanup_complete']for w in workers)
smoke={}
for name in ['10000-objects','1000000-total-points']:
 b=load(S/name/'observation.json.gz');assert b['status']=='completed'and b['sourceStable']and b['artifactStable'];smoke[name]={k:b.get(k)for k in ['status','objects','points','visible_points','canonicalPointsObserved','cache','checks','setup']}
assert smoke['1000000-total-points']['points']==1000000 and smoke['10000-objects']['objects']==10000
D.mkdir(exist_ok=True)
for p in S.rglob('*'):
 if p.is_file()and p.suffix in ['.json','.gz','.log']:copy(p,D/'raw'/p.relative_to(S))
for p in R.glob('out/g5-dpr4-*'):
 if p.is_file()and p.suffix in ['.log','.py']:copy(p,D/'logs'/p.name)
write(D/'navigation-summary.json',summary);write(D/'paired-pixels.json',pairs);write(D/'nonregression-guards.json',nonreg);write(D/'scale-smoke.json',smoke)
inputs={'codeRevision':'5ab41d5f8782958f8bdcbf329f81929da55c2f03','packageRevision':subprocess.check_output(['git','rev-parse','4d665c23'],text=True).strip(),'sourceBefore':bodies['candidate-on-dpr4']['sourceBefore'],'sourceAfter':bodies['candidate-on-dpr4']['sourceAfter'],'nativeBinarySha256':sha(R/'out/host/runtime/render/axiom_render_cache_tests'),'preparedBinarySha256':sha(R/'out/host/runtime/render/axiom_prepared_geometry_tests'),'platform':platform.platform(),'machine':platform.machine(),'red':{'oldGeometryBlobSha256':hashlib.sha256(subprocess.check_output(['git','show','1b22b23c:runtime/render/src/skia_prepared_geometry.cpp'])).hexdigest(),'preparedTest':'4d665c23:runtime/render/tests/prepared_geometry_test.cpp','navigationTest':'5ab41d5f:runtime/render/tests/render_cache_test.cpp','expectedFailures':['visible short replay denied despite hard-cap headroom','offscreen short ink delayed visible coverage under soft pressure'],'procedure':'Prepared RED ran before implementation. Integration RED temporarily used exactly the old1b22b23c geometry source with the new navigation test,then restored only the owned fixed source. Logs preserve both outcomes.'}}
write(D/'execution-inputs.json',inputs)
preserved=['runtime/semantic/src/codec.cpp','runtime/semantic/src/snapshot.cpp','runtime/semantic/include/canvas/semantic/compact_stroke.hpp','semantic-sdk.lock.json','r1-full-skia-sdk.lock.json','sdk/version.json']
invariants={p:{'sha256':sha(R/p),'unchangedFromAnchor':subprocess.check_output(['git','show','1b22b23c:'+p])==(R/p).read_bytes()}for p in preserved};assert all(x['unchangedFromAnchor']for x in invariants.values());write(D/'preserved-inputs.json',invariants)
write(D/'result.json',{'status':'READY_FOR_CONTROL_REVIEW','officialGatePassClaimed':False,'scope':'DPR4 critical replay admission and local engineering corpus;not universal/statistical/physical/platform/G5/P34 PASS','codeRevision':inputs['codeRevision'],'packageRevision':inputs['packageRevision'],'productionFiles':['runtime/render/src/skia_prepared_geometry.cpp'],'native':native,'otherChecks':{'g5Node':39,'boundaries':119},'browserWorkers':10,'allWorkerCleanup':True,'localEndpoint':'candidateDPR4selectedidlepan<150ms in twoON samples,OFF andrestore;all met','pixelPairs':pairs,'dpr1Dpr2NonregressionGuards':nonreg,'budgetsMiB':{'optional':32,'shared':96},'workLimitsChanged':False,'sourceArtifactStable':True,'admissionBoundaryChanged':False,'remaining':['No formal scale/physical/Gate acceptance','66existing native failed/not-built cases','100K/1Mobject admission/containment scope requires Semantic/product authority','Possible residual zoom CPU coverage and other cold paths remain separately measured']})
print(json.dumps({'destination':str(D),'workers':len(workers),'browsers':len(bodies),'status':'READY_FOR_CONTROL_REVIEW'}))
