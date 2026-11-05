import hashlib,json,gzip,re,shutil,subprocess
from pathlib import Path
D=Path('verification/evidence/performance/g5-canonical-budget')
P='g5-budget-closed';REV=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();BASE='10f3ecd86f60f3dfffb407644af3921817dfcc58'
def read(p):return json.loads(Path(p).read_text())
def write(p,v):(D/p).parent.mkdir(parents=True,exist_ok=True);(D/p).write_text(json.dumps(v,indent=2)+'\n')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def copy(src,dst):
 src=Path(src);dst=D/dst;dst.parent.mkdir(parents=True,exist_ok=True)
 if src.suffix=='.log' or src.name.endswith('CMakeCache.txt') or (src.suffix=='.json' and src.stat().st_size>131072):dst=dst.with_suffix(dst.suffix+'.gz');dst.write_bytes(gzip.compress(src.read_bytes(),mtime=0))
 else:shutil.copyfile(src,dst)
def tree(src,dst):
 for p in Path(src).rglob('*'):
  if p.is_file():copy(p,Path(dst)/p.relative_to(src))
D.mkdir(parents=True,exist_ok=True);matrix=read('out/'+P+'-matrix.json');assert len(matrix)==9
correct=True;comparisons=[];sources={};artifacts={}
for row in matrix:
 label='dpr'+str(row['dpr'])+'/'+row['profile'];root=Path(row['output']);s=read(root/'summary.json');r=s['results'][0]
 tree(root,label)
 for p in Path('out').glob(root.name+'-*'):
  if p.is_file():copy(p,Path(label)/p.name)
 expected=BASE if row['profile']=='baseline' else REV
 checked=(s['status']=='MEASURED' and s['sourceStable'] and s['artifactStable'] and s['sourceBefore']['commit']==REV and not s['sourceBefore']['dirty'] and r['setup']['build']['sourceRevision']==expected and not r['setup']['build']['dirty'] and all(r['checks'].values()) and r['setupQuiescence']['stopped'])
 dims=r['setup']['initialPixels'];checked &= dims['width']==r['options']['width']*row['dpr'] and dims['height']==r['options']['height']*row['dpr']
 correct &= checked;sources[label]={k:s['sourceBefore'][k] for k in ['commit','dirty','tree_sha256']};artifacts[label]={'build':r['setup']['build'],'files':s['artifacts'],'cache_mib':row['cacheMiB'],'shared_hard_mib':row['sharedHardMiB']}
 row['verified_bindings']=checked
for dpr in [2,3,4]:
 rows=[r for r in matrix if r['dpr']==dpr];base=next(r for r in rows if r['profile']=='baseline');values=[]
 for a in base['phases']:
  ps=[(r,next(p for p in r['phases'] if p['id']==a['id'])) for r in rows]
  same=all(p['pixels']['sha256']==a['pixels']['sha256'] for r,p in ps);correct &= same
  values.append({'phase':a['id'],'same_final_pixels':same,'profiles':[{'profile':r['profile'],'wait_ms':p['summary']['settle_ms'],'max_exact_attempt_ms':p['maxExactAttemptMs'],'navigation_gap_ms':p['summary']['input_display_submission_gap_ms']['max'],'display_gap_ms':p['summary']['display_submission_gap_ms']['max'],'low_density_fraction':((p['summary'].get('navigation') or {}).get('quality') or {}).get('timeWeightedLowResolutionFraction'),'maximum_upscale':((p['summary'].get('navigation') or {}).get('quality') or {}).get('maximumUpscale'),'observed_shared_charge_bytes':p['maxSharedChargeBytes'],'navigation_charge_bytes':p['maxNavChargeBytes']} for r,p in ps]})
 comparisons.append({'dpr':dpr,'phases':values})
write('comparison.json',comparisons);write('matrix.json',matrix);write('consumer-artifacts.json',artifacts);write('source-inputs.json',{'result_revision':REV,'baseline_revision':BASE,'bindings':sources,'source':read('out/'+P+'-inputs.json')})
for p in Path('out').glob(P+'-*'):
 if p.is_file() and not p.name.startswith(P+'-dpr'):copy(p,Path('checks')/p.name)
for name in ['g5-dense-native-targets.json','g5-dense-native-test-names.txt','g5-budget-red.log','g5-budget-coverage-red.log','g5-budget-coverage-green.log','g5-budget-composite-red.log','g5-budget-closed-profile-inputs.json']:
 copy(Path('out')/name,Path('checks')/name)
for folder in ['g5-budget-exploratory-6407','g5-budget-exploratory-build-race']:tree(Path('out')/folder,Path('exploratory')/folder)
for profile in ['on','off','expanded-on']:
 for p in Path('out').glob('g5-budget-'+profile+'-*'):
  if p.is_file():copy(p,Path('exploratory/6407-builds')/p.name)
native=read('out/'+P+'-native-comparison.json');gpu=read('out/'+P+'-gpu-results.json');packages=read('out/'+P+'-package-checks.json');demo=read('out/'+P+'-demo-smoke.json')
correct &= not native['new_failures'] and len(gpu)==8 and all(g['status']=='PASS' for g in gpu) and packages['passed'] and demo['passed'] and '# fail 0' in Path('out/'+P+'-node.log').read_text() and 'boundary check passed' in Path('out/'+P+'-boundaries.log').read_text()
off=read('out/'+P+'-dpr4-off/summary.json');orow=off['results'][0]
correct &= off['status']=='MEASURED' and off['sourceStable'] and off['artifactStable'] and orow['setup']['build']['sourceRevision']==REV and not orow['setup']['build']['dirty'] and all(orow['checks'].values())
ref=read('out/'+P+'-dpr4-default/summary.json')['results'][0]
correct &= all(a['pixels']['sha256']==b['pixels']['sha256'] for a,b in zip(orow['phases'],ref['phases']))
tree('out/'+P+'-dpr4-off','dpr4/off')
for p in Path('out').glob(P+'-dpr4-off-*'):
 if p.is_file():copy(p,Path('dpr4/off')/p.name)
write('off-corroboration.json',{'status':off['status'],'build':orow['setup']['build'],'checks':orow['checks'],'phases':orow['phases'],'scope':'Timing diagnostics OFF; same DPR4 corpus and exact final pixels. No per-stage ON metrics inferred.'})
write('result.json',{'status':'READY_FOR_CONTROL_REVIEW' if correct else 'BLOCKED_EVIDENCE','result_revision':REV,'baseline_revision':BASE,'official_gate_pass_claimed':False,'default_budget_mib':[32,96],'experimental_budget_mib':[64,192],'qualifying_browser_samples':len(matrix)+1,'gpu_modes':[r['mode'] for r in gpu],'native':native,'all_required_checks_satisfied':correct,'scope':'Local shared Render engineering proof,not physical input-to-photon,all-platform verification or formal100K/1M qualification.'})
copy(Path(__file__),'reproduce/collect.py')
assert correct,'evidence checks failed'
