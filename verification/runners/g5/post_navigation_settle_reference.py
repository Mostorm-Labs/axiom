"""Serial pinned CPU oracle at actual captured pose; no browser starts/retries."""
from pathlib import Path
import json,gzip,hashlib,subprocess,time,math,importlib.util,sys
import numpy as np
R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');O=R/'out/G5-post-navigation-settle-r1';E=R/'.aegis/results/G5-post-navigation-settle-r1';D=O/'references';D.mkdir(exist_ok=True)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();put=lambda p,v:p.write_text(json.dumps(v,indent=2)+'\n')
frozen=json.loads((R/'.aegis/packages/G5-post-navigation-settle-r1/fixed-inputs.json').read_text());binary=Path(frozen['pinned_CPU_reference_binary']['path']);assert h(binary)==frozen['pinned_CPU_reference_binary']['sha256']
profilePath=Path(frozen['pinned_optical_profile_method']['path']);assert h(profilePath)==frozen['pinned_optical_profile_method']['sha256'];spec=importlib.util.spec_from_file_location('pinnedOpticalProfile',profilePath);profile=importlib.util.module_from_spec(spec);spec.loader.exec_module(profile)
fixture=json.loads((R/frozen['fixture']['path']).read_text());snapshot=Path(fixture['path']);image=R/frozen['image']['path'];resource=fixture['resources']['entries'][0]['resourceId']
old=R/'out/G5-quality-candidate-continuation-r1/optical-reference';oldLedger=json.loads((R/'.aegis/results/G5-quality-candidate-continuation-r1/optical-reference-ledger.json').read_text());oldComplete={v['key']for v in oldLedger['starts']if v['status']=='COMPLETED'and v['exit']==0};oldIndex={json.dumps(json.loads(p.read_text()),sort_keys=True):p for p in old.glob('*.request.json')}
ledgerPath=E/'reference-ledger.json';ledger=json.loads(ledgerPath.read_text())if ledgerPath.exists()else{'oracleBinary':{'path':str(binary),'sha256':h(binary)},'newUniqueMax':128,'perStartSeconds':60,'aggregateCapSeconds':1800,'chargedSeconds':0,'attempts':[],'reused':[],'singleProcess':True,'noBrowserStarts':True};rows=[]
def reference(stamp,needEdge=False):
 a=stamp['affine'];w,height=stamp['dimensions'];request={'snapshot':str(snapshot),'operations':[],'width':w,'height':height,'logical_width':1431,'logical_height':1010,'camera':{'scale':a[0],'x':a[4],'y':a[5]},'resources':[{'id':resource,'path':str(image)}]};basis={'request':request,'binary':h(binary),'snapshotSHA':h(snapshot),'imageSHA':h(image),'profileSHA':h(profilePath),'boundarySHA':frozen['pinned_CPU_reference_boundary']['sha256']};key=hashlib.sha256(json.dumps(basis,sort_keys=True).encode()).hexdigest()[:24];dest=D/key;rgba=D/(key+'.rgba.gz');edge=D/(key+'.edge.gz');req=D/(key+'.request.json')
 if rgba.exists()and(not needEdge or edge.exists()):return key,rgba,edge if edge.exists()else None
 prior=oldIndex.get(json.dumps(request,sort_keys=True));priorKey=prior.name.removesuffix('.request.json')if prior else None
 if not needEdge and prior and priorKey in oldComplete and(old/(priorKey+'.rgba.gz')).exists():
  src=old/(priorKey+'.rgba.gz');b=gzip.decompress(src.read_bytes());assert len(b)==w*height*4
  rgba.write_bytes(src.read_bytes());put(req,request);ledger['reused'].append({'key':key,'priorKey':priorKey,'request':str(prior),'requestSHA':h(prior),'rgba':str(src),'rgbaSHA':h(src),'basis':basis});put(ledgerPath,ledger);return key,rgba,None
 assert len(ledger['attempts'])<128 and ledger['chargedSeconds']<1800,'OFFLINE_REFERENCE_POOL_EXHAUSTED'
 assert not any(v['key']==key and v['status']!='COMPLETED'for v in ledger['attempts']),'NO_ORACLE_RETRY'
 put(req,request);entry={'key':key,'status':'STARTED','basis':basis,'atUnix':time.time()};ledger['attempts'].append(entry);put(ledgerPath,ledger);start=time.monotonic();reason='completed';code=None
 try:
  with(D/(key+'.log')).open('w')as f:p=subprocess.run([str(binary),str(req),str(dest)],cwd=R,stdout=f,stderr=subprocess.STDOUT,timeout=min(60,1800-ledger['chargedSeconds']));code=p.returncode
 except subprocess.TimeoutExpired:reason='timeout'
 seconds=time.monotonic()-start;entry.update(seconds=seconds,chargedSeconds=math.ceil(seconds),exit=code,status='COMPLETED'if code==0 else'FAILED',reason=reason);ledger['chargedSeconds']+=entry['chargedSeconds'];put(ledgerPath,ledger)
 if code!=0:raise RuntimeError('PINNED_REFERENCE_FAILURE '+key)
 for suffix in ['rgba','edge','hud','axsnap']:
  src=Path(str(dest)+'.'+suffix)
  if src.exists():b=src.read_bytes();(D/(key+'.'+suffix+'.gz')).write_bytes(gzip.compress(b,compresslevel=3,mtime=0));src.unlink()
 entry['artifacts']={s:{'path':str(D/(key+'.'+s+'.gz')),'bytes':(D/(key+'.'+s+'.gz')).stat().st_size,'sha256':h(D/(key+'.'+s+'.gz'))}for s in['rgba','edge','hud','axsnap']if(D/(key+'.'+s+'.gz')).exists()};put(ledgerPath,ledger);print('reference',key,'seconds',round(seconds,2),'charged',ledger['chargedSeconds'],flush=True);return key,rgba,edge
