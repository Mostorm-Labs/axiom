from pathlib import Path
import json,hashlib,subprocess,importlib.util,math,time,sys,gzip
R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');O=R/'out/G5-exposure-region-reuse-r1';E=R/'.aegis/results/G5-exposure-region-reuse-r1';P=R/'.aegis/packages/G5-exposure-region-reuse-r1';h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def put(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def ref(p):return{'path':str(p),'bytes':p.stat().st_size,'sha256':h(p)}
def freeze():
 assert not(E/'sampling-ledger.json').exists()
 fixture=read(R/'.aegis/results/G5-CPUW4-edit-navigation-r1/local-web/CPUW4-D4-CORRECTIVE/fixture.json');work=read(P/'workload.json')
 manifests={'normalB':R/'.aegis/results/G5-quality-candidate-continuation-r1/normal-SDK-C.json','normalC':E/'normal-SDK-C.json','opticalB':E/'optical-SDK-B.json','opticalC':E/'optical-SDK-C.json'}
 for m in manifests.values():
  data=read(m);assert all(h(Path(data['root'])/n)==value for n,value in data['files'].items())
 files=[R/'verification/runners/g5'/n for n in['exposure_region_reuse_matrix.py','exposure_region_reuse_web.mjs','exposure_region_reuse_consumer.mjs','exposure_region_reuse_analyze.mjs','navigation_observer.mjs','webgl_target_readback.mjs','continuous_zoom_quality_contract.mjs','heavy_scene_attribution_store.mjs','exact_fixed_scene_contract.mjs','bounded_worker.py']]+[P/'metrics-contract.json',P/'workload.json',E/'optical-oracle.json']
 rows=[];(O/'requests').mkdir(exist_ok=True)
 for q in work['ordinary_windows']+work['optical_windows']:
  lane='B'if q['lane']=='baseline'else'C';manifest=manifests[('normal'if q['mode']=='ordinary'else'optical')+lane];m=read(manifest)
  request={**q,'sdk':m['root'],'image':str(R/'.aegis/results/G5-object-interaction-r2-image-move-diagnostic/image-2_3mb.png'),'fixture':{'path':fixture['path'],'metadata':fixture['metadata'],'documentId':fixture['documentId']}}
  p=O/'requests'/(q['id']+'.json');put(p,request);rows.append({**q,'SDK_manifest':ref(manifest),'request':ref(p)})
 put(E/'browser-freeze.json',{'requests':rows,'drivers':[ref(p)for p in files],'SDKs':{k:ref(v)for k,v in manifests.items()},'sources':[ref(E/n)for n in['normal-source-C.json','optical-source-B.json','optical-source-C.json']],'before_first_formal_window':True,'ordinaryMacrosOFF':True,'optical_current_display_immediate':True,'no_metric_fitting_or_retry':True})
 put(E/'sampling-ledger.json',{'attempts':[],'charged_seconds':0,'cap_seconds':1560,'ordinaryStartsMax':8,'opticalStartsMax':4,'remaining':[q['id']for q in rows]})
def verify(f):
 for v in f['drivers']+list(f['SDKs'].values())+f['sources']:assert ref(Path(v['path']))==v,v['path']
 for m in f['SDKs'].values():
  sdk=read(Path(m['path']));assert all(h(Path(sdk['root'])/n)==v for n,v in sdk['files'].items())
def run():
 f=read(E/'browser-freeze.json');l=read(E/'sampling-ledger.json');assert not l.get('stop');sp=importlib.util.spec_from_file_location('erguard',R/'verification/runners/g5/bounded_worker.py');g=importlib.util.module_from_spec(sp);sp.loader.exec_module(g)
 for q in f['requests']:
  if q['id']not in l['remaining']:continue
  verify(f);assert l['charged_seconds']<1560;dest=O/'web'/q['id'];dest.mkdir(parents=True,exist_ok=False);entry={'id':q['id'],'mode':q['mode'],'DPR':q['DPR'],'lane':q['lane'],'status':'STARTED','at_unix':time.time(),'request':q['request']};l['attempts'].append(entry);l['remaining'].remove(q['id']);put(E/'sampling-ledger.json',l)
  guard=g.run(['node',str(R/'verification/runners/g5/exposure_region_reuse_web.mjs'),q['request']['path'],str(dest)],dest/'process',min(120,1560-l['charged_seconds']),6144*1024);entry['guard']=guard;entry['charged_seconds']=math.ceil(guard['elapsed_s']);l['charged_seconds']+=entry['charged_seconds'];error=None
  try:
   verify(f);assert guard['returncode']==0 and guard['reason']=='completed'and guard['cleanup_complete'],'PROCESS_GUARD_OR_ENVIRONMENT_FAILURE'
   raw=read(dest/'raw.json');assert raw['status']=='OBSERVED'and raw['closed']and not raw['errors']and len(raw['routes'])==2,'STATE_OR_CLEANUP_FAILURE';assert all(len(r['inputs'])==360 for r in raw['routes']),'INPUT_LOSS'
   assert gzip.decompress((dest/'final.axsnap.gz').read_bytes())==Path(read(Path(q['request']['path']))['fixture']['path']).read_bytes(),'SNAPSHOT_MISMATCH'
   analyzed=subprocess.run(['node',str(R/'verification/runners/g5/exposure_region_reuse_analyze.mjs'),str(dest/'raw.json')],cwd=R,capture_output=True,text=True,check=True);metrics=json.loads(analyzed.stdout);put(dest/'metrics.json',metrics);entry['metrics']=ref(dest/'metrics.json')
   if q['lane']=='candidate'and q['mode']=='ordinary':assert all(m['proxy']['gaps']['maximum']<=100 and all(b['gap_ms']<=100 for b in m['proxy']['direction_boundaries'])for m in metrics),'FROZEN_ABSOLUTE_ORDINARY_GAP_FAILURE'
   if q['lane']=='candidate'and q['mode']=='optical':
    assert all(m['camera']and m['camera']['pass']for m in metrics),'FROZEN_ACTUAL_CAMERA_HANDOFF_FAILURE'
    assert all(len(r['optical'])==12 for r in raw['routes']),'MISSING_OPTICAL_CHECKPOINT'
   entry['status']='COMPLETED'
  except Exception as e:error=repr(e);entry['status']='FAILED';entry['error']=error;l['stop']={'id':q['id'],'reason':error,'no_more_performance_sampling':True}
  put(E/'sampling-ledger.json',l);print(json.dumps(entry),flush=True)
  if error:raise RuntimeError(error)
if __name__=='__main__':freeze()if sys.argv[1]=='freeze'else run()
