from pathlib import Path
import json,hashlib,subprocess,tarfile,io,shutil
R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');P=R/'.aegis/packages/G5-post-navigation-settle-r1';O=R/'out/G5-post-navigation-settle-r1';E=R/'.aegis/results/G5-post-navigation-settle-r1';h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();bindings=[]
assert not O.exists();O.mkdir();E.mkdir(exist_ok=True)
assert h(P/'package.json')=='841c0b9582cd3cf8966dbb10b13aaac674be0247e985ae08216a1895d7f00d93'
def check(v,base):
 if isinstance(v,dict):
  if {'path','bytes','sha256'}<=v.keys():
   p=Path(v['path']);p=p if p.is_absolute()else base/p
   if not p.exists():p=R/v['path']
   assert p.stat().st_size==v['bytes']and h(p)==v['sha256'],str(p);bindings.append({'path':str(p),'bytes':p.stat().st_size,'sha256':h(p)})
  for x in v.values():check(x,base)
 elif isinstance(v,list):
  for x in v:check(x,base)
package=json.loads((P/'package.json').read_text());check(package,P)
for n in ['authority.lock.json','fixed-inputs.json']:check(json.loads((P/n).read_text()),R)
f=json.loads((P/'fixed-inputs.json').read_text());anchor=f['checkpoint']['revision'];assert subprocess.check_output(['git','rev-parse',package['execution_ref']],cwd=R,text=True).strip()==anchor
assert subprocess.check_output(['git','rev-parse',f['checkpoint']['tag']+'^{commit}'],cwd=R,text=True).strip()==anchor
assert subprocess.check_output(['git','rev-parse',f['checkpoint']['tag']],cwd=R,text=True).strip()==f['checkpoint']['tagObject']
source=json.loads(Path(f['baseline_source']['path']).read_text());primary=json.loads((R/f['primary_preservation']['path']).read_text());assert all(h(R/n)==v for n,v in primary.items())
for item in f['SDK_preservation']+[f['baseline_SDK']]:
 p=Path(item['path']);p=p if p.is_absolute()else R/p;m=json.loads(p.read_text());assert all(h(Path(m['root'])/n)==v for n,v in m['files'].items())
archive=subprocess.check_output(['git','archive',anchor],cwd=R)
for role in ['B','C']:
 S=O/('source-'+role);S.mkdir()
 with tarfile.open(fileobj=io.BytesIO(archive))as t:t.extractall(S,filter='data')
 assert all(h(S/n)==v for n,v in source.items()),role
 (S/'.deps').symlink_to(R/'.deps')
 # Native test-only outputs belong to newtask. No businessmutation yet.
 for n in ['runtime/composition/tests/web_navigation_handoff_test.cpp','runtime/composition/tests/exposure_region_reuse_cache_test.cpp','runtime/render/tests/canonical_prepared_body_test.cpp','runtime/render/tests/navigation_display_workavoidance_test.cpp','runtime/composition/tests/navigation_progressive_proof_test.cpp']:
  p=S/n
  if p.exists():p.write_text(p.read_text().replace('out/G5-exposure-region-reuse-r1','out/G5-post-navigation-settle-r1'))
(O/'correctness').mkdir();(O/'baseline-source.json').write_text(json.dumps(source,indent=2)+'\n')
(E/'preflight.json').write_text(json.dumps({'status':'CLEAR','bindings':bindings,'baselineSourceFiles':len(source),'primaryFiles':len(primary),'ambientHEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),'ambientBranch':subprocess.check_output(['git','branch','--show-current'],cwd=R,text=True).strip(),'executionRef':package['execution_ref'],'anchor':anchor,'readOnlyCheckpoint':f['checkpoint']['worktree'],'noSemanticAuthorityConflict':True},indent=2)+'\n')
shutil.copy2(R/f['optical_method']['path'],E/'optical-oracle.json');print('CLEAR',len(bindings),'source',len(source))