jobs=[]
for case in ['WH-OPT-B-D2','WH-OPT-C-D2','WH-OPT-C-D4']:
 raw=json.loads((O/'web'/case/'raw.json').read_text())
 for route in raw['routes']:
  for cap in route['optical']:jobs.append((case,route['route'],cap,False))
  meta=json.loads((O/'web'/case/(route['route']+'-final.json')).read_text());jobs.append((case,route['route'],meta,True))
for case,route,cap,final in jobs:
 try:
  stamp=cap['stamp'];key,ref,edge=reference(stamp,final);w,height=stamp['dimensions'];actualBytes=gzip.decompress((O/'web'/case/(cap['name']+'.rgba.gz')).read_bytes());expectedBytes=gzip.decompress(ref.read_bytes());assert len(actualBytes)==len(expectedBytes)==w*height*4
  actual=np.frombuffer(actualBytes,dtype=np.uint8).reshape(height,w,4);expected=np.frombuffer(expectedBytes,dtype=np.uint8).reshape(height,w,4)
  if final:
   assert edge;band=np.frombuffer(gzip.decompress(edge.read_bytes()),dtype=np.uint8).reshape(height,w);delta=np.abs(actual.astype(np.int16)-expected.astype(np.int16));interior=(delta.max(axis=2)>2)&(band==0);witness=np.argwhere(interior)[:12];row={'case':case,'route':route,'name':cap['name'],'finalSamePose':True,'referenceKey':key,'nonAAFailedPixels':int(interior.sum()),'changedChannels':int(np.count_nonzero(delta)),'maxChannelDifference':int(delta.max()),'witnesses':[{'x':int(x),'y':int(y),'actual':actual[y,x].tolist(),'expected':expected[y,x].tolist()}for y,x in witness],'status':'PASS'if not interior.any()else'PIXEL_FAILURE','knownDPR4ImageIssue':'reported separately, noautomaticwaiver'}
  else:
   edges=profile.evaluate(expected,actual,stamp);finite=[e for e in edges if e['spread_excess']is not None];dest=O/'optical-profiles'/case;dest.mkdir(parents=True,exist_ok=True);out=dest/(cap['name']+'.json.gz');putdata={'capture':cap,'reference_key':key,'edges':edges,'eligible':len(edges),'no_candidate_dependent_eligibility':True};out.write_bytes(gzip.compress(json.dumps(putdata,separators=(',',':')).encode(),compresslevel=5,mtime=0));import statistics
   row={'case':case,'route':route,'name':cap['name'],'index':cap['index'],'referenceKey':key,'eligible':len(edges),'finite':len(finite),'medianExcess':statistics.median(e['spread_excess']for e in finite)if finite else None,'medianContrast':statistics.median(e['contrast_error']for e in finite)if finite else None,'currentCapture':cap['current'],'status':'MEASURED'if finite else'UNAVAILABLE','profiles':str(out),'reason':None if finite else'NO_ELIGIBLE_FINITE_REFERENCE_EDGES'}
  rows.append(row);put(E/'reference-pixels-and-profiles.json',{'rows':rows,'ledger':str(ledgerPath),'allCurrentPoseReferences':True,'noOracleRetries':True});print(case,cap['name'],row['status'],flush=True)
 except Exception as exc:
  rows.append({'case':case,'route':route,'name':cap['name'],'status':'BLOCKED_REFERENCE','error':repr(exc)});put(E/'reference-pixels-and-profiles.json',{'rows':rows,'ledger':str(ledgerPath),'blocker':repr(exc),'unexecuted':len(jobs)-len(rows)});raise
put(E/'reference-manifest.json',{'oracleBinary':{'path':str(binary),'sha256':h(binary)},'profile':{'path':str(profilePath),'sha256':h(profilePath)},'rows':rows,'ledger':{'path':str(ledgerPath),'sha256':h(ledgerPath)},'sources':frozen['pinned_CPU_reference_source'],'boundary':frozen['pinned_CPU_reference_boundary']});print('DONE',len(rows),len(ledger['attempts']),ledger['chargedSeconds'],flush=True)
