from pathlib import Path
import json,hashlib,subprocess,tarfile,io,shutil
R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');P=R/'.aegis/packages/G5-exposure-region-reuse-r1';O=R/'out/G5-exposure-region-reuse-r1';E=R/'.aegis/results/G5-exposure-region-reuse-r1';h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert not O.exists();O.mkdir();E.mkdir(exist_ok=True);assert h(P/'package.json')=='dd0450ef9bc7880feb61c6c30336ea920ea6fde5ee6379b6d44d204cc757d6c8';bindings=[]
def check(v,base):
 if isinstance(v,dict):
  if{'path','bytes','sha256'}<=v.keys():
   p=Path(v['path']);p=p if p.is_absolute()else base/p
   if not p.exists():p=R/v['path']
   assert p.stat().st_size==v['bytes']and h(p)==v['sha256'],str(p);bindings.append({'path':str(p),'bytes':p.stat().st_size,'sha256':h(p)})
  for x in v.values():check(x,base)
 elif isinstance(v,list):
  for x in v:check(x,base)
check(json.loads((P/'package.json').read_text()),P)
for n in['authority.lock.json','fixed-inputs.json']:check(json.loads((P/n).read_text()),R)
f=json.loads((P/'fixed-inputs.json').read_text());base=Path(f['baseline_4339']['root']);m=json.loads((R/f['baseline_4339']['source_manifest']['path']).read_text());assert len(m)==1079 and all(h(base/n)==v for n,v in m.items());primary=json.loads((R/f['primary_preservation']['path']).read_text());assert all(h(R/n)==v for n,v in primary.items())
for q in f['SDK_preservation']:
 sdk=json.loads((R/q['path']).read_text());assert all(h(Path(sdk['root'])/n)==v for n,v in sdk['files'].items())
archive=subprocess.check_output(['git','archive','HEAD'],cwd=R)
for role in['B','C']:
 S=O/('source-'+role);S.mkdir()
 with tarfile.open(fileobj=io.BytesIO(archive))as t:t.extractall(S)
 for n in m:
  p=S/n;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(base/n,p)
 (S/'.deps').symlink_to(R/'.deps');assert all(h(S/n)==v for n,v in m.items())
 # new outputs/adapt original test only, native qualification not consumerAPI
 for n in['runtime/composition/tests/navigation_progressive_proof_test.cpp','runtime/render/tests/navigation_display_workavoidance_test.cpp']:
  p=S/n;p.write_text(p.read_text().replace('out/G5-quality-candidate-continuation-r1/correctness','out/G5-exposure-region-reuse-r1/correctness'))
 test=(R/f['prior_full_runtime_RED_source']['path']).read_text().replace('out/G5-web-navigation-handoff-r1','out/G5-exposure-region-reuse-r1');(S/'runtime/composition/tests/web_navigation_handoff_test.cpp').write_text(test)
 p=S/'runtime/composition/CMakeLists.txt';s=p.read_text();s+='''\nif(BUILD_TESTING AND CANVAS_RENDER_ENABLE_SKIA_HEADLESS AND APPLE)
 add_executable(axiom_exposure_region_reuse_tests tests/web_navigation_handoff_test.cpp)
 target_compile_definitions(axiom_exposure_region_reuse_tests PRIVATE AXIOM_RUNTIME_QUALIFICATION=1)
 target_link_libraries(axiom_exposure_region_reuse_tests PRIVATE canvas_runtime_composition CanvasSkia::Skia "-framework Foundation" objc)
 target_include_directories(axiom_exposure_region_reuse_tests PRIVATE "${PROJECT_SOURCE_DIR}/.deps/nlohmann-json/single_include")
endif()\n''';p.write_text(s)
(O/'baseline-source.json').write_text(json.dumps(m,indent=2)+'\n');(O/'correctness').mkdir();shutil.copy2(R/f['optical_method']['path'],E/'optical-oracle.json');(E/'preflight.json').write_text(json.dumps({'status':'CLEAR','bindings':bindings,'baseline_count':1079,'primary_count':1083,'HEAD':subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),'branch':'codex/g5-cpu-worker','alloldSDKs_preserved':True},indent=2)+'\n');print('CLEAR',len(bindings))
# Build scripts newtask only
s=(R/'out/G5-web-navigation-handoff-r1/build-native.py').read_text().replace('G5-web-navigation-handoff-r1','G5-exposure-region-reuse-r1').replace("'axiom_progressive_clarity_tests'","'axiom_exposure_region_reuse_tests'");(O/'build-native.py').write_text(s)
