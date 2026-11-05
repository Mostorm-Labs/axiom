import gzip,hashlib,json,re,shutil,statistics
from pathlib import Path
DEST=Path('verification/evidence/performance/g5-gpu-lifetimes')
REV='10f3ecd86f60f3dfffb407644af3921817dfcc58'
BASE='61ede04e819edf7f8a280465d9be842ea1ade860'
PREFIX='g5-gpu-ready'
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def copy(src,dst):
 src=Path(src);dst=DEST/dst;dst.parent.mkdir(parents=True,exist_ok=True)
 if src.suffix=='.log' or (src.suffix=='.json' and src.stat().st_size>131072):
  dst=dst.with_suffix(dst.suffix+'.gz');dst.write_bytes(gzip.compress(src.read_bytes(),mtime=0))
 else:shutil.copyfile(src,dst)
def tree(src,dst):
 for p in Path(src).rglob('*'):
  if p.is_file():copy(p,Path(dst)/p.relative_to(src))
def navstats(root):
 rows=read_gzip(Path(root)/'wheel.console.json.gz')
 nav=[json.loads(x['text'].split('AXIOM_NAVIGATION_TIMING ',1)[1]) for x in rows if 'AXIOM_NAVIGATION_TIMING ' in x.get('text','')]
 if not nav:return None
 return {'observations':len(nav),'evictions':sum(x.get('tile_evictions',0) for x in nav),'builds':sum(x.get('built',0) for x in nav),
 'peak_shared_charged_bytes':max(x.get('budget_used_bytes',0) for x in nav),'peak_navigation_charged_bytes':max(x.get('navigation_tile_bytes',0) for x in nav),
 'last_navigation_charged_bytes':nav[-1].get('navigation_tile_bytes'),'peak_staging_bytes':max(x.get('navigation_staging_bytes',0) for x in nav),
 'last_staging_bytes':nav[-1].get('navigation_staging_bytes'),'peak_pending_release_bytes':max(x.get('navigation_pending_release_bytes',0) for x in nav),
 'last_pending_release_bytes':nav[-1].get('navigation_pending_release_bytes')}
def read_gzip(p):return json.loads(gzip.decompress(p.read_bytes()))
def checked(s,side):
 return s['status']=='MEASURED' and s['sourceStable'] and s['artifactStable'] and not s['sourceBefore']['dirty'] and s['sourceBefore']['commit']==REV and all(s['results'][0]['checks'].values()) and s['results'][0]['setupQuiescence']['stopped'] and s['results'][0]['setup']['build']['sourceRevision']==(REV if side=='after' else BASE) and not s['results'][0]['setup']['build']['dirty']
DEST.mkdir(parents=True,exist_ok=True)
correct=True;comparisons=[];perf={};residency=[];inputs={};full_source=None
for mode in ['off','on']:
 root=Path('out/'+PREFIX+'-matrix-'+mode);matrix=read(root/'matrix.json');tree(root,mode);copy(str(root)+'.log',mode+'/driver.log')
 rows=matrix['results'];correct &= len(rows)==8 and all(r['status']=='MEASURED' for r in rows)
 freeze=[[],[]];warm=[[],[]];idle=[[],[]];exact=[[],[]];cpu=[[],[]]
 for case in sorted({r['case'] for r in rows}):
  summaries=[read(root/(case+'-'+side)/'summary.json') for side in ['before','after']]
  for side,s in zip(['before','after'],summaries):
   correct &= checked(s,side);full_source=s['sourceBefore'];inputs[mode+'/'+case+'/'+side]={k:full_source[k] for k in ['commit','dirty','tree_sha256']}
  a,b=[s['results'][0] for s in summaries]
  correct &= a['setup']['initialPixels']['sha256']==b['setup']['initialPixels']['sha256']
  phases=[]
  for p,q in zip(a['phases'],b['phases']):
   s,t=p['summary'],q['summary'];same=p['pixels']['sha256']==q['pixels']['sha256'];correct &= same
   phases.append({'id':p['id'],'same_final_pixels':same,'before_gap_ms':s['input_display_submission_gap_ms']['max'],'after_gap_ms':t['input_display_submission_gap_ms']['max'],'before_wait_ms':s['settle_ms'],'after_wait_ms':t['settle_ms'],'before_quality':(s.get('navigation') or {}).get('quality'),'after_quality':(t.get('navigation') or {}).get('quality')})
   if 'zoom' in case:
    for i,x in enumerate([s,t]):
     if p['id']=='after-freeze':freeze[i].append(x['input_display_submission_gap_ms']['max'])
     if p['id']=='after-idle':idle[i].append(x['input_display_submission_gap_ms']['max'])
     if p['id'] in ['repeat-0','repeat-1']:
      warm[i].append(x['input_display_submission_gap_ms']['max']);exact[i].append(x['settle_ms'])
      if mode=='on':cpu[i].append(x['exactStages']['cpu_coverage']['inclusive_ms'])
  comparisons.append({'mode':mode,'case':case,'phases':phases})
  if mode=='on':
   pair={side:navstats(root/(case+'-'+side)) for side in ['before','after']};residency.append({'case':case,**pair})
   correct &= all(x and x['peak_shared_charged_bytes']<=96*1024*1024 and x['peak_navigation_charged_bytes']<=32*1024*1024 for x in pair.values())
 med=lambda x:{'before':statistics.median(x[0]),'after':statistics.median(x[1]),'before_samples':x[0],'after_samples':x[1]}
 perf[mode]={'restored_gap_ms':med(freeze),'idle_gap_ms':med(idle),'warm_gap_ms':med(warm),'warm_exact_wait_ms':med(exact),'warm_cpu_coverage_ms':med(cpu) if cpu[0] else None}
