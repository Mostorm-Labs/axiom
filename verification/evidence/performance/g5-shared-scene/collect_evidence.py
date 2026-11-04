import gzip,hashlib,json,re,shutil,subprocess
from pathlib import Path
from statistics import median
root=Path.cwd();import sys;sys.path.insert(0,str(root));out=root/'out';dest=root/'verification/evidence/performance/g5-shared-scene'
def read(p):return json.loads(p.read_text())
def write(name,value):(dest/name).write_text(json.dumps(value,indent=2)+'\n')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
dest.mkdir(parents=True,exist_ok=True)
paired={};runs=[]
for mode in ['on','off','optical']:
 directory=out/('g5-shared-final-matrix-'+mode)
 shutil.copytree(directory,dest/('matrix-'+mode),dirs_exist_ok=True)
 comparison=read(directory/'comparison.json');cases=[]
 for c in comparison['comparisons']:
  row={k:v for k,v in c.items() if k!='phases'};row['phases']=[]
  for p in c.get('phases',[]):
   q={'id':p['id'],'pixelsEqual':p['pixelsEqual']}
   for side in ['before','after']:
    s=p[side];n=s['navigation']
    q[side]={'gapMaxMs':s['input_display_submission_gap_ms']['max'],'gapP95Ms':s['input_display_submission_gap_ms']['p95'],
      'settleMs':s['settle_ms'],'exactCpuMs':s['exactStages'].get('attempt',{}).get('inclusive_ms'),
      'groupCpuMs':s['exactStages'].get('group_update',{}).get('inclusive_ms'),
      'quality':n.get('quality') if n else None,'sources':n.get('sharedSources') if n else None,'prepared':n.get('prepared') if n else None}
   row['phases'].append(q)
  cases.append(row)
 for path in sorted(directory.glob('*/summary.json')):
  s=read(path);runs.append({'path':str(path.relative_to(out)),'status':s['status'],'sourceStable':s['sourceStable'],
   'artifactStable':s['artifactStable'],'sdkRevision':s['results'][0]['setup']['build']['sourceRevision']})
 paired[mode]=cases
write('paired-results.json',paired)
supp={}
for name in ['bounded-optical','edit-publication']:
 sides={}
 for side in ['before','after']:
  directory=out/('g5-shared-'+name+'-'+side);shutil.copytree(directory,dest/(name+'-'+side),dirs_exist_ok=True)
  s=read(directory/'summary.json');sides[side]=s
  runs.append({'path':str(directory.relative_to(out)),'status':s['status'],'sourceStable':s['sourceStable'],
    'artifactStable':s['artifactStable'],'sdkRevision':s['results'][0]['setup']['build']['sourceRevision']})
 a,b=[sides[k]['results'][0] for k in ['before','after']]
 supp[name]={'initialPixelsEqual':a['setup']['initialPixels']['sha256']==b['setup']['initialPixels']['sha256'],
  'phasePixelsEqual':[{'id':x['id'],'equal':x['pixels']['sha256']==y['pixels']['sha256']} for x,y in zip(a['phases'],b['phases'])]}
 if name=='edit-publication':
  supp[name]['edits']=[{'index':x['index'],'pixelsEqual':x['pixels']['sha256']==y['pixels']['sha256'],
    'before':{k:x[k] for k in ['dispatchMs','completeMs','checks']},'after':{k:y[k] for k in ['dispatchMs','completeMs','checks']}}
    for x,y in zip(a['edits'],b['edits'])]
  supp[name]['sceneRows']=b['sceneRows']
write('supplemental-comparisons.json',supp);write('browser-status.json',runs)
assert all(r['status']=='MEASURED' and r['sourceStable'] and r['artifactStable'] for r in runs)
assert all(c.get('initialPixelsEqual') and all(p['pixelsEqual'] for p in c['phases']) for cs in paired.values() for c in cs)
assert all(v['initialPixelsEqual'] and all(p['equal'] for p in v['phasePixelsEqual']) for v in supp.values())
assert all(x['pixelsEqual'] and all(x['before']['checks'].values()) and all(x['after']['checks'].values()) for x in supp['edit-publication']['edits'])
for name in ['reclaimed-inputs','reclaimed-regression-comparison']:
 shutil.copy2(out/('g5-shared-'+name+'.json'),dest/({'reclaimed-inputs':'source-inputs','reclaimed-regression-comparison':'regression-comparison'}[name]+'.json'))
for p in out.glob('g5-shared-*.log'):
 if any(k in p.name for k in ['reclaimed','final-red','final-green','payload-red','snapshot-red','index-red','retention-red','version-red','metrics-red','native-final','focused-final']):
  (dest/(p.name+'.gz')).write_bytes(gzip.compress(p.read_bytes(),mtime=0))
for p in out.glob('g5-shared-reclaimed-gpu-*.json'):shutil.copy2(p,dest/p.name)
shutil.copy2(out/'g5-shared-gpu-full.json',dest/'immutable-gpu-fixture-red.json')
for directory in ['g5-shared-matrix-on','g5-shared-matrix-off','g5-shared-admission-recheck']:
 shutil.copytree(out/directory,dest/'experiments'/directory,dirs_exist_ok=True)
# Hash every installed resource; normal consumers contain neither Runtime headers
# nor stand-alone private/dependency archives. Keep diagnostic OFF actually OFF.
packages={}
for side,prefix in [('before','g5-shared-before'),('after','g5-shared-reclaimed')]:
 for mode in ['on','off']:
  directory=out/(prefix+'-'+mode)/'js';files={str(p.relative_to(directory)):digest(p) for p in sorted(directory.rglob('*')) if p.is_file()}
  wasm=(directory/'axiom.wasm').read_bytes();private=[n for n in files if n.endswith(('.h','.hpp','.a','.lib'))]
  assert not private
  nav=b'AXIOM_NAVIGATION_TIMING ' in wasm;scene=b'AXIOM_SCENE_TIMING ' in wasm
  assert nav==(mode=='on') and scene==(side=='after' and mode=='on')
  packages[side+'-'+mode]={'directory':str(directory),'files':files,'privateHeadersOrArchives':private,
    'diagnosticNavigation':nav,'diagnosticScene':scene,'developerComponents':False}
write('consumer-install.json',packages)
from verification.tools.g5_preflight import source_identity,ROOT
s=source_identity(ROOT);assert s['tree_sha256']==read(dest/'source-inputs.json')['tree_sha256']
write('test-inputs.json',{'source':s,'native':{str(p.relative_to(root)):digest(p) for p in [out/'host/runtime/render/axiom_shared_scene_tests',out/'host/runtime/render/axiom_render_cache_tests',out/'host/runtime/render/axiom_prepared_geometry_tests',out/'host/runtime/scene/tests/canvas_g2_a6_atomic_publication']},
 'probe':{p.name:digest(p) for p in (out/'web-prepared-probe').glob('g5_render_cache_gpu.*') if p.suffix in ['.js','.wasm']}})
assert not read(dest/'regression-comparison.json')['new_failure_names']
print('Collected',len(runs),'MEASURED paired runs; all final paired pixels equal')
