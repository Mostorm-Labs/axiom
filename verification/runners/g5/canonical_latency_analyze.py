from pathlib import Path
import json,gzip,collections,hashlib
root=Path.cwd();out=root/'out/G5-canonical-convergence-latency-r1';res=root/'.aegis/results/G5-canonical-convergence-latency-r1'
def raw(d):
 p=d/'raw.json';return json.loads(p.read_bytes()if p.exists()else gzip.decompress((d/'raw.json.gz').read_bytes()))
rows=[]
for role in ['BASELINE','CANDIDATE']:
 d=out/'web'/('CL-D4-PRIMARY-'+role)
 if not(d/'worker-result.json').exists():continue
 r=raw(d);Clo=r['stages']['C-Up']['at'];Chi=r['stages']['followup-erase']['at'];jobs=[j for j in r['worker']['jobs']if j.get('at',-1)>=Clo and j.get('at',1e100)<=Chi]
 # Actual witness fieldnames preserved; explicit match only when currentjsonschema known.
 worker=r['worker'];events=r['events'];a_up=next(i for i,e in enumerate(events)if e.get('phase')=='up');evA=events[a_up];evB=events[a_up+1]
 raf=[v for v in r['observation']['raf']if Clo<=v['started']<=Chi]
 snapshots={s:hashlib.sha256(gzip.decompress((d/(s+'.axsnap.gz')).read_bytes())).hexdigest()for s in ['AB','undo','redo','followup']}
 keys={j['key']for j in r['worker']['jobs']if Clo<=j['at']<=Chi};issued={j['key']:j for j in r['worker']['jobs']if j['key']in keys};terminals=[t for t in r['worker']['terminals']if t['key']in keys]
 workerC={'jobs':len(issued),'first_issue_after_Up_ms':min(j['at']for j in issued.values())-Clo,'last_terminal_after_Up_ms':max(t['at']for t in terminals)-Clo,'after_last_terminal_ms':Chi-max(t['at']for t in terminals),'sum_compute_ms':sum(t.get('computeMs',0)for t in terminals),'sum_envelope_ms':sum(t['at']-issued[t['key']]['at']for t in terminals),'sum_yield_ms':sum(t.get('yieldMs',0)for t in terminals),'envelope_is_not_all_compute':True}
 row={'role':role,'status':r['status'],'input_down_to_up_ms':r['A_down_to_up_ms'],'AB_up_to_drain_ms':r['up_to_drain_ms'],'C_down_to_drain_ms':r['followup_erase_ms'],'C_up_to_drain_ms':Chi-Clo,'A_Up_handler_ms':evA['end']-evA['at'],'B_Down_handler_ms':evB['end']-evB['at'],'B_Down_after_A_Up_return_ms':evB['at']-evA['end'],'canonical_AB_receipt_notification_ms':r.get('first_AB_present_ms'),'operation_notification_A_ms':r.get('first_A_commit_ms'),'selection_ms':r['masked_selection_ms'],'Worker_C':workerC,'C_worstRAF_ms':max((v['ended']-v['started']for v in raf),default=0),'C_RAF_inside_ms':sum(v['ended']-v['started']for v in raf),'C_RAF_outside_ms':Chi-Clo-sum(v['ended']-v['started']for v in raf),'snapshots':snapshots,'operations':len(r['operations']),'workerjobs':len(worker['jobs']),'errors':r['errors'],'closed':r.get('closed'),'source':str(d.relative_to(root)),'observationloss':r['observation']['loss']}
 rows.append(row)
report={'rows':rows,'no_statistical_performance_PASS':True,'nested_accounting':'ordinaryhasno nativephase; onOperationnotification !=atomiccommit; RAFoutside !=GPUwait','known_image_qualification':'independent302open notusedforprimarypurev4'}
if len(rows)==2:report['paired']={k:{'baseline':rows[0][k],'candidate':rows[1][k],'percent':(rows[1][k]/rows[0][k]-1)*100}for k in ['input_down_to_up_ms','AB_up_to_drain_ms','C_up_to_drain_ms','C_down_to_drain_ms','C_worstRAF_ms']}
(res/'paired-performance.json').write_text(json.dumps(report,indent=2)+'\n')
zoom=[]
for role in ['BASELINE','CANDIDATE']:
 base=out/'web'/('CL-D4-MIXED-'+role)
 if not(base/'worker-result.json').exists():continue
 for d in base.iterdir():
  if not d.is_dir():continue
  r=raw(d);lo=r['navigation'][0]['started'];hi=r['feedback']['at'];raf=[v for v in r['observation']['rafCallbacks']if lo<=v['started']<=hi];dur=sorted(v['ended']-v['started']for v in raf)
  zoom.append({'role':role,'row':d.name,'status':r['status'],'Up_handler_ms':next(e for e in reversed(r['events'])if e['phase']=='up')['ended']-next(e for e in reversed(r['events'])if e['phase']=='up')['started'],'Up_to_firstwheel_ms':lo-r['upReturned'],'input_queue_delays_ms':[e['started']-e['planned']for e in r['navigation']],'wheel_handlers_ms':[e['ended']-e['started']for e in r['navigation']],'firstGL_proxy_ms':r['proxy']['firstDefaultFboContentAt']-lo,'canonical_currentcamera_feedback_ms':hi-lo,'Up_to_settled_ms':r['settledAt']-r['upReturned'],'RAFs':len(raf),'RAFs_sum_ms':sum(dur),'RAF_p95_ms':dur[int(len(dur)*.95)]if dur else None,'RAF_max_ms':max(dur,default=0),'outside_RAF_ms':hi-lo-sum(dur),'errors':r['errors'],'closed':r['closed'],'camera':r['states']['after']['camera'],'feedback_scope':'all6wheels+canonicalgeneration/currentcamera onFrame; earliestLOD notqualified','source':str(d.relative_to(root))})
(res/'zoomout-results.json').write_text(json.dumps({'rows':zoom,'imagequalification':'open302, canonical_latency_guard required','singlepair':True},indent=2)+'\n')
print(json.dumps(report,indent=2));print(json.dumps(zoom,indent=2))
