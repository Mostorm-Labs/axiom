from pathlib import Path
import json,hashlib
R=Path(__file__).resolve().parents[3];B=Path(__file__).resolve().parent
read=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();ref=lambda p:{'path':str(p.relative_to(R)),'sha256':sha(p),'bytes':p.stat().st_size}
ledger=read(B/'local-web/sampling-ledger.json');s1=read(B/'local-web/UR2CRB3-S1/worker-result.json');np=B/'local-web/UR2CRB3-N1/worker-result.json';n=read(np)if np.exists()else{'status':'MISSING','cases':[]}
fail=[];rows=[];counts={'normal_cases':0,'completed_cases':0,'cold_stages_qualified':0,'full_surface_buffers':0,'live_ROI_buffers':0,'roundtrips_qualified':0,'baseline_ROI_buffers':0};qualified=[];baseline=[]
def meta(m,role,target,contextWidth,contextHeight):
 assert m['status']=='VALID'and m['role']==role and m['canvas']['id']==target
 assert m['selected']['readFramebuffer']=='DEFAULT'and m['selected']['readBuffer']==1029 and m['selected']['pixelPackBuffer']=='NONE'
 assert m['selected']['pack']=={'PACK_ALIGNMENT':1,'PACK_ROW_LENGTH':0,'PACK_SKIP_ROWS':0,'PACK_SKIP_PIXELS':0}
 assert all(not m['errors'][p]['codes']and not m['errors'][p]['truncated']for p in ['preexisting','setup','read','restore'])
 assert m['restoration']['status']=='RESTORED'and m['before']==m['after']and m['defaultReadBufferBefore']==m['defaultReadBufferAfter']
 assert not m['context']['lost']and m['canvas']['width']==m['context']['drawingBufferWidth']==contextWidth and m['canvas']['height']==m['context']['drawingBufferHeight']==contextHeight
for c in n['cases']:
 counts['normal_cases']+=1;counts['completed_cases']+=c['status']=='COMPLETED';row={'id':c['id'],'status':c['status'],'failure':c.get('blocking_class'),'errors':c.get('failures',[]),'error':c.get('error'),'failure_captures':c.get('failure_captures',[])};rows.append(row)
 if c['status']!='COMPLETED':fail.append(c['id']+':'+c['status'])
 for stage in c.get('cold',[]):
  counts['cold_stages_qualified']+=stage.get('oracle',{}).get('status')=='QUALIFIED'
  for role,image in stage['surfaces'].items():
   p=B/'local-web/UR2CRB3-N1'/(c['id']+'-'+stage['stage']+'.'+role+'.rgba');assert p.stat().st_size==image['length']and sha(p)==image['sha256'];counts['full_surface_buffers']+=1
   try:meta(image['readback'],role,image['target'],image['width'],image['height']);qualified.append({'case':c['id'],'stage':stage['stage'],'role':role,'readback':image['readback'],'raw':ref(p)})
   except Exception as e:fail.append(c['id']+':full target metadata '+str(e))
 counts['roundtrips_qualified']+=c.get('roundtrip',{}).get('status')=='QUALIFIED'
 if c.get('normal_only'):
  v=c.get('partial',c)
  for image in v.get('live',[]):
   p=B/'local-web/UR2CRB3-N1'/(c['id']+'-'+image['key']+'.roi.rgba')
   if 'sha256'in image:assert p.stat().st_size==image['length']and sha(p)==image['sha256'];counts['live_ROI_buffers']+=1
   else:continue
   try:meta(image['readback'],'canonical',image['target'],800*c['init']['dpr'],500*c['init']['dpr']);qualified.append({'case':c['id'],'stage':image['stage'],'role':'canonical','readback':image['readback'],'raw':ref(p)})
   except Exception as e:fail.append(c['id']+':ROI target metadata '+str(e))
  basis=[e for e in v.get('schedule',[])if e.get('kind')=='ROI-basis']
  for e,index in zip(basis,[0,6,18]):
   p=B/'local-web/UR2CRB3-N1'/(c['id']+'-baseline-'+str(index)+'.roi.rgba');assert p.exists();counts['baseline_ROI_buffers']+=1
   a=p.read_bytes();interior=sum(all(abs(a[i+j]-v)<=2 for j,v in enumerate([13,26,51,255]))for i in range(0,len(a),4));assert interior==e['qualifier']['interior']and e['qualifier']['status']=='PRESERVED'
   last=e is basis[-1];recorded=v.get('roi',{}).get('readback')if last else None
   if recorded:meta(recorded,'canonical',v['live'][0]['target'],800*c['init']['dpr'],500*c['init']['dpr'])
   baseline.append({'case':c['id'],'stage':e['stage'],'raw':ref(p),'camera':e['camera'],'qualifier':e['qualifier'],'readback':recorded,'witness_kind':'RECORDED_METADATA'if recorded else'READER_SUCCESS_ASSERTION_DERIVATION','derivation':None if recorded else'qualified() must receive status VALID before returning these bytes; shared reader checks explicit DEFAULT/BACK/nullPBO/tightPACK, errors and restoration equality on every call. Exact initial/intermediate numeric before/after GL state objects were returned but not individually retained after row.roi was replaced. They are not fabricated here.'})
if s1['status']!='OBSERVED'or len(s1['cases'])!=8:fail.append('S1 qualification missing')
if n['status']!='OBSERVED'or not n.get('source_stable')or not n.get('artifacts_stable'):fail.append('N1 failed/unstable')
if counts['normal_cases']!=16 or counts['completed_cases']!=16 or counts['cold_stages_qualified']!=64 or counts['full_surface_buffers']!=128 or counts['live_ROI_buffers']!=162 or counts['roundtrips_qualified']!=16:fail.append('normal frozen matrix incomplete')
if ledger['remaining_status']!='NONE'or len(ledger['attempts'])!=2:fail.append('closed window ledger incomplete')
proof={'status':'VERIFIED'if not fail else'BLOCKED','counts':counts,'qualified_observations':qualified,'baseline_witnesses':baseline,'normal_cases':rows,'selftest':ref(B/'local-web/UR2CRB3-S1/worker-result.json'),'failures':fail,'old_R2_zero_classification':'UNKNOWN_TARGET; newS1 proves oldreader can selectwrongtarget. New N1 does not retroactively identify old framebuffer.','baseline_metadata_disclosure':'Six final baseline metadata records are retained directly;12initial/intermediate baseline selections/restoration are source-assertion-derived witnesses. No numeric before/after state values are invented. All128full+162comparisonROI observations retain complete state metadata.','classification':'TARGET_READBACK_CORRECTNESS_ONLY_NO_PERFORMANCE_OR_GATE_PASS'}
(B/'target-evidence.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({'status':proof['status'],'counts':counts,'failures':fail}))
