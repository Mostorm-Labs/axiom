import json,os,subprocess,sys
from pathlib import Path
root=Path.cwd();out=root/'.aegis/results/FC-08-SECTION-DE/baseline-raster-001';out.mkdir(exist_ok=False)
baseline=root/'out/fc08-section-de/baseline-source';build=root/'out/fc08-section-de/baseline-build'
env={**os.environ,**json.loads((root/'out/fc08-section-de/environment.json').read_text())['environment'],'PYTHONUTF8':'1'}
journal=[]
def run(name,args,cwd=root):
    r=subprocess.run([str(x) for x in args],cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
    (out/(name+'.stdout.log')).write_text(r.stdout,encoding='utf-8');(out/(name+'.stderr.log')).write_text(r.stderr,encoding='utf-8')
    journal.append({'name':name,'cwd':str(cwd),'argv':[str(x) for x in args],'exit_code':r.returncode});(out/'invocations.json').write_text(json.dumps(journal,indent=2)+'\n')
    print(name,r.returncode,flush=True);return r.returncode
assert not baseline.exists()
assert run('worktree',['git','worktree','add','--detach',baseline,'af1aff801369052eb7b94716f6b4ab3e899eb3e9'])==0
assert run('link-core',['powershell','-NoProfile','-Command',f'New-Item -ItemType Junction -Path "{baseline/".deps"}" -Target "{root/".deps"}"'])==0
sys.path.insert(0,str(root));from tools.semantic.cmake_consumer import consumer_cmake_arguments
skia='C:/Users/Chado/.codex/worktrees/fc08-verification-materialization/canvas/.deps/skia-sdk/windows-x64-d3d12/release'
args=['cmake','-S',baseline,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=ON','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_RF01_BUILD_TESTS=OFF','-DCANVAS_BUILD_RENDER=ON','-DCANVAS_RENDER_BUILD_TESTS=ON','-DCANVAS_BUILD_INK_PLAYGROUND=OFF','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DCANVAS_SKIA_SDK_VARIANT=release','-DCANVAS_SKIA_SDK_ROOT='+skia,'-DAXIOM_FC08_QUALIFICATION_HOOKS=OFF',*consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(env['AXIOM_PROTOC']))]
code=run('configure',args)
if not code:code=run('build',['cmake','--build',build,'--target','axiom_render_headless_skia_tests','--parallel','6'])
if not code:code=run('raster',['ctest','--test-dir',build,'-R','^FC01TriangleConnectorRaster$','--output-on-failure','--output-junit',out/'ctest.xml'])
run('identity',['git','rev-parse','HEAD'],baseline);run('preservation',['git','diff','--exit-code'],baseline)
raise SystemExit(code)
