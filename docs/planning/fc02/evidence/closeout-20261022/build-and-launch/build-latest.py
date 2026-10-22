import json, os, pathlib, subprocess, sys, time
root=pathlib.Path.cwd()
kind=sys.argv[1]
out=root/'out/fc02-resume/launch-3d7afa35'
facts=json.loads((out/f'environment-{kind}.json').read_text())['environment']
bat=pathlib.Path('C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat') if kind=='native' else root/'.deps/emsdk/emsdk_env.bat'
setup=f'call "{bat}" '+('-arch=x64 -host_arch=x64 ' if kind=='native' else '')+'>nul && set'
raw=subprocess.check_output(setup,shell=True,text=True,errors='replace')
env={k.upper():v for k,v in os.environ.items()}
for line in raw.splitlines():
 if '=' in line and not line.startswith('='):
  k,v=line.split('=',1);env[k.upper()]=v
env.update({k.upper():v for k,v in facts.items()})
sys.path.insert(0,str(root))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
args=consumer_cmake_arguments(pathlib.Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']),pathlib.Path(facts['AXIOM_PROTOC']))
if kind=='native':
 build=root/'out/fc02-resume/build'
 cmd=json.loads((root/'out/fc02-resume/configure-command.json').read_text())['command']
 excluded=('CMAKE_PREFIX_PATH','AXIOM_PROTOC','CMAKE_FIND_ROOT_PATH','CMAKE_FIND_ROOT_PATH_MODE_PACKAGE','CMAKE_FIND_USE_PACKAGE_REGISTRY','CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY')
 cmd=[v for v in cmd if not any(v.startswith('-D'+k+'=') for k in excluded)]+args
 target='axiom_minimal_runtime_host_windows'
else:
 build=root/'out/fc02-resume/web-build-repair'
 emsdk=root/'.deps/emsdk'
 cmd=[str(emsdk/'python/3.13.3_64bit/python.exe'),str(emsdk/'upstream/emscripten/emcmake.py'),'cmake','-S',str(root),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RENDER=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DAXIOM_BUILD_DEBUG_UI=ON','-DAXIOM_DEBUG_UI_PROFILE=full','-DCANVAS_SKIA_SDK_VARIANT=release','-DCANVAS_SKIA_SDK_ROOT=C:/Users/Chado/.codex/worktrees/fc00x-runtime-minimum-closure/canvas/.deps/skia-sdk/web-wasm-webgl2/release',*args]
 target='axiom_canvas_demo_web'
commands=[cmd,['cmake','--build',str(build),'--parallel','6','--target',target]]
record={'kind':kind,'revision':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'commands':commands,'results':[]}
for i,command in enumerate(commands):
 log=out/f'{kind}-'+pathlib.Path('') if False else out/f'{kind}-{["configure","build"][i]}.log'
 with log.open('w',encoding='utf-8') as f:r=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT)
 record['results'].append({'exit_code':r.returncode,'log':str(log)})
 (out/f'{kind}-commands.json').write_text(json.dumps(record,indent=2))
 print(f'{kind}: step {i+1} exit {r.returncode} log {log}',flush=True)
 if r.returncode:
  print(log.read_text(errors='replace')[-6000:]);sys.exit(r.returncode)
