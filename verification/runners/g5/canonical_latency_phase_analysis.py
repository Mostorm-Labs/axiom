from pathlib import Path
import json,collections,gzip
root=Path.cwd();out=root/'out/G5-canonical-convergence-latency-r1';res=root/'.aegis/results/G5-canonical-convergence-latency-r1';r=json.loads(gzip.decompress((out/'web/CL-D4-PRIMARY-DIAGNOSTIC/raw.json.gz').read_bytes()))
rows=[]
for v in r['observation']['native']:
 if not v['text'].startswith('AXIOM_FRAME_TIMING '):continue
 x=json.loads(v['text'][19:]);x['js_at']=v['at'];rows.append(x)
phases={}
for name,lo,hi in [('AB',r['stages']['A-Up']['at'],r['stages']['AB-settled']['at']),('C',r['stages']['C-Up']['at'],r['stages']['followup-erase']['at'])]:
 selected=[x for x in rows if lo<=x['js_at']<=hi];roles={}
 for role in sorted(set(x['role']for x in selected)):
  records=[x for x in selected if x['role']==role];stages=collections.defaultdict(lambda:collections.Counter());counts=collections.Counter();maxima=collections.Counter()
  for record in records:
   for stage,v in record['stages'].items():stages[stage].update(v)
   for key,v in record['counts'].items():
    if key.startswith('resource_'):maxima[key]=max(maxima[key],v)
    else:counts[key]+=v
  roles[role]={'rows':len(records),'stages_ms':{s:{k:(v/1e6 if k.endswith('_ns')else v)for k,v in t.items()}for s,t in stages.items()},'counts':dict(counts),'maxima':dict(maxima),'outcomes':dict(collections.Counter(x['outcome']for x in records))}
 phases[name]={'lo':lo,'hi':hi,'wall_ms':hi-lo,'roles':roles}
result={'primary':phases,'observation_loss':r['observation']['loss'],'cautions':['owner/canonicalarenestedviews; donotsumroles','prepared_select/draw perstepinstrumentation overhead; diagnoseworknotnormalperformance','historydrawprimitives=offeredsegments/Filledrecord1, notactualGPUdraw','resetcountsincludeemptyresetsnotlostprogress']}
(res/'diagnostic-phase-accounting.json').write_text(json.dumps(result,indent=2)+'\n')
for phase,v in phases.items():
 print(phase,'wall',v['wall_ms'])
 for role,d in v['roles'].items():
  print(role,d['rows']);print(sorted([(k,round(s['exclusive_ns'],2))for k,s in d['stages_ms'].items()],key=lambda x:-x[1])[:14]);print({k:v for k,v in d['counts'].items()if v and k in ['coverage_steps','body_replay','history_draw_primitives','validation_mask_primitives','material_alias_hits','prepare_resets','body_builds','body_compile','gpu_coverage_tiles','render_alloc_denials','render_pending']},d['maxima'])

for f in (out/'web/CL-D4-MIXED-DIAGNOSTIC').glob('*/raw.json.gz'):
 r=json.loads(gzip.decompress(f.read_bytes()));lo=r['navigation'][0]['started'];hi=r['settledAt'];roles={}
 for v in r['phaseObservation']['native']:
  if not lo<=v['at']<=hi or not v['text'].startswith('AXIOM_FRAME_TIMING '):continue
  x=json.loads(v['text'][19:]);d=roles.setdefault(x['role'],{'rows':0,'exclusive_ms':collections.Counter(),'counts':collections.Counter(),'resources':collections.Counter()});d['rows']+=1
  for n,z in x['stages'].items():d['exclusive_ms'][n]+=z['exclusive_ns']/1e6
  for n,c in x['counts'].items():
   if n.startswith('resource_'):d['resources'][n]=max(c,d['resources'][n])
   else:d['counts'][n]+=c
 result.setdefault('mixed',{})[f.parent.name]={'wall_ms':hi-lo,'roles':roles,'loss':r['phaseObservation']['loss']}
(res/'diagnostic-phase-accounting.json').write_text(json.dumps(result,indent=2)+'\n')
