"""Independent common-wrapper overhead calculation from recorded deliveries."""
from pathlib import Path
import json,math,statistics
R=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
B=R/'verification/evidence/performance/g5-unified-render-ur0b/dade34ea89c46625291c9aa39a8cbffb56b3df72fa12eac07f4048cb011af2a5/resume-r3/execution'
load=lambda p:json.loads(p.read_text())
archived=load(B/'qualification-pairs.json');pairs=[]
for n in range(1,7):
 pair={'pair':n};runs={}
 for label in ['A','B']:
  run=load(B/f'raw-runs/qualification-p{n}-{label}/worker-result.json');m=run['measurement'];planned=[e for e in m['planned'] if e['kind'] in ['pointer','wheel']];values=[]
  for e in planned:
   found=[h for h in m['handlers'] if h['event_id']==e['event_id']];assert len(found)==1 and found[0]['enter_ns'] is not None and found[0]['exit_ns'] is not None
   elapsed=int(found[0]['exit_ns'])-int(found[0]['enter_ns']);assert elapsed>=0;values.append(elapsed)
  values.sort();stats={'p95_ns':str(values[math.ceil(.95*len(values))-1]),'wall_ns':str(int(m['end_ns'])-int(m['origin_ns'])),'dispatch_population':len(planned),'critical_loss':run['trace']['loss']['critical']}
  pair['off' if label=='A' else 'on']=stats;runs[label]=run
  assert stats==archived[n-1]['off' if label=='A' else 'on']
 assert runs['A']['artifacts_before']==runs['B']['artifacts_before']
 assert runs['A']['final']['snapshot']['sha256']==runs['B']['final']['snapshot']['sha256']
 assert runs['A']['measurement']['after']['activeContacts']==runs['B']['measurement']['after']['activeContacts']==0
 assert [e['error'] for e in runs['A']['measurement']['errors']]==[e['error'] for e in runs['B']['measurement']['errors']]
 pairs.append(pair)
fields={}
for name in ['p95_ns','wall_ns']:
 off=[int(p['off'][name]) for p in pairs];diff=[int(p['on'][name])-int(p['off'][name]) for p in pairs];center=statistics.median(off);mad=statistics.median([abs(x-center) for x in off]);delta=statistics.median(diff);bound=max(1000000 if name=='p95_ns' else 20000000,.05*center)
 fields[name]={'off_median_ns':center,'off_mad_ns':mad,'paired_median_increment_ns':delta,'limit_ns':bound,'noisy':mad>1000000 and mad>.1*center}
expected=load(B/'overhead-qualification.json');assert expected['measurements']==fields
assert all(not f['noisy'] and f['paired_median_increment_ns']<=f['limit_ns'] for f in fields.values())
assert all(p['on']['critical_loss']==0 for p in pairs)
j={'status':'QUALIFIED_OBSERVATION_OVERHEAD_ONLY','independent_common_metrics_from_raw_handler_plans':True,'pairs':pairs,'measurements':fields,'product_performance_pass':False,'new_browser_workers':0}
(D/'overhead-check.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps({k:v for k,v in j.items() if k!='pairs'}))
