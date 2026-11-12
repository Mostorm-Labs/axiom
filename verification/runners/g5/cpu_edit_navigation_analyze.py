"""Offline attribution; never turns GL/RAF proxies into qualified presents."""
from pathlib import Path
import collections,hashlib,json,struct
ROOT=Path(__file__).resolve().parents[3]
RESULT=ROOT/'.aegis/results/G5-CPUW4-edit-navigation-r1'
RUN=RESULT/'local-web/CPUW4-D3-BASELINE-TRACE'
def read(p):return json.loads(Path(p).read_text())
def write(name,v):(RESULT/name).write_text(json.dumps(v,indent=2)+'\n')
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':hashlib.sha256(Path(p).read_bytes()).hexdigest()}
def merge_intervals(spans):
 out=[]
 for a,b in sorted(spans):
  if out and a<=out[-1][1]:out[-1][1]=max(b,out[-1][1])
  else:out.append([a,b])
 return out
def analyze():
 ordinary=[];diagnostic=[]
 for p in sorted(RUN.glob('*/raw.json')):
  d=read(p);nav=d['navigation'];o=d['observation'];first=nav[0]['started'];proxy=d['proxy']['firstDefaultFboContentAt'];up=d['upReturned']
  item={'id':d['id'],'status':d['status'],'raw':ref(p),'edit_up_dispatch_ms':next(e['ended']-e['started']for e in reversed(d['events'])if e['phase']=='up'),'planned_to_first_handler_ms':first-nav[0]['planned'],'up_return_to_first_handler_ms':first-up,'first_content_proxy_after_NAV_handler_ms':None if proxy is None else proxy-first,'first_content_proxy_after_Up_ms':None if proxy is None else proxy-up,'last_onFrame_after_NAV_ms':max((f['at']for f in d['frames']if f['at']>=first),default=first)-first,'stable_poll_after_NAV_ms':d['settledAt']-first,'maximum_RAF_callback_ms':max((f['ended']-f['started']for f in o['rafCallbacks']),default=0),'maximum_NAV_dispatch_ms':max(e['ended']-e['started']for e in nav),'maximum_pointer_dispatch_ms':max(e['ended']-e['started']for e in d['events']),'event_lateness_ms':[e['started']-e['planned']for e in nav],'first_matching_NAV_submit_ms':None,'qualified_present_note':'No ordinary camera-qualified signal; default FBO content and onFrame are proxies. Stable polling includes three quiet checks.','operations':len(d['operations']),'final_objects':d['states']['after']['objects'],'expected_camera':d['expectedCamera'],'actual_camera':d['states']['after']['camera']}
  if not d['trace']:ordinary.append(item);continue
  dec=read(p.parent/'decoded.json');records=[r for c in d['trace']for r in c['records']];offset=d['clock'][0]['js']-int(d['clock'][0]['native']['clock_ns'])/1e6
  spans=dec['spans'];native_up=(up-offset)*1e6;native_nav=(first-offset)*1e6
  before=[]
  for s in spans:
   if s['start_ns']is None or s['end_ns']is None:continue
   a,b=int(s['start_ns']),int(s['end_ns'])
   if b>native_up and a<native_nav:before.append({'id':s['id'],'parent':s['parent'],'tag':s['tag'],'start_ns':s['start_ns'],'end_ns':s['end_ns'],'inclusive_ms':int(s['inclusive_ns'])/1e6,'exclusive_ms':int(s['exclusive_ns'])/1e6,'intersection_with_post_Up_queue_ms':max(0,min(b,native_nav)-max(a,native_up))/1e6})
  intervals=merge_intervals([(max(int(s['start_ns']),native_up),min(int(s['end_ns']),native_nav))for s in spans if s['start_ns']is not None and s['end_ns']is not None and int(s['end_ns'])>native_up and int(s['start_ns'])<native_nav])
  nav_timings=[];frame_timings=[]
  for log in read(p.parent/'console.json'):
   text=log['text']
   if text.startswith('AXIOM_NAVIGATION_TIMING '):
    v=json.loads(text.split(' ',1)[1])
    if int(v['start_ns'])>=native_up:nav_timings.append(v)
   elif text.startswith('AXIOM_FRAME_TIMING '):
    v=json.loads(text.split(' ',1)[1])
    if int(v['start_ns'])>=native_up:frame_timings.append(v)
  max_by_tag={}
  for s in spans:
   if s['end_ns']is None:continue
   ms=int(s['exclusive_ns']or 0)/1e6
   if ms>max_by_tag.get(str(s['tag']),{}).get('exclusive_ms',-1):max_by_tag[str(s['tag'])]={'exclusive_ms':ms,'inclusive_ms':int(s['inclusive_ns'])/1e6,'span':s}
  nav_records=[r for r in records if r['purpose']==4]
  submitted=[r for r in nav_records if r['kind']==11 and r['tag']==12]
  bases=[r for r in records if r['kind']==5 and r['tag']==26 and len(r['values'])==10]
  accepts=[]
  for r in records:
   if r['kind']==11 and r['tag']==19 and r['values'][-1]=='0':
    matches=[b for b in bases if b['job']==r['job']and b['basis']==r['basis']and b['values'][0:4]==r['values'][0:4]and b['values'][5:7]==r['values'][4:6]]
    accepts.append({'provider_record':r,'matched_render_basis':matches[0]if len(matches)==1 else None,'camera':matches[0]['values'][4]if len(matches)==1 else None,'qualification':'actual draw/provider job and frame match only; full commit/owner join remains missing'})
  item.update({'diagnostic_only':True,'clock_offset_ms':offset,'clock_error_note':'One initial sampled JS/native clock pair, 0.1ms browser timer precision; no GPU clock/photon measurement.','decoded_errors':dec['errors'],'decoded_missing':dec['missing'],'qualified_completion_count':len(dec['qualified_completions']),'trace_loss':d['traceLoss'],'post_Up_queue_spans':before,'post_Up_queue_instrumented_interval_union_ms':sum(b-a for a,b in intervals)/1e6,'post_Up_queue_uninstrumented_or_wait_ms':max(0,first-up-sum(b-a for a,b in intervals)/1e6),'maximum_exclusive_by_tag':max_by_tag,'navigation_scopes':sum(r['kind']==2 and r['tag']==12 for r in nav_records),'navigation_geometry_build_scopes':sum(r['kind']==2 and r['tag']==21 for r in nav_records),'actual_NAV_submit_records':submitted,'canonical_draw_provider_matches':accepts,'navigation_timing_after_Up':nav_timings,'canonical_frame_timing_after_Up':frame_timings,'Worker':{'route':d.get('cpuFacts',{}).get('error')or'no eraser request in append row','compute_ms':None,'reason':'No Worker job timing captured in this material-ineligible eraser/append workload; not fabricated zero.'},'GPU_execution_ms':None,'GPU_note':'Only CPU-record/flush/completion-proxy trace. Exclusive render does not distinguish GPU execution or driver waits.'})
  diagnostic.append(item)
 write('baseline-and-attribution.json',{'classification':'OBSERVED_ENGINEERING_FIXTURE_NOT_EXACT_USER_REPRODUCTION','ordinary_rows':ordinary,'diagnostic_rows':[{'id':r['id'],'raw':r['raw'],'first_content_proxy_after_NAV_handler_ms':r['first_content_proxy_after_NAV_handler_ms'],'qualified_completion_count':r['qualified_completion_count']}for r in diagnostic],'selected_targets':[read(RUN/'diag-append-target-target.json'),read(RUN/'diag-erase-target-target.json')],'findings':['Settled zoom-in writes quickly (1.4ms append /12ms erase); settled zoom-out/pan first GL content 887–1362ms.','Immediate in also waits; the user immediate direction distinction is not fully reproduced.','Append Up→first NAV task has about70–72ms queue; erase about28–30ms plus174–213ms Up dispatch.','GL content proxy is not physical or camera-qualified presentation.']})
 write('phase-accounting.json',{'classification':'OFFLINE_EXISTING_TRACE_ATTRIBUTION','diagnostic_rows':diagnostic,'no_double_count':'Spans retain inclusive and exclusive values. Queue sums use interval union, not inclusive nested sum. Worker-exterior time is not all CPU.','unresolved':['Full operation/Scene/owner qualification missing in current trace window; no synthesized setup identities.','Exact physical present/GPU execution duration unavailable.','Native timing console held by host, so JSON lines are used offline and not equated to primary normal latency.']})
 return ordinary,diagnostic
if __name__=='__main__':analyze()
