"""Offline identity/mask/pixel witness analysis; existing Semantic root stays authority."""
from pathlib import Path
import json,gzip,hashlib,collections,math,importlib.util
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'out/G5-live-erase-input-feedback-r1';RES=ROOT/'.aegis/results/G5-live-erase-input-feedback-r1'
spec=importlib.util.spec_from_file_location('retained_if_wire',ROOT/'verification/runners/g5/idle_input_feedback_analyze.py');wire=importlib.util.module_from_spec(spec);spec.loader.exec_module(wire)
fields,first,idhex,objects=wire.fields,wire.first,wire.idhex,wire.objects
sha=lambda b:hashlib.sha256(b).hexdigest()
def masks(obj):return{idhex(first(m,1)):m for f,m in fields(obj)if f==8}
def mask_segments(mask):
 geometry=first(mask,2);variant=list(fields(geometry))
 if len(variant)!=1:raise ValueError('mask geometry oneof')
 kind,data=variant[0]
 if kind!=1:raise ValueError('expected swept mask in frozen waveform')
 return sum(f==1 for f,_ in fields(data))
def without_pose_masks(obj):return[(f,v)for f,v in fields(obj)if f not in(5,8)]
def window(ident):
 directory=OUT/'web'/ident;q=json.loads((directory/'raw.json').read_text());d=q['pageData'];o=d['observation'];initial=gzip.decompress((directory/'initial.axsnap.gz').read_bytes());final=gzip.decompress((directory/'final.axsnap.gz').read_bytes());before,after=objects(initial),objects(final)
 assert len(before)==40 and len(after)==43
 target=q['drags'][0]['actualTarget'];assert all(v['actualTarget']==target and v['afterSelection']['primary']==target for v in q['drags'])
 old_mask_count=0;old_segment_count=0
 for key,obj in before.items():
  assert key in after and without_pose_masks(obj)==without_pose_masks(after[key])
  assert first(obj,5)==first(after[key],5)or key==target
  for mid,mask in masks(obj).items():
   assert masks(after[key]).get(mid)==mask;old_mask_count+=1;old_segment_count+=mask_segments(mask)
 operations=[];added=[];created=[];transforms=[]
 for index,pub in enumerate(d['operations']):
  raw=gzip.decompress((directory/f'operation-{index}.bin.gz').read_bytes());opid=idhex(first(raw,1));assert opid==pub['state']['lastCommit']['operationId'];kind,value=list(fields(first(raw,5)))[0]
  row={'index':index,'operationId':opid,'kind':kind,'generation':pub['state']['generation'],'at':pub['at'],'sha256':sha(raw)}
  if kind==12:
   items=[v for f,v in fields(value)if f==1];row.update(targets=len(items),mask_count=0,segments=0,object_ids=[])
   for item in items:
    key=idhex(first(item,1));assert key in before;row['object_ids'].append(key)
    for f,mask in fields(item):
     if f!=2:continue
     mid=idhex(first(mask,1));assert mid not in masks(before[key]);assert masks(after[key]).get(mid)==mask
     row['mask_count']+=1;row['segments']+=mask_segments(mask);added.append((key,mid))
   assert row['targets']==40 and row['mask_count']==40 and row['segments']==6400
  elif kind==10:
   obj=first(value,1);key=idhex(first(obj,1));assert key not in before and key in after and list(fields(obj))==list(fields(after[key]));compact=first(first(obj,7),14);count=first(compact,3);assert len(first(compact,4))==count*16 and len(first(compact,5))==count*2
   row.update(created_id=key,knots=count,positions_sha256=sha(first(compact,4)),pressure_sha256=sha(first(compact,5)));created.append(row)
  else:
   assert kind==5;items=[v for f,v in fields(value)if f==1];assert len(items)==1 and idhex(first(items[0],1))==target;transforms.append(row)
  operations.append(row)
 assert len(operations)==9 and len(created)==3 and len(transforms)==3 and len(added)==120
 assert len(set(added))==120 and sum(len(masks(v))for v in after.values())==200
 pixel_rows=[]
 for p in q['pixels']:
  row={'label':p['label'],'at':p['at'],'targets':[]}
  for c in p['captures']:
   data=gzip.decompress((directory/c['file']).read_bytes());assert sha(data)==c['sha256'];amber=sum(1 for i in range(0,len(data),4)if data[i]>100 and data[i+1]>50 and data[i+2]<60 and data[i+3]>30);dark=sum(1 for i in range(0,len(data),4)if data[i]<80 and data[i+1]<100 and data[i+2]<120 and data[i+3]>30)
   row['targets'].append({'role':c['metadata']['role'],'visibility':c['visibility'],'amber_pixels':amber,'dark_pixels':dark,'status':c['metadata']['status'],'target':c['metadata']['canvas'],'file':c['file'],'sha256':c['sha256'],'GLerror_qualification':c['metadata']['error_queue']})
  pixel_rows.append(row)
 pointer=[v for v in o['terminals']['values']if v['command']=='pointer'];actions=[]
 labels=[v['label']for v in q['erases']+q['draws']+q['drags']]
 for label in labels:
  vs=[v for v in pointer if v['phase']==label];down=next(v for v in reversed(vs)if v['request']['phase']==0);up=next(v for v in vs if v['request']['phase']==2 and v['at']>=down['at']);actual=[v for v in vs if down['at']<=v['at']<=up['at']];expected=162 if label.startswith('erase')else 66 if label.endswith('draw')else 34
  assert len(actual)==expected and all(v['result'].get('ok')is True for v in actual)
  frame=[v for v in o['frames']['values']if down['at']<=v['at']<=up['at']];sample=next((v for v in q['pixels']if v['label']==label+'-move16'),None)
  actions.append({'label':label,'down':down['at'],'up':up['at'],'source':down['request']['source'],'pointer':down['request']['pointer'],'calls':len(actual),'Down_CPU_ms':down['end']-down['at'],'RAF_callback':wire.quantile([v['end']-v['at']for v in frame]),'first_sampled_feedback_upper_bound_ms':sample['at']-down['at']if sample else None,'first_correct_feedback_latency':None})
 for label in [v['label']for v in q['draws']]:
  cap=next(v for v in pixel_rows if v['label']==label+'-move16');preview=cap['targets'][1];assert preview['visibility']=='visible'and preview['amber_pixels']>0
 lo,hi=q['pause']['before']['at'],q['pause']['after']['at'];pause={'actual_ms':q['pause']['actual_ms'],'RAF_calls':sum(lo<v['at']<hi for v in o['calls']['values']),'triggers':[v for v in o['triggers']['values']if lo<v['at']<hi],'lifecycle':[v for v in o['events']['values']if lo<v['at']<hi],'GPU_write_delta':q['pause']['after']['observation']['glWrites']-q['pause']['before']['observation']['glWrites']}
 canonical_rows={k:{'state':v['state'],'sha256':v['sha256']}for k,v in q.items()if isinstance(v,dict)and'sha256'in v and'state'in v}
 return{'id':ident,'status':q['status'],'document':d['states'][0]['state']['documentId'],'camera':q['camera'],'brush':q['brush'],'operations':operations,'initial_objects':40,'final_objects':43,'initial_masks':old_mask_count,'initial_segments':old_segment_count,'new_masks':120,'new_segments':19200,'final_masks':200,'original_body_property_placement_preserved':True,'original_masks_preserved':True,'actual_input_rejections':sum(v['result'].get('ok')is False for v in pointer),'actual_frame_rejections':sum(v['result'].get('ok')is False for v in o['calls']['values']),'root_errors':d['errors'],'page_errors':q['errors'],'actions':actions,'draws':q['draws'],'drags':q['drags'],'pause':pause,'canonical_snapshots':canonical_rows,'pixels':pixel_rows,'rings':{k:{'records':len(o[k]['values']),'lost':o[k]['lost']}for k in ['frames','events','logs','tasks','workers','calls','terminals','nativeFrames','triggers']},'repeated_aggregate_dropped':o['repeatedLost'],'display_events':[v for v in o['logs']['values']if v['text'].startswith('AXIOM_IF_EVENT')and'display_'in v['text']],'limitation':'Aggregate repeated telemetry loss is explicit in diagnostic. Criticalringloss0 does not recover everypreview/fence/coverageevent. PixelGLerrorqueue unconsumed/unqualified; restoredtarget/context/bytes observed. No firstGL==correctfeedback. No completecontinuousframeorCPUoracle proof.'}
