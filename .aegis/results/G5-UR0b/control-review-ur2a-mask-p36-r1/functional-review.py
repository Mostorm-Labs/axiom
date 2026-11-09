import pathlib,json,re,math,hashlib,subprocess
R=pathlib.Path.cwd();B=R/'.aegis/results/G5-UR2a-mask-isolation-p36-r1';O=pathlib.Path(__file__).resolve().parent
read=lambda p:json.loads(p.read_text())
text=(R/'runtime/foundation/include/canvas/foundation/internal/g5_observation.hpp').read_text()
tag_names=re.search(r'enum class Tag[^\{]*\{(.*?)\};',text,re.S)[1].split(',')
tags={x.strip().split('=')[0].strip():i+1 for i,x in enumerate(tag_names)}
rows=[];joined=[];content=[];brush=[];led=read(B/'local-functional/sampling-ledger.json')
assert led['historical_starts']==62 and led['historical_charged_seconds']==2724 and led['original_cap_seconds']==8370
assert [x['id'] for x in led['attempts']]==['UR2AMASKR1-Q1','UR2AMASKR1-N1'] and led['remaining_status']=='NONE'
for a in led['attempts']:
 g=a['guard'];assert a['status']=='COMPLETED' and a['worker_status']=='OBSERVED'
 assert g['reason']=='completed' and g['returncode']==0 and g['cleanup_complete'] and not g['remaining_owned_pids']
 assert g['timeout_s']==360 and g['peak_process_family_rss_kib']<=3072*1024
 assert a['charged_seconds']==math.ceil(g['elapsed_s'])
 d=B/'local-functional'/a['id'];normal=a['id'].endswith('N1')
 files=sorted(d.glob('*-final.json'));assert len(files)==10
 for f in files:
  id=f.name[:-11];v=read(f);p=v['phases'][0];s=p['after'];before=p['before']
  assert not v['errors'] and s['activeContacts']==0 and v['state']['activeContacts']==0
  assert p['status']=='INPUT_COMPLETED' and p['id']==id
  expected=3 if id.endswith(('E-reerase','H-undo-before-present')) else 2 if id.endswith('B-brush-erase') else 1
  assert int(s['generation'])-int(before['generation'])==expected
  assert v['state']['lastCommit']['presentedGeneration']==v['state']['generation'] and v['activity']['pending']==0
  input_downs=[x for x in p['schedule'] if x.get('phase')=='down']
  assert input_downs and all(x['activeContacts']==1 for x in input_downs)
  if id.endswith('E-reerase'):assert len(input_downs)==3 and any(x.get('same_dom_task_after_up') for x in input_downs)
  if id.endswith(('P-pan','Z-zoom-reentry')):
   wheels=[x for x in p['inputEvents'] if x['type']=='wheel'];assert len(wheels)==(24 if id.endswith('P-pan') else 48)
   writes=[x for x in p['colorOps'] if x['kind']=='content' and x['started']>=wheels[0]['started'] and x['ended']<=wheels[-1]['ended']]
   assert writes
   exact=[x for x in v['frames'] if x['at']>=p['afterUp']['at'] and x['state']['lastCommit']['presentedGeneration']==p['afterUp']['state']['generation']]
   assert exact
   early=[x for x in p['colorOps'] if x['kind']=='content' and x['started']>=p['afterUp']['at'] and x['ended']<exact[0]['at'] and x['ended']<=p['inputEnded']]
   assert early
   content.append({'worker':a['id'],'case':id,'content_writes':len(writes),'writes_before_exact':len(early),'first_content_ms':writes[0]['started']-wheels[0]['started'],'not_physical_display_or_SLO':True})
  if id.endswith('B-brush-erase'):
   css=p['brushOverlay'];assert css['display']!='none' and css['visibility']=='visible' and float(css['opacity'])>0
   brush.append({'worker':a['id'],'case':id,'actual_css_visible_during_brush':True})
  rows.append({'worker':a['id'],'case':id,'expected_operation_delta':expected,'actual_operation_delta':int(s['generation'])-int(before['generation']),'contacts':s['activeContacts'],'dirty_settled':True})
  trace=read(d/(id+'-trace.json'))
  if normal:assert trace==[];continue
  records=[]
  for st in trace:
   for c in st['chunks']:
    assert not c.get('loss',{}).get('critical',0);records+=c.get('records',[])
  commits={};headers={};rosters={};submitted={};qualified={};outcomes=[]
  for r in records:
   v=r.get('values',[]);t=r['tag'];k=r['kind']
   if t==tags['display_record'] and k==6:
    assert len(v)==8;commits[v[0]]=r
   elif t==tags['display_roster'] and k==5:
    if len(v)==8:headers[v[0]]=r;rosters[v[0]]=set()
    elif len(v)==2:assert v[0] in headers;rosters[v[0]].add(v[1])
   elif t==tags['display_roster'] and k==11:
    assert len(v)==6;key=tuple(v[1:5]);assert v[0] in headers
    assert len(rosters[v[0]])==int(v[5]);submitted[key]=r
   elif t==tags['canonical_receipt'] and k==12 and v[0]=='1':
    assert len(v)==11;key=(v[2],v[3],v[4],v[1]);qualified[key]=r
   elif t==tags['display_record'] and k==12:
    assert len(v)==11 and v[1] in ('0','1','2')
    key=tuple(v[2:6]);c=commits[v[0]];submit=submitted[key];complete=qualified[key];frame=headers[v[6]]
    assert v[0] in rosters[v[6]] and submit['values'][0]==v[6]==complete['values'][6]
    assert c['values'][2:6]==v[7:11] and frame['values'][1:5]==complete['values'][7:11]
    assert int(c['at_ns'])<=int(frame['at_ns'])<=int(submit['at_ns'])<=int(complete['at_ns'])<=int(r['at_ns'])
    if v[1]=='0':assert c['values'][6]==complete['values'][8]
    outcomes.append({'serial':v[0],'outcome':['OwnCanonicalVisible','ReplacedByCurrentFrame','SupersededByCurrentState'][int(v[1])],'work':key,'frame':v[6],'own_visible_ns':r['at_ns'] if v[1]=='0' else None,'qualified_replacement_ns':None if v[1]=='0' else r['at_ns']})
  assert outcomes;joined.append({'case':id,'qualified_outcomes':outcomes,'method':'Independent chronological raw-event traversal, not producer join verdict'})
assert len(rows)==20 and len(content)==8 and len(brush)==4 and len(joined)==10
assert led['cumulative_starts']==64 and led['cumulative_charged_seconds']==2724+sum(a['charged_seconds'] for a in led['attempts'])==3118
out={'status':'QUALIFIED','cases':rows,'navigation':content,'brush_visibility':brush,'qualified_joins':joined,'browser_starts':0,'oracle_invocations':0,'cumulative_starts':64,'cumulative_seconds':3118,'remaining_seconds':8370-3118,'remaining_ids':[]}
(O/'functional-review.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps({'status':out['status'],'cases':len(rows),'camera_cases_before_exact':len(content),'Brush_actual_visibility_cases':len(brush),'qualified_private_join_cases':len(joined),'new_browser_starts':0,'remaining_seconds':out['remaining_seconds']}))
