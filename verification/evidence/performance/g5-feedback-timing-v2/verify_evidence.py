#!/usr/bin/env python3
"""Evidence integrity and independent protocol checks;never a Runtime PASS."""
import gzip,hashlib,json,subprocess
from pathlib import Path
D=Path(__file__).resolve().parent

def read(name):
 p=D/name
 return json.loads(p.read_text())if p.exists()else json.loads(gzip.decompress(Path(str(p)+'.gz').read_bytes()))

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
m=read('manifest.json');inventory={str(p.relative_to(D))for p in D.rglob('*')if p.is_file()and p.name!='manifest.json'}
assert inventory==set(m['files'])
for n,h in m['files'].items():assert digest(D/n)==h,n
r=read('result.json');spec=read('design/verification-spec.json');matrix=read('pilot/matrix.json');analysis=read('analysis.json');rows=matrix['results']
assert r['methodStatus']=='READY_FOR_P31' and r['runtimeComparison']=='INCONCLUSIVE' and not r['officialGatePassClaimed']
assert r['productionCodeChanged'] is False and r['priorBlockedUnchanged'] is True
assert len({x['id']for x in rows})==32
assert spec['pilot']['workers']==len(rows)==32 and spec['successor_P31_protocol']['minimum_independent_blocks_per_case']==8
assert analysis['comparisonStatus']=='INCONCLUSIVE' and analysis['observedBlocksPerCase']==2
assert r['methodRevision']==analysis['methodRevision']
assert read('checks/g5-feedback-probe-qualified-final.json')['status']=='PASS'
for name,h in read('checks/g5-feedback-probe-qualified-final.json')['artifacts'].items():assert digest(D/'method'/name)==h
assert read('checks/g5-feedback-startup-green.json')['status']=='PASS'
assert read('checks/g5-feedback-startup-red.json')['status']=='FAIL'
for log,needle in [('node.log','# tests 39'),('node.log','# fail 0'),('python.log','Ran 16 tests'),('python.log','OK'),('boundaries.log','passed (118 contract files)')]:
 assert needle in gzip.decompress((D/'checks'/(log+'.gz')).read_bytes()).decode(),log
assert read('failed-pilot/matrix.json')['results'][0]['status']=='BLOCKED_EVIDENCE'
assert 'probe is not defined'in read('failed-pilot/AA-pan-0-0-A/summary.json')['results'][0]['error']
artifacts={};pixels={};run_endpoints={}
for row in rows:
 name=row['id'];s=read('pilot/'+name+'/summary.json');raw=read('pilot/'+name+'/wheel.json');w=read('pilot/'+name+'-worker.worker.json')
 assert s['status']=='MEASURED'and s['sourceStable']and s['artifactStable']and s['sourceBefore']['commit']==r['methodRevision']and not s['sourceBefore']['dirty']
 assert s['sourceBefore']==s['sourceAfter']
 assert s['observationVersion']==2 and s['modes']==['wheel'] and s['selected'] is False
 assert raw['setup']['build']['sourceRevision']==(spec['pilot']['baseline_source']if row['label']=='A'else spec['pilot']['candidate_source'])
 assert not raw['setup']['build']['dirty']and all(raw['checks'].values())and raw['setupQuiescence']['stopped']
 assert raw['panOnly']==(row['case']=='pan')
 assert raw['options']=={'width':636,'height':923,'points':2048,'lines':5,'span':570,'lineSpacing':65,'uiReads':True,'shapes':2,'scribble':True,'scribblePhase':.63,'zoom':1.59,'drawLast':True}
 if row['label']in artifacts:assert artifacts[row['label']]==s['artifacts']
 artifacts[row['label']]=s['artifacts']
 assert w['reason']=='completed'and w['returncode']==0 and w['cleanup_complete']and not w['remaining_owned_pids']
 assert w['elapsed_s']<=w['timeout_s']==180 and w['peak_process_family_rss_kib']<=w['rss_limit_kib']==3145728
 assert [p['id']for p in raw['phases']]==['repeat-0','repeat-1','after-idle','after-freeze']
 vals=[]
 for phase,summary in zip(raw['phases'],s['results'][0]['phases']):
  assert phase['summary']==summary['summary']
  q=phase['raw'];f=phase['summary']['feedbackV2'];n=phase['summary']['runtimeSubmitV2']
  assert len(q['inputEvents'])==f['eventCount']==288 and f['status']==n['status']=='OBSERVED'
  assert q['quiescence']['pending']==0 and q['quiescence']['quietMs']==200
  assert f['firstInputAt']==q['inputEvents'][0]['started']and f['lastInputAt']==q['inputEvents'][-1]['started']
  assert f['preInputWaitMs']==f['firstInputAt']-q['started']
  stamps=[x['at']for x in q['receipts']if x['at']>=f['lastInputAt']]
  assert f['lastInputToLastCoarseReceiptMs']==max(stamps)-f['lastInputAt']
  assert all(q['checks'][k]for k in ['documentUnchanged','selectionUnchanged','operationUnchanged','cameraReturned'])
  pixel_key=(row['case'],phase['id'])
  if pixel_key in pixels:assert pixels[pixel_key]==q['pixels']['sha256']
  pixels[pixel_key]=q['pixels']['sha256']
  p=next(p for p in analysis['phases']if p['run']==name and p['phase']==phase['id'])
  assert p['firstAcceptedMs']==n['firstAcceptedSubmitMs']and p['steadyAcceptedGapMs']==n['steadyAcceptedGapMs']['max']
  assert p['lastEventToLastCoarseReceiptMs']==f['lastInputToLastCoarseReceiptMs']
  assert p['lowDensity']==phase['summary']['navigation']['quality']['timeWeightedLowResolutionFraction']
  vals.append(p)
 run_endpoints[name]={k:max(p[k]for p in vals)for k in ['firstAcceptedMs','steadyAcceptedGapMs','lastEventToLastCoarseReceiptMs','lowDensity']}
 # Validate original raw-byte hashes despite durable gzip packaging.
 for file,h in read('pilot/'+name+'/sha256.json').items():
  p=D/'pilot'/name/file
  content=p.read_bytes()if p.exists()else gzip.decompress(Path(str(p)+'.gz').read_bytes())
  assert hashlib.sha256(content).hexdigest()==h
