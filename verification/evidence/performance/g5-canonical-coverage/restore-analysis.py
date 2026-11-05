import gzip,json
from pathlib import Path
rows=[]
for mode in ['on','off']:
 for case in ['dense-zoom-pan','dense-selected-zoom-pan']:
  for side in ['before','after']:
   root=Path('out/g5-canonical-matrix-'+mode)/(case+'-'+side)
   r=json.loads(gzip.decompress((root/'wheel.json.gz').read_bytes()))
   p=next(p for p in r['phases'] if p['id']=='after-freeze');raw=p['raw'];ts=[raw['started']]+[t for t in raw['displayTimes'] if raw['started']<=t<=raw['inputEnded']]+[raw['inputEnded']]
   a,b=max(zip(ts,ts[1:]),key=lambda x:x[1]-x[0]);samples=[s for s in raw['samples'] if a<=s['at']<=b+40]
   rows.append({'mode':mode,'case':case,'side':side,'gap_ms':b-a,'gap_start_from_phase_ms':a-raw['started'],'gap_end_from_phase_ms':b-raw['started'],
    'samples_near_gap':[{'index':s['index'],'raf_interval_ms':s['interval'],'dispatch_ms':s['dispatchMs'],'color_submissions':s['draws']} for s in samples],
    'exact_wait_ms':p['summary']['settle_ms'],'navigation_cpu':(p['summary']['navigation'] or {}).get('cpu_ms')})
recheck=[]
for side in ['after','before']:
 s=json.loads(Path('out/g5-canonical-restore-recheck-'+side+'/summary.json').read_text());r=s['results'][0];p=next(p for p in r['phases'] if p['id']=='after-freeze')
 recheck.append({'side':side,'status':s['status'],'gap_ms':p['summary']['input_display_submission_gap_ms']['max'],'exact_wait_ms':p['summary']['settle_ms'],'source_stable':s['sourceStable'],'artifact_stable':s['artifactStable'],'all_state_checks':all(r['checks'].values()),'final_pixels_sha256':p['pixels']['sha256']})
result={'status':'OPEN_NAVIGATION_CONTINUITY_FINDING','original_metrics_unchanged':True,'matrix':rows,'reverse_order_default_selected_60s_idle':recheck,
 'observed':'Several restore zoom-to-pan intervals have no color submissions despite continued RAF/input. Reverse order reproduces the larger candidate gap while exact completion is much faster.',
 'causal_limit':'This demonstrates an unresolved continuity guard; it does not identify GPU execution or a staging-lifetime cause. Navigation CPU alone is insufficient to explain the draw-submission gap.',
 'next_owner':'Shared Runtime/Render navigation frontier coverage/presentation and scheduler. Keep canonical A8 strict pixels and budgets; no JS renderer or arbitrary larger per-frame budget.'}
Path('out/g5-canonical-restore-analysis.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'recheck':recheck}))
