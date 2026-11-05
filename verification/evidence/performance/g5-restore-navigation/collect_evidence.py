import gzip,hashlib,json,re,shutil,statistics
from pathlib import Path

DEST=Path('verification/evidence/performance/g5-restore-navigation')
REV='0a1bd10338dc2ebda5d34691352b3d99278b0dd3'
BASE='1a39d1a0de65d7f22270c771160ae96b246d062c'
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(src,dst):
 src=Path(src);dst=DEST/dst;dst.parent.mkdir(parents=True,exist_ok=True)
 if src.suffix=='.log' or (src.suffix=='.json' and src.stat().st_size>131072):
  dst=dst.with_suffix(dst.suffix+'.gz');dst.write_bytes(gzip.compress(src.read_bytes(),mtime=0))
 else:shutil.copyfile(src,dst)
DEST.mkdir(parents=True,exist_ok=True)
correct=True;comparisons=[];perf={};inputs={};full_source=None
for mode in ['off','on']:
 root=Path('out/g5-restore-matrix-'+mode);matrix=read(root/'matrix.json')
 for p in root.rglob('*'):
  if p.is_file():copy(p,Path(mode)/p.relative_to(root))
 copy(str(root)+'.log',mode+'/driver.log')
 rows=matrix['results'];correct &= len(rows)==8 and all(r['status']=='MEASURED' for r in rows)
 freeze=[[],[]];idle=[[],[]];warm=[[],[]];wait=[];coverage=[]
 for case in sorted({r['case'] for r in rows}):
  a=next(r for r in rows if r['case']==case and r['side']=='before');b=next(r for r in rows if r['case']==case and r['side']=='after')
  old=read(root/(case+'-before')/'summary.json');new=read(root/(case+'-after')/'summary.json')
  full_source=new['sourceBefore']
  inputs[mode+'/'+case]={k:full_source[k] for k in ['commit','dirty','tree_sha256']}
  correct &= all(s['sourceStable'] and s['artifactStable'] and all(s['results'][0]['checks'].values()) and s['results'][0]['setupQuiescence']['stopped'] for s in [old,new])
  correct &= a['setup']['build']['sourceRevision']==BASE and b['setup']['build']['sourceRevision']==REV
  correct &= a['setup']['initialPixels']['sha256']==b['setup']['initialPixels']['sha256']
  phases=[]
  for p,q in zip(a['phases'],b['phases']):
   s,t=p['summary'],q['summary'];same=p['pixels']['sha256']==q['pixels']['sha256'];correct &= same
   phases.append({'id':p['id'],'same_full_pixels':same,
    'before_gap_ms':s['input_display_submission_gap_ms']['max'],'after_gap_ms':t['input_display_submission_gap_ms']['max'],
    'before_wait_ms':s['settle_ms'],'after_wait_ms':t['settle_ms'],
    'before_quality':(s['navigation'] or {}).get('quality'),'after_quality':(t['navigation'] or {}).get('quality'),
    'after_mask_encoding':(t['navigation'] or {}).get('maskEncoding'),'after_components_ms':(t['navigation'] or {}).get('componentsMs'),
    'after_region_compactions':(t['navigation'] or {}).get('regionCompactions')})
   if 'zoom' in case:
    for i,x in enumerate([s,t]):
     gap=x['input_display_submission_gap_ms']['max']
     if p['id']=='after-freeze':freeze[i].append(gap)
     if p['id']=='after-idle':idle[i].append(gap)
     if p['id'] in ['repeat-0','repeat-1']:warm[i].append(gap)
    if p['id'] in ['repeat-0','repeat-1']:
     wait.append(t['settle_ms'])
     if mode=='on':coverage.append(t['exactStages']['cpu_coverage']['inclusive_ms'])
  comparisons.append({'mode':mode,'case':case,'phases':phases})
 metrics=lambda x:{'before':statistics.median(x[0]),'after':statistics.median(x[1]),'before_samples':x[0],'after_samples':x[1]}
 perf[mode]={'restored_gap_ms':metrics(freeze),'foreground_idle_gap_ms':metrics(idle),'warm_gap_ms':metrics(warm),
  'warm_exact_wait_median_ms':statistics.median(wait),'warm_exact_coverage_median_ms':statistics.median(coverage) if coverage else None}
 if mode=='off':
  m=perf[mode]['restored_gap_ms'];perf[mode]['restore_150ms_and_60percent']=m['after']<=150 and m['after']<=m['before']*.6
 else:perf[mode]['canonical_80ms_guard']=statistics.median(coverage)<=80
 perf[mode]['canonical_300ms_guard']=statistics.median(wait)<=300
 perf[mode]['warm_no_regression_guard']=perf[mode]['warm_gap_ms']['after']<=perf[mode]['warm_gap_ms']['before']

for name in ['prototype','diagnostic','motion','compact','dependency']:
 root=Path('out/g5-restore-'+name)
 for p in root.rglob('*'):
  if p.is_file():copy(p,Path('exploratory')/name/p.relative_to(root))
 for p in Path('out').glob('g5-restore-'+name+'-worker.*'):
  if p.is_file():copy(p,Path('exploratory')/name/p.name)

for p in Path('out').glob('g5-restore-gpu-*'):
 if p.is_file():copy(p,Path('gpu')/p.name)
for pattern in ['*red*.log','*green*.log','*tests.log','*native-build.log','final-*.log','*compaction*build.log']:
 for p in Path('out').glob('g5-restore-'+pattern):
  if p.is_file():copy(p,Path('checks')/p.name)
