"""Offline analysis of the frozen finite pending navigation runs; no browser.

Missing IAB rows remain missing. Snapshot field inspection corroborates actual
SemanticCodec restore/capture and does not create another codec authority.
"""
from pathlib import Path
import base64
import hashlib
import importlib.util
import json
import statistics

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'out/G5-pending-navigation-owner-progress-r1'
RES=ROOT/'.aegis/results/G5-pending-navigation-owner-progress-r1'
spec=importlib.util.spec_from_file_location('wire',ROOT/'verification/runners/g5/idle_input_feedback_analyze.py')
wire=importlib.util.module_from_spec(spec)
spec.loader.exec_module(wire)

def sha(b):return hashlib.sha256(b).hexdigest()
def binary(v):return base64.b64decode(v)
def digest_fields(v,omit=()):
 return sha(repr([(f,x)for f,x in wire.fields(v)if f not in omit]).encode())
def pending_erase(state):return any(v['kind']=='erase'for v in state['pending']['intents'])
def analyze_row(window,row):
 phase=row['phase'];up=row['atUp']['at'];first=phase.get('wheels',[{}])[0].get('started',up) if phase.get('wheels') else row.get('atNextDown',{}).get('at',up)
 facts=[v for v in row['pendingObservation']['values']if v['at']>=up]
 preparing=[v for v in facts if v['result'].get('state')=='PreparingAtomicCommit']
 frame_calls=[v for v in row['observer']['calls']['values']if v['at']>=up]
 # Multiple passive cpuPending queries can belong to one frame. Associate each
 # observed preparation fact with the immediately preceding actual frame call.
 owners={max((i for i,v in enumerate(frame_calls)if v['at']<=f['at']),default=-1)
         for f in preparing}
 owners.discard(-1)
 writes=[v for v in phase['colorOps']if v['kind']=='content'and v['started']>=first]
 callbacks=[v for v in phase['rafCallbacks']if v['source']=='sdk'and v['started']>=first]
 before=binary(row['snapshots']['before-erase']['base64']);after=binary(row['snapshots']['final']['base64'])
 old,new=wire.objects(before),wire.objects(after)
 assert len(old)==3 and len(new)==(4 if row['q']['action']=='nextdraw'else 3)
 body_preserved=all(digest_fields(v,(8,))==digest_fields(new[k],(8,))for k,v in old.items())
 assert body_preserved
 masks={key:[sha(wire.first(v,2))for f,v in wire.fields(new[key])if f==8]for key in old}
 assert all(len(v)==1 for v in masks.values())
 operations=[]
 for value in row['operations']:
  raw=binary(value['base64']);kind,payload=list(wire.fields(wire.first(raw,5)))[0]
  detail={'kind':kind,'sha256':sha(raw),'Up_to_callback_ms':value['at']-up}
  if kind==12:
   items=[v for f,v in wire.fields(payload)if f==1]
   detail['targets']=[wire.idhex(wire.first(v,1))for v in items]
   detail['geometry_sha256']=[[sha(wire.first(mask,2))for f,mask in wire.fields(item)if f==2]for item in items]
  operations.append(detail)
 assert [v['kind']for v in operations]==([12,10]if row['q']['action']=='nextdraw'else[12])
 assert row['status']=='OBSERVED'and row['closed']and not row['errors']
 actual_pending=pending_erase(row.get('atNavigation',row.get('atNextDown'))['state'])
 assert actual_pending
 return {'window':window,'lane':row['q']['lane'],'action':row['q']['action'],'pixel':row['q']['pixel'],
  'actual_erase_pending_at_navigation_or_nextDown':actual_pending,
  'Up_to_nextDown_ms':row.get('atNextDown',{}).get('at',up)-up if row.get('atNextDown')else None,
  'Up_to_first_operation_callback_ms':operations[0]['Up_to_callback_ms'],
  'Up_to_stable_ms':row['settled']['at']-up,
  'first_content_GL_proxy_ms':writes[0]['started']-first if writes else None,
  'max_RAF_callback_ms':max((v['ended']-v['started']for v in callbacks),default=0),
  'preparing_passive_observations':len(preparing),
  'owner_frames_with_observed_preparing_after_step':len(owners),
  'preparing_observation_span_ms':preparing[-1]['end']-preparing[0]['at']if preparing else None,
  'preparing_CPU_ms':max((int(v['result'].get('commitNs','0'))for v in facts),default=0)/1e6,
  'maximum_owner_CPU_ms':max((int(v['result'].get('maxOwnerNs','0'))for v in facts),default=0)/1e6,
  'original_objects_preserved_excluding_added_masks':body_preserved,
  'new_mask_geometry_by_original_object':masks,
  'objects':[len(old),len(new)],'operations':operations,
  'initial_camera':row['snapshots']['before-erase']['state']['camera'],'final_camera':row['end']['camera'],
  'generation':row['end']['generation'],'presented_generation':row['end']['lastCommit']['presentedGeneration'],
  'root_errors':row['errors'],'pending_observer_lost':row['pendingObservation']['lost'],
  'critical_ring_loss':{k:v['lost']for k,v in row['observer'].items()if isinstance(v,dict)and'lost'in v}}