assert len(analysis['phases'])==128 and len(analysis['blocks'])==8
for block in analysis['blocks']:
 runs=sorted([x for x in rows if(x['case'],x['kind'],x['block'])==(block['case'],block['kind'],block['block'])],key=lambda x:x['position'])
 assert [x['label']for x in runs]==list('AAAA'if block['kind']=='AA'else'ABBA')
 assert len(runs)==4
 for key,e in block['endpoints'].items():
  values=[run_endpoints[x['id']][key]for x in runs];a=(values[0]+values[3])/2;b=(values[1]+values[2])/2
  assert e['values']==values and e['baseline']==a and e['candidate']==b and e['effect']==b-a
  assert e['margin']==(.02 if key=='lowDensity'else max(16.7,a*.1))and e['centeredEffect']==b-a-e['margin']
repo=D.parents[3]
if (repo/'.git').exists():
 for n,h in spec['inputs'].items():assert hashlib.sha256(subprocess.check_output(['git','show',spec['anchor']+':'+n],cwd=repo)).hexdigest()==h,n
 for n,h in read('provenance.json')['methodFiles'].items():
  assert digest(D/'method'/Path(n).name)==h
  assert hashlib.sha256(subprocess.check_output(['git','show',r['methodRevision']+':'+n],cwd=repo)).hexdigest()==h
 subprocess.run(['git','diff','--exit-code',spec['anchor'],r['methodRevision'],'--','runtime','sdk','apps','debug_ui','tools','CMakeLists.txt'],cwd=repo,check=True,stdout=subprocess.PIPE)
 assert digest(repo/'verification/evidence/performance/g5-retirement-arbitration/manifest.json')==read('legacy-audit.json')['sourceManifestSha256']
 old=repo/'verification/evidence/performance/g5-retirement-arbitration/result.json'
 assert digest(old)==spec['inputs']['verification/evidence/performance/g5-retirement-arbitration/result.json']
 assert json.loads(old.read_text())['status']=='BLOCKED'
subprocess.run(['node',str(D/'method/recompute_metrics.mjs'),str(D/'pilot')],check=True)
print('Evidence verified:',len(rows),'clean bounded workers;128phases;method READY_FOR_P31;Runtime INCONCLUSIVE;previous BLOCKED unchanged')