extras={};reverse=[]
for label in ['recheck','dpr3','dpr4']:
 pair={};r=[]
 for side in ['after','before']:
  root=Path('out/'+PREFIX+'-'+label+'-'+side);s=read(root/'summary.json');tree(root,Path(label)/side)
  for p in Path('out').glob(PREFIX+'-'+label+'-'+side+'-*'):
   if p.is_file():copy(p,Path(label)/side/p.name)
  correct &= checked(s,side);pair[side]=s['results'][0];
  if label.startswith('dpr'):
   dpr=int(label[3:]);pixels=s['results'][0]['setup']['initialPixels'];opts=s['results'][0]['options'];correct &= pixels['width']==opts['width']*dpr and pixels['height']==opts['height']*dpr
  r.append({'side':side,'phases':s['results'][0]['phases'],'surface_pixels':s['results'][0]['setup']['initialPixels'],'resources':navstats(root)})
 correct &= all(a['pixels']['sha256']==b['pixels']['sha256'] for a,b in zip(pair['before']['phases'],pair['after']['phases']))
 extras[label]=r
reverse=extras['recheck'];rgap={r['side']:next(p['summary']['input_display_submission_gap_ms']['max'] for p in r['phases'] if p['id']=='after-freeze') for r in reverse}
for p in Path('out').glob(PREFIX+'-*'):
 if p.is_file() and not any(p.name.startswith(PREFIX+'-'+label+'-') for label in ['recheck','dpr3','dpr4']):copy(p,Path('checks')/p.name)
for pattern in ['g5-gpu-*red*.log','g5-gpu-*green*.log','g5-gpu-dpr3-debug*','g5-gpu-dpr3-after*','g5-gpu-dpr3-green*','g5-gpu-dpr4-probe*','g5-gpu-debug-lifecycle.mjs','g5-gpu-corroborate.py','g5-gpu-matrix-*.log']:
 for p in Path('out').glob(pattern):
  if p.is_file():copy(p,Path('exploratory')/p.name)
  elif p.is_dir():tree(p,Path('exploratory')/p.name)
for mode in ['off','on']:tree(Path('out/g5-gpu-matrix-'+mode),Path('exploratory/76ae71fc')/mode)
for mode in ['off','on']:tree(Path('out/g5-gpu-closed-matrix-'+mode),Path('exploratory/3306d78a')/mode)
for p in Path('out').glob('g5-gpu-nav-readmask*'):
 if p.is_file():copy(p,Path('exploratory/readmask')/p.name)
 elif p.is_dir():tree(p,Path('exploratory/readmask')/p.name)
for label in ['recheck','dpr3','dpr4']:
 for side in ['before','after']:
  src=Path('out/g5-gpu-closed-'+label+'-'+side)
  if src.exists():tree(src,Path('exploratory/3306d78a')/(label+'-'+side))
for p in Path('out').glob('g5-gpu-closed-*'):
 if p.is_file():copy(p,Path('exploratory/3306d78a/checks')/p.name)
for pattern in ['g5-gpu-recheck-*','g5-gpu-dpr3-before*']:
 for p in Path('out').glob(pattern):
  if p.is_file():copy(p,Path('exploratory/76ae71fc/checks')/p.name)
  elif p.is_dir():tree(p,Path('exploratory/76ae71fc')/p.name)
copy('out/g5-gpu-performance-assessment.json','assessment.json')
for name in ['notes.md','primary-source-refs.json']:copy(Path('out/g5-gpu-lifecycle-preflight')/name,Path('references')/name)
for name in ['targets.json','test-names.txt']:
 p=Path('out/g5-dense-native-'+name);copy(p,Path('checks')/p.name)