def main():
 rows=[window(v)for v in ['LE-ORDINARY','LE-DIAGNOSTIC']]
 for row in rows:
  (RES/(row['id']+'-observation.json')).write_text(json.dumps(row,indent=2)+'\n')
 conclusion={'diagnosis':'FINITE_NO_REPRODUCTION_WITH_PENDING_DRAW_COVERAGE','windows':[{'id':v['id'],'status':v['status'],'immediate_draw_erase_pending':v['draws'][0]['erase_pending_at_Down'],'immediate_drag_erase_pending':v['drags'][0].get('erase_pending_at_Down'),'root_errors':len(v['root_errors']),'input_rejections':v['actual_input_rejections'],'frame_rejections':v['actual_frame_rejections'],'new_masks':v['new_masks'],'new_segments':v['new_segments'],'pause_ms':v['pause']['actual_ms'],'repeated_aggregate_dropped':v['repeated_aggregate_dropped']}for v in rows], 'confirmed':'Actual3nonNoop live erasuresx40targets, immediate64move mouse draw admittedwhilethird erasepending; sampledamberpresent and actualcommit. settledandpost60spausecontrolsoperate. SameactualoriginaldragID acrossallphases; oldbodies/masks preserved.', 'scope_gaps':['erasependingdragbranch missed:third erasefinishedbeforeactualdragDown','user22documentunavailable; engineering40duplicatefixture','no20minrun, no longidlequalification inthispackage','opaque samepositiondrawoverlap, geometry/Operation/amberused; finalblackdifference insufficient','samplepixel atmove16/32 only: firstcorrectfeedback latencyunmeasured, sparsehandoffwitness','diagnostic repeated telemetry dropped3682; criticalinput/display/error ringslost0 but finefence/preview chronology incomplete'], 'rootcause':'UNPROVEN; no failure/blank capturedinscope; reject simple deterministicclaim that3liveerasuresnecessarilybreak nextdraw.', 'minimum_next_proposal':'Keepuserdefectopen. Prefercaptureactualuserdocumentandpassivefailure-boundary identities onnextnaturaloccurrence; ifnewscopeauthorized, isolatefirstdragwhileerasepending beforethe64movedrawdrainsit. Do notrepeat20min/loadamplify/addWorkerorfixgeometrywithoutnewauthorization.', 'no_behavior_repair':True,'no_performance_or_P34_PASS':True}
 (RES/'causal-analysis.json').write_text(json.dumps(conclusion,indent=2)+'\n')
 print({'diagnosis':conclusion['diagnosis'],'windows':conclusion['windows']})
if __name__=='__main__':main()
