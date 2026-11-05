from pathlib import Path
import json,gzip,statistics,hashlib
O=Path('out')
read=lambda p:json.loads(p.read_text())
raw=lambda p:json.loads(gzip.decompress(p.read_bytes()))
before=read(O/'g5-large-before/matrix.json');after=read(O/'g5-large-after/matrix.json')
assert before['sourceStable'] and after['sourceStable']
assert len(before['rows'])==len(after['rows'])==8
pairs=[]
for old,new in zip(before['rows'],after['rows']):
 assert old['id']==new['id'] and old['fixture_sha256']==new['fixture_sha256']
 assert old['classification']==new['classification']=='MEASURED'
 assert old['worker']['cleanup_complete'] and new['worker']['cleanup_complete']
 p={'id':old['id'],'case':old['case'],'side':old['side'],'phases':[]}
 for a,b in zip(old['phases'],new['phases']):
  assert a['pixels']['sha256']==b['pixels']['sha256'],(old['id'],a['id'],'pixel drift')
  p['phases'].append({'id':a['id'],'pixels_equal':True,'before':a['summary'],'after':b['summary']})
 pairs.append(p)
regression={}
for side in ['on','off']:
 values=lambda stage:[p[stage]['input_display_submission_gap_ms']['max'] for r in pairs if r['case']=='million' and r['side']==side for p in r['phases']]
 a,b=values('before'),values('after');ratio=statistics.median(b)/statistics.median(a)
 regression[side]={'before_ms':a,'after_ms':b,'before_median_ms':statistics.median(a),'after_median_ms':statistics.median(b),'ratio':ratio,'meets_task_reduction':ratio<=.5}
 assert ratio<=.5,(side,ratio)
edits=[]
for kind in ['transform','erase','text','table','resource']:
 records={side:raw(O/('g5-large-edits/'+kind+'-'+side+'/observation.json.gz')) for side in ['on','off']}
 for side,b in records.items():
  assert b['status']=='completed' and b['sourceStable'] and b['artifactStable']
  baseline=raw(O/('g5-step4-matrix/edit-'+kind+'-'+('diagnostic' if side=='on' else 'default')+'/observation.json.gz'))
  assert all(b['checks'].values())
  for phase in ['before','after']:assert b[phase]['image']['sha256']==baseline[phase]['image']['sha256'],(kind,side,phase)
 for phase in ['before','after']:assert records['on'][phase]['image']['sha256']==records['off'][phase]['image']['sha256']
 edits.append({'kind':kind,'canonical_export_pixels_equal_to_baseline_and_on_off':True,'checks':{s:b['checks'] for s,b in records.items()},'phases':{s:[{'id':p['id'],'summary':p['summary']} for p in b['phases']] for s,b in records.items()}})
result={'schema':'g5-large-navigation-comparison-v1','scope':'engineering regression, no product SLO','regression':regression,'pairs':pairs,'edits':edits}
(O/'g5-large-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(regression,indent=2))
for case in ['single100k','dense10k']:
 print(case,[(r['side'],[(p['id'],round(p['before']['input_display_submission_gap_ms']['max'],1),round(p['after']['input_display_submission_gap_ms']['max'],1),round(p['before']['settle_ms'],1),round(p['after']['settle_ms'],1)) for p in r['phases']]) for r in pairs if r['case']==case])
