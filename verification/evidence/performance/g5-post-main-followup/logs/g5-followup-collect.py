from pathlib import Path
import json,gzip,hashlib,shutil,platform,subprocess,re
R=Path.cwd();S=R/'out/g5-followup-final';D=R/'verification/evidence/performance/g5-post-main-followup';D.mkdir(parents=True,exist_ok=True)
def load(p):return json.loads(gzip.decompress(p.read_bytes()))if p.suffix=='.gz'else json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def copy(p,d):
 d.parent.mkdir(parents=True,exist_ok=True)
 if p.suffix=='.log':d=d.with_suffix(d.suffix+'.gz');d.write_bytes(gzip.compress(p.read_bytes(),mtime=0))
 else:shutil.copyfile(p,d)
# Raw observations, outcomes and worker logs. Snapshot payloads remain reproducible
# local artifacts; their full SHA/byte size and public-SDK export hashes are in raw.
for p in S.rglob('*'):
 if p.is_file()and(p.suffix in ['.json','.gz','.log']):copy(p,D/'raw'/p.relative_to(S))
for p in R.glob('out/g5-followup-*'):
 if p.is_file()and p.suffix in ['.log','.py']:copy(p,D/'logs'/p.name)
# Keep diagnostic-only/failed fixture pilots separate from clean-source results.
for name in ['g5-followup-runner-red','g5-followup-runner-pilot','g5-followup-runner-pilot2','g5-followup-runner-pilot3','g5-followup-context-pilot']:
 p=R/'out'/name/'observation.json.gz'
 if p.exists():copy(p,D/'pilots'/name/'observation.json.gz')
 for ext in ['stdout.log','stderr.log','worker.json']:
  p=R/'out'/(name+'-worker.'+ext)
  if p.exists():copy(p,D/'pilots'/(name+'-worker.'+ext))
nav={};bodies={}
for p in sorted(S.glob('*dpr*/observation.json.gz')):
 b=load(p);name=p.parent.name;bodies[name]=b;w=load(S/(name+'-worker.worker.json'))
 assert b['status']=='MEASURED'and b['sourceStable']and b['artifactStable']and not b['sourceBefore']['dirty']and not b['sourceAfter']['dirty']
 assert b['setup']['build']['sourceRevision']=='c05d3a2f54fcb44c771d797bbe3255f46dcac962'and not b['setup']['build']['dirty']
 assert w['reason']=='completed'and w['cleanup_complete']
 rows=[]
 for x in b['navigation']:
  raw=x['raw'];frames=[json.loads(v['text'][19:])for v in raw['nativeRecords']if v['text'].startswith('AXIOM_FRAME_TIMING ')];ns=[json.loads(v['text'][24:])for v in raw['nativeRecords']if v['text'].startswith('AXIOM_NAVIGATION_TIMING ')]
  rows.append({'id':x['id'],'checks':x.get('checks',raw.get('checks')),'feedback':x['feedback'],'runtime':x['runtime'],'settleMilliseconds':raw.get('settleMilliseconds'),'pixel':raw['pixels'],'maxCanonicalAttemptMs':max((f['stages']['attempt']['inclusive_ns']/1e6 for f in frames),default=None),'cpuCoverageMs':sum(f['stages'].get('cpu_coverage',{}).get('exclusive_ns',0)for f in frames)/1e6 if frames else None,'maxNavigationCallMs':max((f['elapsed_ns']/1e6 for f in ns),default=None)})
 nav[name]={'status':b['status'],'dpr':b['dpr'],'edits':len(b['edits']),'navigationPhases':len(rows),'build':b['setup']['build'],'browser':b['browser'],'objects':b['final']['state']['objects'],'targets':b['setup']['targets'],'initialExport':b['initial']['image'],'finalExport':b['final']['image'],'editChecks':[{'id':v['id'],'checks':v['checks'],'dispatchMs':v['edit']['dispatchMs'],'completeMs':v['edit']['completeMs']}for v in b['edits']],'cache':b['cache'],'restore':{k:b['restore'][k]for k in ['firstReceiptWaitMs','completeMs','checks','surfaceDiagnostic']}if 'restore'in b else None,'worker':w,'navigation':rows}
write(D/'edited-navigation.json',nav)
pairs={}
for a,c in [('on-dpr1','off-dpr1'),('on-dpr2','off-dpr2'),('on-dpr2-restore','off-dpr2-restore'),('on-dpr4','off-dpr4'),('on-dpr4-repeat','off-dpr4')]:
 x,y=bodies[a],bodies[c];v={'sameEditCount':len(x['edits'])==len(y['edits']),'samePhaseCount':len(x['navigation'])==len(y['navigation']),'editedExportPixels':all(i['after']['image']['sha256']==j['after']['image']['sha256']for i,j in zip(x['edits'],y['edits'])),'navigationCanvasPixels':all(i['id']==j['id']and i['raw']['pixels']['sha256']==j['raw']['pixels']['sha256']for i,j in zip(x['navigation'],y['navigation'])),'finalExportPixels':x['final']['image']['sha256']==y['final']['image']['sha256']};assert all(v.values());pairs[a+'__'+c]=v