for name in ['native-comparison.json','package-checks.json','gpu-inputs.json','demo-smoke.json','demo.png','prefilled.png','demo-smoke.mjs','demo-smoke-worker.worker.json']:
 copy('out/g5-restore-'+name,Path('checks' if name in ['native-comparison.json','package-checks.json','gpu-inputs.json'] else 'demo')/name)
copy('out/g5-dense-native-targets.json','checks/native-targets.json')
copy('out/g5-dense-native-test-names.txt','checks/native-test-names.txt')

recheck=[]
for side in ['after','before']:
 root=Path('out/g5-restore-recheck-'+side)
 for p in root.rglob('*'):
  if p.is_file():copy(p,Path('recheck')/side/p.relative_to(root))
 for p in Path('out').glob('g5-restore-recheck-'+side+'-worker.*'):
  if p.is_file():copy(p,Path('recheck')/side/p.name)
 s=read(root/'summary.json');correct &= s['status']=='MEASURED' and s['sourceStable'] and s['artifactStable'] and all(s['results'][0]['checks'].values())
 recheck.append({'side':side,'phases':s['results'][0]['phases']})
ar=next(r for r in recheck if r['side']=='after');br=next(r for r in recheck if r['side']=='before')
correct &= all(a['pixels']['sha256']==b['pixels']['sha256'] for a,b in zip(ar['phases'],br['phases']))
ag=next(p['summary']['input_display_submission_gap_ms']['max'] for p in ar['phases'] if p['id']=='after-freeze')
bg=next(p['summary']['input_display_submission_gap_ms']['max'] for p in br['phases'] if p['id']=='after-freeze')
reverse_guard=ag<=150 and ag<=bg*.6

native=read('out/g5-restore-native-comparison.json');gpu=read('out/g5-restore-gpu-results.json');package=read('out/g5-restore-package-checks.json');demo=read('out/g5-restore-demo-smoke.json')
gpu_source=read('out/g5-restore-gpu-inputs.json')
correct &= not native['new_failures'] and len(gpu)==7 and all(r['status']=='PASS' for r in gpu) and package['passed'] and demo['passed'] and gpu_source['same_runtime_sdk_schema_inputs_as_prototype'] and gpu_source['implementation_revision']==REV
performance=perf['off']['restore_150ms_and_60percent'] and all(perf[m]['canonical_300ms_guard'] and perf[m]['warm_no_regression_guard'] for m in ['off','on']) and perf['on']['canonical_80ms_guard'] and reverse_guard
result={'status':'READY_FOR_CONTROL_REVIEW' if correct and performance else 'BLOCKED_PERFORMANCE' if correct else 'BLOCKED_EVIDENCE',
 'scope':'Restore navigation continuity slice; no formal Gate/G5/P34/platform PASS',
 'implementation_revision':REV,'baseline_revision':BASE,'source_anchor':'2790f27ba95afd718f5c828dfd789a577b07ff9c',
 'task_package':{'path':'docs/planning/G5_RESTORE_NAVIGATION_CONTINUITY.md','sha256':sha(Path('docs/planning/G5_RESTORE_NAVIGATION_CONTINUITY.md'))},
 'official_gate_pass_claimed':False,'correctness_checks':correct,'matrix_performance_target_met':performance,
 'performance':perf,'reverse_order_recheck':recheck,'reverse_order_restore_guard':reverse_guard,'native':native,'gpu_modes':[{'mode':r['mode'],'status':r['status']} for r in gpu],
 'node_tests':int(re.search(r'# tests (\d+)',Path('out/g5-restore-final-node.log').read_text())[1]),'boundary_contracts':int(re.search(r'\((\d+) contract',Path('out/g5-restore-final-boundary.log').read_text())[1]),
 'remaining_work':['GPU resident versus staging lifetime separation remains dependent work; not implemented in this slice.',
 'Complex editing and100K/1M formal qualification remain pending.',
 'Five pre-existing native failures and physical platform evidence are not resolved by Web results.'],
 'demo':{'url':'http://127.0.0.1:4222/example.html','prefilled':'http://127.0.0.1:4222/mixed-navigation.html?dense=1','artifact':'out/g5-restore-final-off/js','preserved4219_4220_4221':True},
 'measurement_limits':'Synthetic Mac Chrome WebGL2 color submission timing, not GPU execution or physical input-to-photon; CDP freeze is not device sleep. Public frame receipt is coarse completion; exact camera/scene/surface stamps remain in ON native traces.'}
(DEST/'result.json').write_text(json.dumps(result,indent=2)+'\n')
(DEST/'comparison.json').write_text(json.dumps(comparisons,indent=2)+'\n')
(DEST/'source-inputs.json').write_text(json.dumps({'samples':inputs,'files':full_source['files']},indent=2)+'\n')
artifacts={m:{str(p.relative_to(Path('out/g5-restore-final-'+m+'/js'))):sha(p) for p in sorted(Path('out/g5-restore-final-'+m+'/js').rglob('*')) if p.is_file()} for m in ['on','off']}
(DEST/'consumer-artifacts.json').write_text(json.dumps(artifacts,indent=2)+'\n')
shutil.copyfile(__file__,DEST/'collect_evidence.py')
if Path('out/g5-restore-README.md').exists():shutil.copyfile('out/g5-restore-README.md',DEST/'README.md')
files={str(p.relative_to(DEST)):sha(p) for p in sorted(DEST.rglob('*')) if p.is_file() and p.name!='SHA256.json'}
(DEST/'SHA256.json').write_text(json.dumps(files,indent=2)+'\n')
print(json.dumps({'status':result['status'],'correctness':correct,'performance':perf},indent=2))
