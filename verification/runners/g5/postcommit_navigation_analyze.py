"""Offline role, timing, semantic and pixel qualification; never starts browser."""
from pathlib import Path
import gzip, hashlib, importlib.util, json, statistics
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'out/G5-postcommit-navigation-feedback-r1'
RES=ROOT/'.aegis/results/G5-postcommit-navigation-feedback-r1'
spec=importlib.util.spec_from_file_location('wire',ROOT/'verification/runners/g5/idle_input_feedback_analyze.py')
wire=importlib.util.module_from_spec(spec);spec.loader.exec_module(wire)
def sha(b):return hashlib.sha256(b).hexdigest()
def write(name,value):(RES/name).write_text(json.dumps(value,indent=2)+'\n')
def payload(base,value):
 b=value['buffer'];compressed=(base/b['path']).read_bytes();raw=gzip.decompress(compressed)
 assert len(raw)==b['bytes']and sha(raw)==b['sha256']and sha(compressed)==b['compressed_sha256']
 return raw
def load(id):
 base=OUT/'web'/id;index=json.loads((base/'index.json').read_text());assert index['complete']
 rows=[]
 for entry in index['rows']:
  p=base/entry['path'];assert sha(p.read_bytes())==entry['sha256'];row=json.loads(p.read_text())
  assert row['status']=='OBSERVED'and row['closed']and not row['errors']and not row.get('closeError')
  rows.append((base,row))
 return rows
def semantic(base,row):
 first='before-erase'if'before-erase'in row['snapshots']else'before'
 before,after=[wire.objects(payload(base,row['snapshots'][key]))for key in[first,'final']]
 body=lambda obj:[(f,v)for f,v in wire.fields(obj)if f!=8]
 assert len(before)==len(after)==3 and all(body(v)==body(after[k])for k,v in before.items())
 masks={}
 for key in before:
  values=[v for f,v in wire.fields(after[key])if f==8];assert len(values)==1;masks[key]=sha(wire.first(values[0],2))
 assert len(row['operations'])==1
 op=payload(base,row['operations'][0]);kind,items=list(wire.fields(wire.first(op,5)))[0];assert kind==12
 assert len([v for f,v in wire.fields(items)if f==1])==3
 return {'original_bodies_config_pressure_placement_unchanged':True,'mask_geometry':masks,'operation_sha256':sha(op),'one_AddEraseMasks':True}