write(D/'paired-pixels.json',pairs)
source=bodies['on-dpr1']['sourceBefore'];source['native_fixture_binary_sha256']=sha(R/'out/host/axiom_g5_scale_fixture');source['creation_test_sha256']=sha(R/'verification/runtime_qualification/tests/creation_control_binding_test.cpp');source['platform']=platform.platform();source['machine']=platform.machine();source['note']='Native fixture/test rebuilt from the same content before code freeze;Web ON/OFF packages configured,built,installed after clean c05d3a2f commit. Binary hashes/logs are separate from the clean Web build stamp.';write(D/'source-inputs.json',source)
for name,bdir in [('on','out/web-sampling'),('off','out/web-sampling-off'),('native','out/host')]:
 lines=(R/bdir/'CMakeCache.txt').read_text().splitlines();write(D/('build-'+name+'.json'),{'directory':bdir,'settings':[x for x in lines if not x.startswith('//')and any(k in x for k in ['AXIOM_ENABLE_FRAME_TIMING:','AXIOM_RENDER_CACHE_MIB:','AXIOM_RENDER_SHARED_MIB:','AXIOM_PROTOC:','CMAKE_BUILD_TYPE:','AXIOM_SEMANTIC_RUNTIME_ROOT:','AXIOM_SKIA_SDK_ROOT:'])]})
m=load(S/'scale/matrix.json');total=load(S/'point-total/result.json');native=load(S/'native-comparison.json')
assert m['sourceStable']and len(m['rows'])==49 and total['sourceStable']and total['classification']=='MEASURED'
assert native['passed']==1010 and native['new_failed_names']==[]and len(native['resolved_names'])==4
workers=[load(p)for p in S.rglob('*.worker.json')];assert workers and all(w['cleanup_complete']for w in workers)
result={'status':'READY_FOR_CONTROL_REVIEW_WITH_FINDINGS','scope':'This test/tooling/engineering-observation slice only;no G5/P34/physical/all-platform PASS','officialGatePassClaimed':False,'codeRevision':source['commit'],'taskAnchor':'55e21707ccaa42cb5c2fef49de1265134fd6a996','productionRuntimeChanged':False,'sourceArtifactFrozen':True,'steps':{'creationFixtures':{'status':'VERIFIED','resolved':native['resolved_names']},'editedNavigation':{'status':'MEASURED','cleanSourceWorkers':len(nav),'editsPerWorker':8,'baseNavigationPhases':12,'additionalContextPhase':1,'pairedPixels':pairs},'coldRecovery':{'status':'OBSERVED_WITH_FINDINGS','defaultDpr2RestoreFirstReceiptMs':{s:nav[s]['restore']['firstReceiptWaitMs']for s in ['on-dpr2-restore','off-dpr2-restore']},'remaining':'Reproducible DPR4 idle selected pan submission gap:many short coverage preparation attempts do not present;DPR4 zoom also has finite CPU coverage cost. No accepted SLO or paired prior-version regression experiment;not declared repaired.'},'scale':{'status':m['classification'],'matrixRows':len(m['rows']),'measured':sum(x['classification']=='MEASURED'for x in m['rows']),'blocked':sum(x['classification']=='BLOCKED_EVIDENCE'for x in m['rows']),'documentTotalPoints':{'classification':total['classification'],'objects':10,'points':1000000,'visiblePointCentres':16000,'dpr':2,'scope':'ON diagnostic engineering observation;not single-stroke1M or1Mobjects'}}},'native':native,'otherChecks':{'g5Node':39,'boundaries':119,'python':11,'fullLogs':'logs/'},'workerContainment':{'workers':len(workers),'allCleanupComplete':True,'nativeRssMiB':2048,'browserFamilyRssMiB':3072,'timeoutSeconds':180,'rssInterpretation':'sum of process RSS;shared pages may be counted more than once'},'limits':{'snapshotRepeatedFieldOccurrences':{'value':65535,'source':'runtime/semantic/src/codec.cpp','scope':'one repeated wire field in one message;not universal Runtime document object maximum'},'snapshotObjectRecordBytes':{'value':16777216,'source':'runtime/semantic/src/codec.cpp'},'compactStrokeKnots':{'value':262144,'source':'runtime/semantic/include/canvas/semantic/compact_stroke.hpp'},'renderMiB':{'optional':32,'shared':96}},'unresolved':['DPR4 repeated idle navigation coverage wait and zoom coverage cost','100K/1Mobject root SDK tiers blocked by admission or containment','66existing native failed/not-built tests including G3 authority issue','Formal statistical,physical input,other platforms and Gate acceptance not performed'],'preserved':['4242Demo/tab/document','old G5/P34 records','Semantic/Skia locks','SDK0.4.3/API/ABI/budgets']}
write(D/'result.json',result)
print(json.dumps({'destination':str(D),'cases':len(nav),'workers':len(workers),'status':result['status']}))
