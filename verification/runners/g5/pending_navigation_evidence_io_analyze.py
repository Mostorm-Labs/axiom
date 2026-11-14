"""Audit four supplemental rows and join existing parent pixel references offline."""
from pathlib import Path
import base64
import gzip
import hashlib
import importlib.util
import json

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'out/G5-pending-navigation-evidence-io-repair-r1'
RES=ROOT/'.aegis/results/G5-pending-navigation-evidence-io-repair-r1'
BASE=OUT/'iab/PNIO-IAB'
spec=importlib.util.spec_from_file_location('wire',ROOT/'verification/runners/g5/idle_input_feedback_analyze.py')
wire=importlib.util.module_from_spec(spec);spec.loader.exec_module(wire)
def sha(b):return hashlib.sha256(b).hexdigest()
def data(value):
 if 'buffer'in value:
  info=value['buffer'];compressed=(BASE/info['path']).read_bytes();raw=gzip.decompress(compressed)
  assert len(raw)==info['bytes']and sha(raw)==info['sha256']and sha(compressed)==info['compressed_sha256']
  return raw
 return base64.b64decode(value['base64'])
def body(record):return [(f,v)for f,v in wire.fields(record)if f!=8]
def erase_pending(value):return any(v['kind']=='erase'for v in value['intents'])
def amber(buffer,width):
 xs=[];ys=[]
 for i in range(0,len(buffer),4):
  if buffer[i]>100 and buffer[i+1]>50 and buffer[i+2]<60 and buffer[i+3]>30:
   xs.append((i//4)%width);ys.append(i//4//width)
 return {'count':len(xs),'bounds_device':[min(xs),min(ys),max(xs),max(ys)]if xs else None}
def main():
 index=json.loads((BASE/'index.json').read_text());assert index['complete']and len(index['rows'])==4
 rows={}
 for entry in index['rows']:
  path=BASE/entry['path'];assert sha(path.read_bytes())==entry['sha256'];assert 'base64'not in path.read_text()
  rows[entry['key']]=json.loads(path.read_text())
 parentpath=ROOT/'out/G5-pending-navigation-owner-progress-r1/iab/PN-IAB/raw.json'
 parent=json.loads(parentpath.read_text())
 for row in parent['rows']:
  if row['q']['pixel']:rows[row['q']['lane']+'-'+row['q']['action']]=row
 audit=[];all_buffers=0
 for key,row in rows.items():
  assert row['status']=='OBSERVED'and row['closed']and not row['errors']and row['pixelRAFRestored']
  before,after=data(row['snapshots']['before-erase']),data(row['snapshots']['final'])
  old,new=wire.objects(before),wire.objects(after);assert all(body(v)==body(new[k])for k,v in old.items())
  assert len(old)==3 and len(new)==(4 if key.endswith('nextdraw')else 3)
  maskgeometry={k:[sha(wire.first(v,2))for f,v in wire.fields(new[k])if f==8]for k in old}
  assert all(len(v)==1 for v in maskgeometry.values())
  operations=[]
  for op in row['operations']:
   raw=data(op);kind,payload=list(wire.fields(wire.first(raw,5)))[0];operations.append({'kind':kind,'sha256':sha(raw)})
  assert [v['kind']for v in operations]==([12,10]if key.endswith('nextdraw')else[12])
  entry={'key':key,'objects':[len(old),len(new)],'old_body_config_pressure_transform_preserved':True,
   'mask_geometry':maskgeometry,'operations':operations,'final_camera':row['end']['camera'],
   'actual_erase_pending_at_input':erase_pending(row.get('atNavigation',row.get('atNextDown'))['state']['pending']),
   'samples':[],'root_errors':row['errors'],'observer_lost':row['pendingObservation']['lost']}
  assert entry['actual_erase_pending_at_input']
  before_pixels=data(row['pixels'][0]);finalpixels=data(next(p for p in row['pixels']if p['label']=='final'))
  for pixel in row['pixels']:
   preview=pixel['preview'];assert pixel['metadata']['status']=='VALID'and preview['metadata']['status']=='VALID'
   assert pixel['metadata']['restoration']['status']=='RESTORED'and preview['metadata']['restoration']['status']=='RESTORED'
   assert not pixel['metadata']['context']['lost']and not preview['metadata']['context']['lost']
   canonical=data(pixel);overlay=data(preview);assert len(canonical)==len(overlay)==2400*1600*4
   all_buffers+=2
   sample={'label':pixel['label'],'camera':pixel['camera'],'erase_pending':erase_pending(pixel['pending']),
    'canonical_sha256':sha(canonical),'same_as_old_camera_pixels':canonical==before_pixels,
    'same_as_final_pixels':canonical==finalpixels,'preview_visibility':preview['visibility'],
    'preview_amber':amber(overlay,2400),'target_status':'VALID/restored/no context loss'}
   if pixel['label']=='final':assert not pixel['pending']['intents']and preview['visibility']=='hidden'
   entry['samples'].append(sample)
  entry['final_RGBA_sha256']=sha(finalpixels)
  if key.endswith('nextdraw'):
   samples={v['label']:v for v in entry['samples']}
   first=samples['opportunity-1'];middle=samples['nextdraw-move8'];late=samples['nextdraw-move16']
   assert first['erase_pending']and first['preview_visibility']=='visible'and first['preview_amber']['count']>0
   assert middle['preview_visibility']==late['preview_visibility']=='visible'
   assert late['preview_amber']['bounds_device'][2]>middle['preview_amber']['bounds_device'][2]>first['preview_amber']['bounds_device'][2]
   for sample in [first,middle,late]:
    left,top,right,bottom=sample['preview_amber']['bounds_device']
    assert 1550<=left<=1650 and 930<=top<=1050 and 1600<=right<=1840 and 970<=bottom<=1070
   entry['nextdraw_feedback']='Visible nonempty amber on actualpreview target whileerasepending; move8/16 rightboundary advances along frozeninputtrajectory; finalhidden aftercanonical. Finite captures, nofirstphoton.'
  audit.append(entry)
 comparisons=[]
 for action in ['zoom','pan','nextdraw']:
  baseline=next(r for r in audit if r['key']=='baseline-'+action);candidate=next(r for r in audit if r['key']=='candidate-'+action)
  assert baseline['mask_geometry']==candidate['mask_geometry']and baseline['final_camera']==candidate['final_camera']
  assert baseline['final_RGBA_sha256']==candidate['final_RGBA_sha256']
  value={'action':action,'all_final_RGBA_exact_baseline':True,'all_original_body_and_mask_geometry_equal':True,'final_camera_equal':True}
  if action!='nextdraw':
   pixel=next(v for v in rows['candidate-'+action]['pixels']if v['label']=='final');rgba=data(pixel)
   cpu=(OUT/'offline-oracle'/('candidate-'+action+'.rgba')).read_bytes();edge=(OUT/'offline-oracle'/('candidate-'+action+'.edges')).read_bytes()
   assert len(cpu)==len(rgba)
   outside=sum(a!=b and not edge[i//4]for i,(a,b)in enumerate(zip(cpu,rgba)))
   assert outside==0
   value['independent_CPU_nonedge_exact_channels']=True;value['CPU_reference_sha256']=sha(cpu);value['source_only_fixed_boundary_sha256']=sha(edge)
   pending_samples=[v for v in candidate['samples']if v['label'].startswith('opportunity')and v['erase_pending']]
   value['Web_current_camera_while_pending_pixels_captured']=any(not v['same_as_old_camera_pixels']for v in pending_samples)
   value['native_actual_pending_camera_oracle']='Parent actualholdWorker Composition fullsurfacepan-equivalent pixelGREEN7camera modes; unchanged productionhash.'
  comparisons.append(value)
 result={'schema':'axiom-pending-navigation-IO-pixel-combined-v1','supplement_rows':4,'parent_pixel_reused':2,
  'all_readback_buffers_verified':all_buffers,'comparisons':comparisons,'rows':audit,
  'limitations':['Candidate zoom/pan finitefirst3Webreadbacks didnotcapturecurrentcamera image whileerasepending: earlierreadbacksoldframe andlateraftercommit. NativeholdkernelactualCompositionfullframeprovesPN1; keepthisWebcoveragegap explicit.',
   'Fullcanonical+previewreadbacksadd~600msperopportunity andalterworker/ownerchronology. No timingsfromthissupplementusedasperformance.',
   'AllfinalRGBAexactbaseline; Nextdraw independentcompact-onlyboundaryoracle notapplicable todefaultvectorcommand (bad_variant_access capturedoffline); no productcrash. Finalbaselineexact+actualpreviewtrajectory+Semanticwire/bodyproof used.',
   'NewOperation/maskidentitiesrandomperRuntime; geometrysame. Exactsameinputencodedwire/storeproof isparentSemanticnative.']}
 (RES/'pixel-semantic-observation.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps({'rows':4,'parentpixelreuse':2,'finalexact':True,'comparisons':comparisons},indent=2))
if __name__=='__main__':main()