native=read('out/'+PREFIX+'-native-comparison.json');gpu=read('out/'+PREFIX+'-gpu-results.json');package=read('out/'+PREFIX+'-package-checks.json');demo=read('out/'+PREFIX+'-demo-smoke.json');gpu_source=read('out/'+PREFIX+'-inputs.json')
node=Path('out/'+PREFIX+'-node.log').read_text();boundary=Path('out/'+PREFIX+'-boundaries.log').read_text()
correct &= not native['new_failures'] and len(gpu)==7 and all(x['status']=='PASS' for x in gpu) and package['passed'] and demo['passed'] and REV[:8] in demo['denseSetup'] and gpu_source['implementation_revision']==REV and bool(re.search(r'# fail 0\b',node)) and 'boundary check passed' in boundary
numeric=perf['off']['restored_gap_ms']['after']<=150 and rgap['after']<=150 and all(perf[m]['warm_exact_wait_ms']['after']<=300 for m in ['off','on']) and perf['on']['warm_cpu_coverage_ms']['after']<=80
useful=any(x['after']['evictions']<x['before']['evictions'] and x['after']['builds']<x['before']['builds'] for x in residency)
# Relative/navigation-quality assessment stays explicit; no official Gate claim.
assessment=read('out/g5-gpu-performance-assessment.json')
performance=numeric and useful and assessment['no_repeatable_navigation_regression']
result={'status':'READY_FOR_CONTROL_REVIEW' if correct and performance else 'BLOCKED_PERFORMANCE' if correct else 'BLOCKED_EVIDENCE',
 'scope':'GPU derived navigation residency/staging lifetime slice; not formal G5/P34/platform PASS','official_gate_pass_claimed':False,'implementation_revision':REV,'baseline_revision':BASE,'coverage_reuse_evidence_revision':'3e2903475310a3a333d4a25c03f973c91473f61b',
 'task_package':{'path':'docs/planning/G5_GPU_RESIDENT_STAGING_LIFETIMES.md','sha256':sha('docs/planning/G5_GPU_RESIDENT_STAGING_LIFETIMES.md')},
 'correctness_checks':correct,'numeric_performance_guards_met':numeric,'useful_residency_improved':useful,'performance_assessment':assessment,'performance':perf,'residency':residency,'corroboration':extras,
 'native':native,'gpu_modes':[{'mode':x['mode'],'status':x['status']} for x in gpu],'node_tests':int(re.search(r'# tests (\d+)',node)[1]),'boundary_contracts':int(re.search(r'\((\d+) contract',boundary)[1]),'package':package,
 'snapshot_denial_regression':{'red_revision':'76ae71fc9a801686102c8b9f70796f86b6dc34d5','fix_revision':'3306d78aed70ef6b0017e9edd5ed81658b2b194c','verified_revision':REV,'classification':'IMPLEMENTATION_DEFECT: optional retained navigation snapshot denial escaped as std::bad_alloc; canonical generation15 applied but presented generation14','fixed_in_root_DPR3':all(x['side']!='after' or all(p['checks'].values()) for x in extras['dpr3'] for p in x['phases'])},
 'quality_finding_open':True,'cold_exact_tail_open':True,
 'remaining_work':['Continuous-input fine-detail/clarity and cold exact tails remain measured findings.','Complex editing and100K/1M formal qualification remain pending.','Five pre-existing native failures and physical platform qualification remain outside this slice.'],
 'demo':{'url':'http://127.0.0.1:4226/example.html','prefilled':'http://127.0.0.1:4226/mixed-navigation.html?dense=1','artifact':'out/g5-gpu-ready-off/js','previous_installs_preserved':True},
 'measurement_limits':'Synthetic Chrome WebGL2 color submission timing,not physical input-to-photon. CDP freeze is not OS/device sleep. Density is a source proxy with differing presented-pose sets,not matched-pose optical proof. Quota accounts admitted derived resources,not all Skia/driver allocations. Ambiguous context abandonment conservatively retains charge until budget owner closes.'}
(DEST/'result.json').write_text(json.dumps(result,indent=2)+'\n');(DEST/'comparison.json').write_text(json.dumps(comparisons,indent=2)+'\n')
(DEST/'source-inputs.json').write_text(json.dumps({'samples':inputs,'files':full_source['files']},indent=2)+'\n')
artifacts={m:{str(p.relative_to(Path('out/'+PREFIX+'-'+m+'/js'))):sha(p) for p in sorted(Path('out/'+PREFIX+'-'+m+'/js').rglob('*')) if p.is_file()} for m in ['on','off']}
(DEST/'consumer-artifacts.json').write_text(json.dumps(artifacts,indent=2)+'\n')
shutil.copyfile(__file__,DEST/'collect_evidence.py')
shutil.copyfile('out/g5-gpu-verify-evidence.py',DEST/'verify_evidence.py')
if Path('out/g5-gpu-README.md').exists():shutil.copyfile('out/g5-gpu-README.md',DEST/'README.md')
files={str(p.relative_to(DEST)):sha(p) for p in sorted(DEST.rglob('*')) if p.is_file() and p.name!='SHA256.json'}
(DEST/'SHA256.json').write_text(json.dumps(files,indent=2)+'\n')
print(json.dumps({'status':result['status'],'correctness':correct,'numeric':numeric,'useful':useful,'performance':perf},indent=2))
