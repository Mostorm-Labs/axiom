import gzip,hashlib,json,shutil,statistics,subprocess,re
from pathlib import Path
DEST=Path('verification/evidence/performance/g5-canonical-coverage')
REV='1a39d1a0de65d7f22270c771160ae96b246d062c'
BASE='32418c783170eea56a4131579fac627c003c91bb'
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(src,dst):
 src=Path(src);dst=DEST/dst;dst.parent.mkdir(parents=True,exist_ok=True)
 if src.suffix=='.log' or (src.suffix=='.json' and src.stat().st_size>131072):
  dst=dst.with_suffix(dst.suffix+'.gz');dst.write_bytes(gzip.compress(src.read_bytes(),mtime=0))
 else:shutil.copyfile(src,dst)
DEST.mkdir(parents=True,exist_ok=True)
comparisons=[];all_measured=True;all_pixels=True;all_state=True;all_provenance=True;closure={}
for mode in ['on','off']:
 root=Path('out/g5-canonical-matrix-'+mode);matrix=read(root/'matrix.json')
 for p in root.rglob('*'):
  if p.is_file():copy(p,Path(mode)/p.relative_to(root))
 copy(str(root)+'.log',mode+'/driver.log')
 rows=matrix['results'];all_measured &= len(rows)==8 and all(r['status']=='MEASURED' for r in rows)
 waits=[[],[]];coverage=[[],[]]
 for case in sorted({r['case'] for r in rows}):
  a=next(r for r in rows if r['case']==case and r['side']=='before');b=next(r for r in rows if r['case']==case and r['side']=='after')
  old=read(root/(case+'-before')/'summary.json');new=read(root/(case+'-after')/'summary.json')
  all_provenance &= all(s['sourceStable'] and s['artifactStable'] for s in [old,new])
  all_provenance &= old['results'][0]['setup']['build']['sourceRevision']==BASE and new['results'][0]['setup']['build']['sourceRevision']==REV
  all_provenance &= all(s['results'][0]['setupQuiescence']['stopped'] for s in [old,new])
  all_state &= all(all(s['results'][0]['checks'].values()) for s in [old,new])
  all_pixels &= a['setup']['initialPixels']['sha256']==b['setup']['initialPixels']['sha256']
  phases=[]
  for p,q in zip(a['phases'],b['phases']):
   same=p['pixels']['sha256']==q['pixels']['sha256'];all_pixels &= same
   s=p['summary'];t=q['summary']
   phases.append({'id':p['id'],'same_full_pixels':same,'before_gap_ms':s['input_display_submission_gap_ms']['max'],'after_gap_ms':t['input_display_submission_gap_ms']['max'],
    'before_wait_ms':s['settle_ms'],'after_wait_ms':t['settle_ms'],'before_exact_attempts':s['exactAttempts'],'after_exact_attempts':t['exactAttempts'],
    'before_cpu_coverage':s['exactStages'].get('cpu_coverage'),'after_cpu_coverage':t['exactStages'].get('cpu_coverage'),
    'after_work':t['exactCoverageWork'],'after_stage_breakdown':{k:v for k,v in t['exactStages'].items() if k in ['coverage_select','contour_compile','contour_record','contour_replay','contour_draw']},
    'before_quality':(s['navigation'] or {}).get('quality'),'after_quality':(t['navigation'] or {}).get('quality')})
   if 'zoom' in case and p['id'] in ['repeat-0','repeat-1']:
    for side,x in enumerate([s,t]):
     waits[side].append(x['settle_ms'])
     if mode=='on':coverage[side].append(x['exactStages']['cpu_coverage']['inclusive_ms'])
  comparisons.append({'mode':mode,'case':case,'objects':[a['setup']['state']['objects'],b['setup']['state']['objects']],'selection_kind':[a['setup']['selectedKind'],b['setup']['selectedKind']],'phases':phases})
 med=[statistics.median(x) for x in waits];closure[mode]={'before_warm_wait_median_ms':med[0],'after_warm_wait_median_ms':med[1],'wait_ratio':med[1]/med[0],'wait_reduction_30_percent':med[1]<=med[0]*.7}
 if mode=='on':
  med=[statistics.median(x) for x in coverage];closure[mode].update(before_warm_coverage_median_ms=med[0],after_warm_coverage_median_ms=med[1],coverage_ratio=med[1]/med[0],coverage_halved=med[1]<=med[0]*.5)
