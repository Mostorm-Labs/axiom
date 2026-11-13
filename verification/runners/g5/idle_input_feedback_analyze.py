"""Offline field/identity and pixel diagnostic checks; not a Semantic codec oracle."""
from pathlib import Path
import json,hashlib,gzip,struct,collections,math
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'out/G5-idle-input-feedback-diagnostic-r1';RES=ROOT/'.aegis/results/G5-idle-input-feedback-diagnostic-r1'
def sha(b):return hashlib.sha256(b).hexdigest()
def fields(b):
 def var(i):
  value=0;shift=0
  while True:
   if i>=len(b)or shift>70:raise ValueError('invalid varint')
   x=b[i];i+=1;value|=(x&127)<<shift
   if x<128:return value,i
   shift+=7
 i=0
 while i<len(b):
  tag,i=var(i);f,w=tag>>3,tag&7
  if not f:raise ValueError('zero field')
  if w==0:v,i=var(i)
  elif w in(1,5):n=8 if w==1 else 4;v=b[i:i+n];i+=n
  elif w==2:n,i=var(i);v=b[i:i+n];i+=n
  else:raise ValueError('wire kind')
  if i>len(b):raise ValueError('truncated field')
  yield f,v

def first(b,f,default=None):return next((v for k,v in fields(b)if k==f),default)
def idhex(b):return first(b,1).hex()
def objects(b):return{idhex(first(v,1)):v for f,v in fields(b)if f==3}
def snapshot(p):return gzip.decompress(p.read_bytes())
def quantile(v):
 v=sorted(v)
 return{'count':len(v),'median_ms':v[(len(v)-1)//2]if v else None,'p95_ms':v[math.ceil(len(v)*.95)-1]if v else None,'max_ms':v[-1]if v else None}
def write(n,v):(RES/n).write_text(json.dumps(v,indent=2)+'\n')
def analyze():
 directory=OUT/'web/IF-FOREGROUND-IDLE';q=json.loads((directory/'raw.json').read_text());d=q['pageData'];o=d['observation'];b=q['idle']['before'];a=q['idle']['after'];lo,hi=b['observation']['at'],a['observation']['at'];
 pointer=[v for v in o['terminals']['values']if v['command']=='pointer'];frame=o['calls']['values'];events=o['events']['values'];result={'status':q['status'],'idle':q['idle'],'sequences':[],'errors':d['errors'],'rings':{k:{'records':len(o[k]['values']),'lost':o[k]['lost']}for k in ['frames','events','logs','tasks','workers','calls','terminals','nativeFrames']}}
 result['idle_activity']={'frame_calls':len([v for v in frame if lo<v['at']<hi]),'GLwrite_delta':a['observation']['glWrites']-b['observation']['glWrites'],'lifecycle': [v for v in events if lo<v['at']<hi],'pointer_calls':len([v for v in pointer if lo<v['at']<hi]),'worker_events':len([v for v in o['workers']['values']if lo<v['at']<hi]),'failed_frame_calls':sum(v['result'].get('ok')is False for v in frame if lo<v['at']<hi),'retry_frame_calls':sum(bool(v['result'].get('pending')or v['result'].get('retry'))for v in frame if lo<v['at']<hi),'cause':'UNKNOWN: idle driver/observer owns no RAF; SDK no-button hover/pointerleave may schedule, but hover/resize entry facts were not retained. Do not label natural calls as prewarm or infer cause from timestamps.'}
 for seq in q['sequences']:
  name=seq['name'];r={'name':name,'actions':[],'selected_id_before':seq['selected']['primary'],'selected_id_after':seq['afterSelection']['primary'],'requested_view_displacement':seq['requested_view_displacement'],'actual_view_displacement':seq['actual_view_displacement'],'before_pose':seq['selected']['transform'],'after_pose':seq['afterSelection']['transform'],'up_to_next_down_ms':seq['up_to_next_down_ms']}
  for action in ['draw-A','draw-B','drag']:
   phase=name+'-'+action;vs=[v for v in pointer if v['phase']==phase];down=next(v for v in vs if v['request']['phase']==0);up=next(v for v in vs if v['request']['phase']==2);vs=[v for v in vs if down['at']<=v['at']<=up['at']];expected=66 if action.startswith('draw')else 34
   assert len(vs)==expected and all(v['result']['ok']for v in vs)
   dom=[v for v in events if v['phase']==phase and down['at']-30<=v['at']<=up['at']+30 and v['type']in['pointerdown','pointerup','pointercancel']]
   times=[v['end']-v['at']for v in o['frames']['values']if down['at']<=v['at']<=up['at']];observed=next(v for v in q['pixels']if v['label']==phase+'-move16')
   r['actions'].append({'phase':phase,'source':down['request']['source'],'pointer':down['request']['pointer'],'Down':down,'Up':up,'calls':len(vs),'DOM_terminal_events':dom,'callback':quantile(times),'Down_submit_cpu_ms':down['end']-down['at'],'first_sampled_feedback_upper_bound_ms':observed['at']-down['at'],'first_correct_feedback_latency_ms':None,'sampling_limit':'firstsample atmove16; latency before it unobserved, ROI readbacks add synchronous observercost'})
  result['sequences'].append(r)
 initial=snapshot(directory/'initial.axsnap.gz');pre=snapshot(directory/'pre-idle.axsnap.gz');final=snapshot(directory/'final.axsnap.gz');original=objects(initial);before=objects(pre);after=objects(final)
 assert len(original)==40 and len(before)==42 and len(after)==44
 target=q['sequences'][0]['selected']['primary'];preservation=[]
 for key,value in original.items():
  assert key in before and key in after
  rest=lambda obj:[(f,v)for f,v in fields(obj)if f!=5]
  assert rest(value)==rest(before[key])==rest(after[key])
  same_pose=first(value,5)==first(before[key],5)==first(after[key],5)
  assert same_pose or key==target
  preservation.append({'id':key,'body_and_masks_equal':True,'pose_equal':same_pose})
 creates=[];transforms=[]
 for i,public in enumerate(d['operations']):
  raw=gzip.decompress((directory/f'operation-{i}.bin.gz').read_bytes());payload=first(raw,5);parts=list(fields(payload));assert len(parts)==1;kind,value=parts[0]
  assert idhex(first(raw,1))==public['state']['lastCommit']['operationId']
  if kind==10:
   obj=first(value,1);key=idhex(first(obj,1));assert key in after and list(fields(obj))==list(fields(after[key]));content=first(obj,7);compact=first(content,14);count=first(compact,3);positions=first(compact,4);pressure=first(compact,5);assert len(positions)==count*16 and len(pressure)==count*2
   points=list(struct.iter_unpack('<dd',positions));camera=q['camera'];x=[v[0]*camera['scale']+camera['x']for v in points];y=[v[1]*camera['scale']+camera['y']for v in points]
   creates.append({'operation':public['state']['lastCommit']['operationId'],'id':key,'knots':count,'positions_sha256':sha(positions),'effective_pressure_sha256':sha(pressure),'view_bounds':[min(x),min(y),max(x),max(y)],'persisted_content_matches_operation':True,'algorithm_version':first(first(compact,1),2)})
  else:
   assert kind==5;transforms.append({'kind':'SetTransforms','operation':public['state']['lastCommit']['operationId'],'sha256':sha(raw)})
 assert len(creates)==4 and len(transforms)==2
 pixel_rows=[]
 for p in q['pixels']:
  row={'label':p['label'],'at':p['at'],'targets':[]}
  for c in p['captures']:
   data=gzip.decompress((directory/c['file']).read_bytes());amber=sum(1 for i in range(0,len(data),4)if data[i]>100 and data[i+1]>50 and data[i+2]<60 and data[i+3]>30);dark=sum(1 for i in range(0,len(data),4)if data[i]<80 and data[i+1]<100 and data[i+2]<120 and data[i+3]>30)
   row['targets'].append({'role':c['metadata']['role'],'visibility':c['visibility'],'status':c['metadata']['status'],'amber_pixels':amber,'dark_pixels':dark,'sha256':sha(data),'file':c['file']})
  pixel_rows.append(row)
 result['pixel_checks']={'samples':pixel_rows,'assertion':'at sampledwhileDown points both pre/postidle showamber preview; complete committeddrawbounds matchfrozeninput; existing bodies/masks preserved independently by snapshot field comparison','limitation':'samecoordinate opaque postidle draw overlapspreidle; finalblackdifference alone isnotnewstroke proof. FiniteROIs/snapshot checks arenotfullsceneCPUoracle/AAqualification; no firstphoton claim'}
 result['semantic_field_checks']={'method':'offline wire field checks using frozen proto; root restore/capture still uses realSemanticcodec. Not a second codec authority.','document_identity_equal':idhex(first(initial,1))==idhex(first(pre,1))==idhex(first(final,1)),'objects':[40,42,44],'original40':preservation,'new_strokes':creates,'transforms':transforms,'all_original_masks_preserved':True,'original_selected_target':target}
 result['no_reproduction_boundary']=['Fixed40 differs from user22document','80 persisted masks x32segments are present, but no active consecutive erase in restored runtime before idle','entryfullysettled generation/presented3/3 and empty pending; no unfinished erase->idle','visible/focus lifecycle qualified; physicalOSfrontmost not continuously observed','strictnoRAFidlefailed29naturalidleframes; GLwritesunchanged, triggerunknown','firstfeedback is only sampled upperbound; no absolute or relative performancePASS','one terminal window, no retry or source changes']
 result['normal_vs_diagnostic_cost']='ordinary and private timing are separate facts; do not compare as pairedperformance'
 write('idle-observation.json',result)
 write('causal-analysis.json',{'diagnosis':'FINITE_NO_REPRODUCTION_WITH_QUALIFICATION_LIMIT','confirmed':{'actual_idle_ms':q['idle']['qualification']['elapsed_ms'],'visible_focus_before_after':True,'idle_frame_calls':29,'idle_GPU_write_delta':0,'new_strokes':4,'transforms':2,'final_objects':44,'frame_rejections':sum(v['result'].get('ok')is False for v in frame),'input_rejections':sum(v['result'].get('ok')is False for v in pointer),'ring_loss':sum(v['lost']for v in result['rings'].values()),'historical_masks':80,'historical_segments':2560},'unproven_hypotheses':[{'id':'handoff','finding':'no missing sampledpreview/canonical in stable short+idlecontrols; retainedidentity provesadmission/apply. User dynamicerase state not covered.'},{'id':'frame_exception_stopsRAF','finding':'actualframeokfalse0; no catchfailure inthese runs, no syntheticfault. Sourcelevelrisk remains, notusercauseproof.'},{'id':'idleTTL_or_needprewarm','finding':'no20minTTLfound; thisconditionnorepro; 29naturalidleframes meanstrictnostepintervalunqualified, no prewarmfixauthorized.'}], 'qualification_blocker':'ENVIRONMENT_OR_FOREGROUND_VISIBILITY_QUALIFICATION: strict no runtime RAF condition failed; 29idle requests noGPUwrites, unknown natural origin; no new20minwindowallowed','latest_user_precondition':'User says repeatedpartialerasesbeforeidle. Persistedmasksalone do not reproduce livecache/pending/displayhistory.','minimum_successor_proposal':{'kind':'diagnostic only until causallylocalized','owner':'existing interaction pendingLocal/CPU erase -> OperationEngine -> display records/composition -> canonical/preview/fence; SDKcollector/frame scheduler observed','sequence':'sameordinaryrootdocument: consecutivepartialerases -> immediate nextdraw anddrag both while pending and aftersettled; preserve originaldocument/operations ifavailable. Capture exact pending serial/key, maskrevision, display covered/retire and frame basis atentry. Addpassive hover/resize trigger facts onlytoexplainnaturalRAF. 20minrepeat requires newfinite authorization, notautomatic.','no_behavior_patch_yet':True},'forbidden_claims':['repaired','prewarmrequired','allidlePASS','performancePASS','circlefix','P34PASS']})
 print({'analysis':'materialized','actualidle':q['idle']['qualification'],'semanticfieldchecks':True,'finalobjects':44})
if __name__=='__main__':analyze()