def main():
 diag=[]
 for id in['NC-DIAG-BASE','NC-DIAG-CAND']:
  for base,row in load(id):
   up=row['atUp']['at'];events=row['nativeObservation'];assert events['lost']==0
   times=[];renders=[]
   for e in events['values']:
    if e['at']<up:continue
    if e['text'].startswith('AXIOM_FRAME_TIMING '):
     v=json.loads(e['text'].split('AXIOM_FRAME_TIMING ',1)[1]);times.append({'at_ms':e['at']-up,**v})
    elif' nc_render 'in e['text']:renders.append({'at_ms':e['at']-up,'text':e['text'].split(' nc_render ',1)[1]})
   canon=next(v for v in renders if v['text'].startswith('role=canonical'))
   transient=[v for v in renders if v['text'].startswith('role=transient')]
   diag.append({'id':id,'case':row['q']['action'],'semantic':semantic(base,row),'render_events':renders,
    'canonical_submit_ms':canon['at_ms'],'Operation_callback_ms':row['operations'][0]['at']-up,
    'body_compile_replay_after_up':sum(v['counts']['body_compile']+v['counts']['body_replay']for v in times),
    'prepare_resets':sum(v['counts']['prepare_resets']for v in times),'owner_cpu_sum_ms':sum(v['stages']['attempt']['inclusive_ns']/1e6 for v in times if v['role']=='owner'),
    'owner_opportunities':sum(v['role']=='owner'for v in times),'transient_retry_reasons':[v['text']for v in transient],
    'first_navigation_ms':next(v['at_ms']for v in renders if v['text'].startswith('role=navigation')),
    'native_timings':times})
 write('cause-and-timeline.json',{'status':'ACTUAL_SCOPED_EXTRA_QUIET_WINDOW_DEMONSTRATED_AND_REPAIRED',
  'older_proxy_context':{'source':'.aegis/results/G5-postcommit-navigation-feedback-control-r1/offline-preflight.json','role_limit':'OldfirstGLcommonlytransientprecommit; currentfirstGLpostcommitnavigation. Not anequivalentcorrectframe regression.'},
  'diagnostic_rows':diag,'confirmed':['Compositionselectsrouteafterpendingcommit; transient2groups+sharedallowancekeepnewcameramaterialprivate untilcomplete.','Camera andnewsemanticmaskbasisneedinvalidation;bodygeometrycompile/replayafterUp=0. No fullgeometryrebuild claim.','ExistingserviceNavigationPresentation initializedquietclockonlyatfirsteligiblepostcommitframe. Realcameraalreadyquiet; this restarted100ms. Commonprivatecamerainvalidationnowtimestampssameexistingownerclock.','Same100mspolicy andallbudgets/gates unchanged; target/scene/camera/sourceidentityguards retained.'],
  'diagnostic_timing_not_ordinary':True})
 ordinary=[]
 for id in['NC-ORD-1','NC-ORD-2']:
  for base,row in load(id):
   up=row['atUp']['at'];phase=row['phase'];nav=phase['wheels'][0]['started']
   content=[v for v in phase['colorOps']if v['kind']=='content'and v['started']>=nav]
   callbacks=[v for v in row['frames']if v['at']>=up]
   calls=[v for v in row['observer']['calls']['values']if v['at']>=up]
   assert content and len(callbacks)==1 and all(v['result'].get('ok')for v in calls)
   state=row['atNavigation']['state'];assert any(v['kind']=='erase'for v in state['pending']['intents'])
   facts=row['pendingObservation']['values'];prep=[v for v in facts if v['at']>=up and v['result'].get('state')=='PreparingAtomicCommit']
   ordinary.append({'id':id,'lane':row['q']['lane'],'case':row['q']['action'],'semantic':semantic(base,row),
    'actual_erase_pending_at_navigation':True,'up_to_navigation_ms':nav-up,'first_GL_proxy_ms':content[0]['started']-nav,'first_GL_function':content[0]['name'],
    'up_to_Operation_callback_ms':row['operations'][0]['at']-up,'up_to_qualified_onFrame_callback_ms':callbacks[0]['at']-up,
    'post_up_owner_opportunities':len(calls),'max_owner_call_ms':max(v['end']-v['at']for v in calls),
    'max_RAF_callback_ms':max(v['ended']-v['started']for v in phase['rafCallbacks']if v['source']=='sdk'),
    'pending_observer_lost':row['pendingObservation']['lost'],'initial_camera':row['snapshots']['before-erase']['state']['camera'],'final_camera':row['end']['camera']})
 medians=[]
 for action in['zoom','pan']:
  lanes={}
  for lane in['baseline','candidate']:
   values=[v for v in ordinary if v['case']==action and v['lane']==lane]
   lanes[lane]={key:statistics.median(v[key]for v in values)for key in['first_GL_proxy_ms','up_to_Operation_callback_ms','up_to_qualified_onFrame_callback_ms','post_up_owner_opportunities','max_owner_call_ms','max_RAF_callback_ms']}
  medians.append({'case':action,'lanes':lanes,'qualification_callback_reduction_ms':lanes['baseline']['up_to_qualified_onFrame_callback_ms']-lanes['candidate']['up_to_qualified_onFrame_callback_ms']})
 write('paired-observation.json',{'ordinary_rows':ordinary,'paired_medians':medians,'round_orders':['baseline->candidate','candidate->baseline'],
  'limits':['onFrame presentedcount changeisrootSDKqualifiedcanonicalcallback observation, notGPUelapsed orphysicalphoton. ItisnotsemanticpresentedGeneration; initialrestoredsemanticgeneration0 haspreexistingpresentedframecount2.','FirstGLproxy isnotcorrectcamera/completeframe. Ordinaryrunsnoframereadbacks/heldWorker/nativeTiming.','Twooppositelanorders smallsample; allregressionsreported andnooverallperformancePASS.']})
 pixel=[];stored=load('NC-PIXEL')
 for base,row in stored:
  sem=semantic(base,row);assert row['pixelRAFRestored']
  final=next(v for v in row['pixels']if v['label']=='final');old=row['pixels'][0];before=payload(base,old);end=payload(base,final);samples=[]
  for v in row['pixels']:
   rgba=payload(base,v);m=v['metadata'];assert m['status']=='VALID'and m['restoration']['status']=='RESTORED'and not m['context']['lost']
   assert len(rgba)==900*600*4
   colored=sum(rgba[i:i+3]!=b'\xff\xff\xff'for i in range(0,len(rgba),4));assert colored>0
   erased=0;count=0;camera=v['camera']
   if v['label']!='before-up':
    x0,x1=[int((x*camera['scale']+camera['x'])*2)-300 for x in[424,436]]
    y0,y1=[int((y*camera['scale']+camera['y'])*2)-300 for y in[220,285]]
    for y in range(y0,y1):
     for x in range(x0,x1):
      if 0<=x<900 and 0<=y<600:
       at=(y*900+x)*4;erased+=rgba[at:at+4]!=b'\xff\xff\xff\xff';count+=1
    assert erased==0 and count>1000
   samples.append({'label':v['label'],'camera':v['camera'],'generation':v['state']['generation'],'presented_generation':v['state']['lastCommit']['presentedGeneration'],
    'pending':len(v['state']['pending']['intents']),'sha256':sha(rgba),'same_as_old_camera':rgba==before,'same_as_final':rgba==end,
    'nonwhite_pixels':colored,'erased_interior_opaque_white_failures':erased,'erased_interior_checked_pixels':count})
  item={'lane':row['q']['lane'],'case':row['q']['action'],'semantic':sem,'samples':samples,'final_RGBA_sha256':sha(end),'no_blank_no_oldcamera_return_at_samples':all(not v['same_as_old_camera']for v in samples[1:]),'finite_ROI':{'x':300,'y':300,'width':900,'height':600}}
  assert item['no_blank_no_oldcamera_return_at_samples']
  coarse=next(v for v in row['pixels']if v['label']=='actual-content-2')
  coarse_rgba=payload(base,coarse)
  item['coarse_vs_final']={'changed_pixels':sum(coarse_rgba[i:i+4]!=end[i:i+4]for i in range(0,len(end),4)),
    'changed_channels':sum(a!=b for a,b in zip(coarse_rgba,end)),
    'maximum_delta':max(abs(a-b)for a,b in zip(coarse_rgba,end)),
    'coarse_sha256':sha(coarse_rgba)}
  if row['q']['lane']=='candidate':
   key='candidate-'+row['q']['action'];cpu=(OUT/'offline-oracle'/(key+'.rgba')).read_bytes();edge=(OUT/'offline-oracle'/(key+'.edges')).read_bytes();outside=0
   for y in range(600):
    for x in range(900):
     di=(y*900+x)*4;si=((y+300)*2400+x+300)*4
     if not edge[si//4]:outside+=sum(cpu[si+j]!=end[di+j]for j in range(4))
   assert outside==0;item['independent_CPU_nonedge_mismatched_channels']=outside
  pixel.append(item)
 for action in['zoom','pan']:
  matches=[v for v in pixel if v['case']==action]
  assert len({v['final_RGBA_sha256']for v in matches})==1
  assert len({json.dumps(v['semantic']['mask_geometry'],sort_keys=True)for v in matches})==1
 write('pixel-semantic.json',{'Web_rows':pixel,'final_ROI_exact_baseline_both_actions':True,'actual_pending_current_camera_samples':True,
  'native_wholeframe':{'source':'runtime/composition/tests/cpu_pending_runtime_test.cpp postcommitzoom/pan','passed33log':'out/G5-postcommit-navigation-feedback-r1/final-native-tests.log','reference':'Independentobjectsnapshot->projectRuntimeScene->Directcommands/nullcache SkCanvas1431x1010 fullframe; zoom3channelsmax1allfixed1pxsourcecontourAA,panexact; noedgeexclusiondiff-derived.',
    'guards':'Retrykeepslastsuccessfulpixels; delayedmatchingrosterkept, foreignsurfacefeedbackrejected, exactmatchingretire; originalSemanticStoreunchanged; freshcamera99/100 andexactnoopclockguards'},
  'intermediate_coarse_basis':'Baseline/candidate content2samecamera/maskgeometryandcoarsehash. FixederaseinterioropaqueWhite/nonblank/nooldcamera atfiniteROIpoints. Coarse differs final at2415/2780pixels, max199/208; do not claimabsolutecompletecorrectinterimimage or fullmaskboundarycorrectness. Candidatecontent3final wherebaselinecontent3stillcoarse; no falsecanonicalreceiptfornavigation.',
  'limitations':['Finite3actualcontentROI captures: notcontinuousallviewportorfirstphoton. Nativefullframeandtargetguardsareseparate.','NavigationinterimapproximateRGBA differsfromfinalexact, butalltestedcurrentcameraeraseinterioropaqueWhite/nonblank andcrosslaneequivalentroutepixelssame. No canonicalreceiptforinterim.','Pixelrunsreadbacksperturbclock; neverpoolwithordinary.']})
 print(json.dumps({'ordinary':len(ordinary),'pixels':len(pixel),'medians':medians,'final_exact':True,'sampled_erase_interior_failures':0},indent=2))
if __name__=='__main__':main()