(DEST/'comparison.json').write_text(json.dumps(comparisons,indent=2)+'\n')
for name in ['prototype','checkpoint','checkpoint2','localdraw','scalar','alpha']:
 root=Path('out/g5-canonical-'+name)
 for p in root.rglob('*'):
  if p.is_file():copy(p,Path('exploratory')/name/p.relative_to(root))
 for p in Path('out').glob('g5-canonical-'+name+'-worker.*'):
  if p.is_file():copy(p,Path('exploratory')/name/p.name)
for p in Path('out').glob('g5-canonical-alpha-gpu-*'):
 if p.is_file():copy(p,Path('gpu')/p.name)
for name in ['red.log','soft-red.log','locality-green.log','native-build.log','native-tests.log','native-comparison.json','node.log','boundary.log','final-locality-tests.log',
 'checkpoint-metrics-red.log','checkpoint-metrics-green.log','final-on-configure.log','final-on-build.log','final-on-install.log','final-off-configure.log','final-off-build.log','final-off-install.log']:
 p=Path('out/g5-canonical-'+name)
 if p.exists():copy(p,Path('checks')/name)
for name in ['package-checks.json','demo-smoke.json','demo.png','prefilled.png','demo-smoke.mjs','demo-smoke-worker.worker.json']:
 copy('out/g5-canonical-'+name,Path('demo')/name)
native=read('out/g5-canonical-alpha-native-comparison.json');gpu=read('out/g5-canonical-alpha-gpu-results.json');demo=read('out/g5-canonical-alpha-demo-smoke.json');package=read('out/g5-canonical-alpha-package-checks.json')
correct=bool(all_measured and all_pixels and all_state and all_provenance and not native['new_failures'] and all(r['status']=='PASS' for r in gpu) and demo['passed'] and package['passed'])
performance=closure['on']['coverage_halved'] and closure['off']['wait_reduction_30_percent']
result={'status':'BLOCKED_PERFORMANCE' if correct else 'BLOCKED_EVIDENCE',
 'canonical_exact_result':'READY_FOR_CONTROL_REVIEW' if correct and performance else 'BLOCKED_PERFORMANCE',
 'navigation_continuity_closed':False,
 'navigation_finding':read('out/g5-canonical-restore-analysis.json'),
 'scope':'Canonical exact coverage locality, no formal G5/P34/platform qualification','implementation_revision':REV,'baseline_revision':BASE,'official_gate_pass_claimed':False,
 'correctness_checks':correct,'performance_target_met':performance,'measured':all_measured,'same_final_full_canvas_pixels':all_pixels,'state_and_receipt_checks':all_state,'provenance_checks':all_provenance,
 'performance':closure,'native':native,'gpu':gpu,'node_tests':int(re.search(r'# tests (\d+)',Path('out/g5-canonical-alpha-node.log').read_text())[1]),'boundary_contracts':118,
 'remaining_findings':['Canonical warm performance targets are met, but restored zoom-to-pan submission gaps worsened in multiple samples; reverse-order recheck confirms an open continuity guard. Overall performance acceptance remains blocked.',
 'Temporary compiler metadata uses hard-budget job scratch while optional pictures remain soft-admitted; configured limits unchanged.',
 'GPU image/staging lifecycle and high-DPR resident quality budget remain a separate design/verification slice.',
 'Public frame receipts are coarse completion proxies; see native frame stamps in ON raw traces, not physical input-to-photon.',
 'Five existing native failures remain; no new failures. Physical Windows/Android/Apple devices and formal100K/1M qualification not established.'],
 'rejected_candidates':'Exploratory traces include grouped-record-only, checkpoint, extra diagnostic and rejected per-primitive cull trials. They bind dirty source hashes but are not final artifact evidence.',
 'demo':{'url':'http://127.0.0.1:4221/example.html','prefilled':'http://127.0.0.1:4221/mixed-navigation.html?dense=1','artifact':'out/g5-canonical-alpha-final-off/js','preserved4219':True}}