def main():
 rows=[];pixel_rows=[];raw_refs=[]
 for window in ['web/PN-CHROME-1','web/PN-CHROME-2','iab/PN-IAB']:
  p=OUT/window/'raw.json';raw=json.loads(p.read_text());raw_refs.append({'path':str(p.relative_to(ROOT)),'sha256':sha(p.read_bytes()),'rows':len(raw['rows'])})
  for row in raw['rows']:
   value=analyze_row(window,row);rows.append(value)
   if row['q']['pixel']:
    pixel_rows.append({'window':window,'lane':row['q']['lane'],'action':row['q']['action'],
      'samples':[{'label':p['label'],'camera':p['camera'],'canonical_sha256':sha(binary(p['base64']))if p['base64']else None,
        'canonical_target_status':p['metadata']['status'],
        'preview_target_status':p['preview']['metadata']['status']if p['preview']else None,
        'preview_visibility':p['preview']['visibility']if p['preview']else None}for p in row['pixels']]})
 ordinary=[row for row in rows if not row['pixel']]
 paired=[]
 for browser in ['web/','iab/']:
  for action in ['zoom','pan','nextdraw']:
   values=[v for v in ordinary if v['window'].startswith(browser)and v['action']==action]
   lanes={lane:[v for v in values if v['lane']==lane]for lane in ['baseline','candidate']}
   metrics={lane:{key:statistics.median(v[key]for v in subset if v[key]is not None)
      if any(v[key]is not None for v in subset)else None for key in ['Up_to_first_operation_callback_ms','first_content_GL_proxy_ms',
        'preparing_observation_span_ms','preparing_CPU_ms','owner_frames_with_observed_preparing_after_step','maximum_owner_CPU_ms']}
      for lane,subset in lanes.items()}
   paired.append({'browser':'Chrome'if browser=='web/'else'IAB','action':action,'lanes':metrics,
     'same_original_mask_geometry_ignoring_new_random_identity':len({json.dumps(v['new_mask_geometry_by_original_object'],sort_keys=True)for v in values})==1})
 assert all(v['same_original_mask_geometry_ignoring_new_random_identity']for v in paired)
 report={'schema':'axiom-pending-navigation-progress-analysis-v1','raw_refs':raw_refs,'ordinary_rows':ordinary,'paired_medians':paired,
   'retained_pixel_rows':pixel_rows,'canonical_geometry_preservation':'All original body/config/pressure/placement unchanged; each target added the same exact mask geometry across lanes. Newly allocated Operation/Mask/Object identities are random per Runtime, so browser encodedbytes are not cross-lane identical; independent native staged/sequential test proves exact sameinputwire/store behavior.',
   'limits':['Only2baselinepixelrows persisted. Candidatepixel andnextdrawpreviewpixels missing due aggregateJSONstringlimit; no firstcorrectcamera/previewclaim.',
      'Passive PreparingAtomicCommit observations are not individual ownerframes. A complete stage within one frame can be unobserved after-step; actual native witness is authoritative for mechanism.',
      'FirstGL defaulttarget call is not currentcamera/completeframe/physicalphoton. OnOperation callback is canonical publication notification, not persistence/GPUvisibility.',
      'Owner allowance is cooperative8000us originalturn, no preemption; atomic/target costs may exceed. No globalperformanceSLO orlongtailclosure.',
      'Original6IABordinaryrecords andcriticalobserver rings accumulate acrosssamepage; timestamps isolatepercase, phase strings alone insufficient.'],
   'status':'VALID_NATIVE_AND_ORDINARY_FACTS_BUT_BLOCKED_REQUIRED_CANDIDATE_PIXEL_EVIDENCE'}
 (RES/'browser-observation.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({'ordinary':len(ordinary),'pixel_rows_saved':len(pixel_rows),'paired':paired},indent=2))

if __name__=='__main__':main()
