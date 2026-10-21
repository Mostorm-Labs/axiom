import json, os, pathlib, subprocess, sys, time
root=pathlib.Path(__file__).resolve().parents[2]
out=root/'out/fc02-resume'
sys.path.insert(0,str(root))
facts=json.loads((out/'web-environment.json').read_text())['environment']
emsdk=root/'.deps/emsdk'
cmd=f'call "{emsdk / "emsdk_env.bat"}" >nul && set'
raw=subprocess.check_output(cmd,shell=True,text=True)
env={k.upper():v for k,v in os.environ.items()}
for line in raw.splitlines():
 if '=' in line and not line.startswith('='):
  k,v=line.split('=',1);env[k.upper()]=v
env.update({k.upper():v for k,v in facts.items()})
# Keep the locked emsdk as the compiler and the repo's explicit Semantic helper paths.
webroot=out/'web-build'
skia=pathlib.Path('C:/Users/Chado/.codex/worktrees/fc00x-runtime-minimum-closure/canvas/.deps/skia-sdk/web-wasm-webgl2/release')
from tools.semantic.cmake_consumer import consumer_cmake_arguments
command=[sys.executable,str(emsdk/'upstream/emscripten/emcmake.py'),'cmake','-S',str(root),'-B',str(webroot),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RENDER=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DAXIOM_BUILD_DEBUG_UI=ON','-DAXIOM_DEBUG_UI_PROFILE=full','-DCANVAS_SKIA_SDK_VARIANT=release',f'-DCANVAS_SKIA_SDK_ROOT={skia.as_posix()}',*consumer_cmake_arguments(pathlib.Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']),pathlib.Path(facts['AXIOM_PROTOC']))]
if len(sys.argv)>1 and sys.argv[1]=='build':
 command=['cmake','--build',str(webroot),'--parallel','4','--target','axiom_canvas_demo_web']
log=out/('web-build.log' if len(sys.argv)>1 and sys.argv[1]=='build' else 'web-configure.log'); log.write_text(json.dumps({'command':command,'env':facts,'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())},indent=2))
with log.open('a') as f:r=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT)
print(log.read_text(errors='replace')[-12000:]);raise SystemExit(r.returncode)