(DEST/'result.json').write_text(json.dumps(result,indent=2)+'\n')
artifacts={mode:{str(p.relative_to(Path('out/g5-canonical-alpha-final-'+mode+'/js'))):sha(p) for p in sorted(Path('out/g5-canonical-alpha-final-'+mode+'/js').rglob('*')) if p.is_file()} for mode in ['on','off']}
(DEST/'consumer-artifacts.json').write_text(json.dumps(artifacts,indent=2)+'\n')
inputs=json.dumps(read('out/g5-canonical-matrix-on/dense-zoom-pan-after/summary.json')['sourceBefore'])
(DEST/'source-inputs.json').write_text(inputs)
for mode in ['on','off']:
 root=Path('out/g5-canonical-index-matrix-'+mode)
 for p in root.rglob('*'):
  if p.is_file():copy(p,Path('exploratory/index-matrix')/mode/p.relative_to(root))
for p in Path('out/g5-canonical-r8-source').rglob('*'):
 if p.is_file():copy(p,Path('exploratory/r8-source')/p.relative_to('out/g5-canonical-r8-source'))
for name in ['red.log','soft-red.log','locality-green.log','checkpoint-metrics-red.log','checkpoint-metrics-green.log','scalar-red.log','scalar-green.log','scalar-gpu.json','alpha-red.log','alpha-green.log','alpha-gpu.json','scalar-metrics-red.log','scalar-metrics-green.log','format-probe.cpp','format-probe.py','format-probe.log']:
 copy('out/g5-canonical-'+name,Path('checks/experiments')/name)
for name in ['native-build.log','native-tests.log','native-comparison.json','node.log','boundary.log','final-local-tests.log','final-on-configure.log','final-on-build.log','final-on-install.log','final-off-configure.log','final-off-build.log','final-off-install.log','final-gpu-build.log']:
 copy('out/g5-canonical-alpha-'+name,Path('checks/final')/name)
for name in ['package-checks.json','demo-smoke.json','demo.png','prefilled.png','demo-smoke.mjs','demo-smoke-worker.worker.json']:
 copy('out/g5-canonical-alpha-'+name,Path('demo/final')/name)
shutil.copyfile('out/g5-canonical-collect.py',DEST/'collect_evidence.py')
for side in ['after','before']:
 root=Path('out/g5-canonical-restore-recheck-'+side)
 for p in root.rglob('*'):
  if p.is_file():copy(p,Path('restore-recheck')/side/p.relative_to(root))
 for p in Path('out').glob('g5-canonical-restore-recheck-'+side+'-worker.*'):
  copy(p,Path('restore-recheck')/side/p.name)
copy('out/g5-canonical-restore-analysis.json','restore-analysis.json')
copy('out/g5-canonical-restore-analysis.py','restore-analysis.py')
copy('out/g5-dense-native-targets.json','checks/final/native-targets.json')
copy('out/g5-dense-native-test-names.txt','checks/final/native-test-names.txt')
copy('verification/evidence/performance/g5-dense-navigation/checks/native-comparison.json','checks/final/baseline-native-comparison.json')
print(json.dumps({'status':result['status'],'canonical_exact_result':result['canonical_exact_result'],'performance':closure,'correctness':correct}))

for name in ['node','boundary']:
 copy('out/g5-canonical-alpha-final-'+name+'.log','checks/final/final-'+name+'.log')
if Path('out/g5-canonical-README.md').exists():shutil.copyfile('out/g5-canonical-README.md',DEST/'README.md')
files={str(p.relative_to(DEST)):sha(p) for p in sorted(DEST.rglob('*')) if p.is_file() and p.name!='SHA256.json'}
(DEST/'SHA256.json').write_text(json.dumps(files,indent=2)+'\n')
