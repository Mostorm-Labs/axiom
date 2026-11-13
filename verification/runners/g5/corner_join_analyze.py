"""Actual Web payload/history/pixel witnesses; native shape oracle is separate."""
from pathlib import Path
import json,gzip,hashlib,struct,importlib.util,collections,math
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'out/G5-corner-join-repair-r1';RES=ROOT/'.aegis/results/G5-corner-join-repair-r1'
s=importlib.util.spec_from_file_location('wire',ROOT/'verification/runners/g5/idle_input_feedback_analyze.py');w=importlib.util.module_from_spec(s);s.loader.exec_module(w)
sha=lambda b:hashlib.sha256(b).hexdigest()
def main():
 d=OUT/'web/CJ-WEB';q=json.loads((d/'raw.json').read_text());snap={n:gzip.decompress((d/(n+'.axsnap.gz')).read_bytes())for n in ['round','marker','full','erased','undo','redo','restored']};obj={n:w.objects(b)for n,b in snap.items()}
 assert obj['full']==obj['undo'] and obj['erased']==obj['redo']==obj['restored']
 def body(o):return[(f,v)for f,v in w.fields(o)if f!=8]
 for k,v in obj['full'].items():assert body(v)==body(obj['erased'][k])
 oldMasks=sum(sum(f==8 for f,v in w.fields(o))for o in obj['full'].values());newMasks=sum(sum(f==8 for f,v in w.fields(o))for o in obj['erased'].values());assert newMasks>oldMasks
 operations=[]
 for i,p in enumerate(q['operations']):
  raw=gzip.decompress((d/f'operation-{i}.bin.gz').read_bytes());kind,value=list(w.fields(w.first(raw,5)))[0];oid=w.idhex(w.first(raw,1));assert oid==p['state']['lastCommit']['operationId'];operations.append({'id':oid,'kind':kind,'sha256':sha(raw)})
 assert [o['kind']for o in operations]==[10,10,12,13,12]
 strokes=[]
 for key,o in obj['full'].items():
  compact=w.first(w.first(o,7),14);cfg=dict(w.fields(w.first(compact,1)));points=list(struct.iter_unpack('<dd',w.first(compact,4)));pressures=struct.unpack('<'+'H'*(len(w.first(compact,5))//2),w.first(compact,5));family=cfg[5];sharp=[]
  for i in range(1,len(points)-1):
   a,b,c=points[i-1:i+2];u=(b[0]-a[0],b[1]-a[1]);v=(c[0]-b[0],c[1]-b[1]);dot=u[0]*v[0]+u[1]*v[1]
   if dot<0:sharp.append({'index':i,'center':b,'pressure':pressures[i]})
  assert sharp;strokes.append({'id':key,'family':family,'algorithm':cfg[2],'knot_count':len(points),'position_sha256':sha(w.first(compact,4)),'pressure_sha256':sha(w.first(compact,5)),'config_sha256':sha(w.first(compact,1)),'sharp_knots':sharp})
 pixels=[]
 for p in q['pixels']:
  targets=[]
  for c in p['values']:
   b=gzip.decompress((d/c['file']).read_bytes());assert sha(b)==c['sha256'];width=c['metadata']['canvas']['width'];height=c['metadata']['canvas']['height'];amber=sum(1 for i in range(0,len(b),4)if b[i]>100 and b[i+1]>50 and b[i+2]<60 and b[i+3]>30)
   targets.append({'role':c['metadata']['role'],'visible':c['visible'],'amber_pixels':amber,'sha256':sha(b),'file':c['file'],'width':width,'height':height,'target_qualification':c['metadata']['status']})
  if p['label'].endswith('preview'):assert targets[1]['visible']=='visible'and targets[1]['amber_pixels']>0
  pixels.append({'label':p['label'],'targets':targets})
 # Same restored canonical objects/masks should reproduce actual device pixels.
 pix=lambda label:gzip.decompress((d/next(x for x in q['pixels']if x['label']==label)['values'][0]['file']).read_bytes())
 a,b=pix('redo'),pix('restored');changed=[i for i in range(len(a))if a[i]!=b[i]];edge=(OUT/'redo-oracle.edges').read_bytes();assert all(edge[i//4]for i in changed);restore_pixel_note={'changed_channels':len(changed),'max_channel_delta':max((abs(a[i]-b[i])for i in changed),default=0),'all_changes_on_independently_projected_boundary_band':True,'exact':not changed}
 result={'status':'ACTUAL_ROOT_WEB_VERIFIED','SDK_build':q['build'],'operations':operations,'strokes':strokes,'snapshots':{n:{'objects':len(obj[n]),'sha256':sha(v)}for n,v in snap.items()},'original_spine_config_pressure_seed_unchanged_by_mask_history':True,'Undo_restores_original_objects':True,'Redo_restore_objects_masks_equal':True,'restored_device_pixels':restore_pixel_note,'actual_corner_ROI':json.loads((OUT/'web-corner-roi.json').read_text()),'CPU_Web_parity':json.loads((OUT/'cpu-web-parity.json').read_text()),'new_masks':newMasks-oldMasks,'pixels':pixels,'root_errors':q['rootErrors'],'physical_platform_qualification':False,'limitations':['nativeindependentconstantpressure/markerellipseoracleprovesalgorithm; browseractualspineisfiltered/mousesimulatedpressure','WebfirstpreviewislatefinitecapturebeforeUp, notfirstphoton','GLerrorqueuenotconsumed; targetidentity/staterestoreverified only','v4wrongsharpderivedpixelsqueriesallowedchange; payloadnotmigration','ordinarycorrectnessnotperformancePASS; nofullscene40objectoracle']}
 (RES/'web-observation.json').write_text(json.dumps(result,indent=2)+'\n');print({'web':'VERIFIED','spines':len(strokes),'masks':newMasks-oldMasks,'operation_kinds':[v['kind']for v in operations],'restored_pixels':restore_pixel_note})
if __name__=='__main__':main()
