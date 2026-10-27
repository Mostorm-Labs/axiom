from pathlib import Path
import json,os,sys,subprocess
root=Path.cwd();sys.path.insert(0,str(root))
facts=json.loads((root/'out/selection-batch2/web-environment.json').read_text())['environment']
args=json.loads((root/'out/selection-batch2/configure-native.json').read_text())
args[args.index('out/selection-batch1/native')]='out/selection-batch1/web'
args=[a for a in args if not a.startswith(('-DCANVAS_SKIA_SDK_ROOT=','-DAXIOM_PROTOC=','-DCMAKE_PREFIX_PATH='))]
from tools.semantic.cmake_consumer import consumer_cmake_arguments
sys.path.insert(0,str(root/'tools/skia'))
from fetch import verify_installed
from consumer import load_lock
skia=Path('C:/Users/Chado/.codex/worktrees/fc08-verification-materialization/canvas/.deps/skia-sdk/web-wasm-webgl2/release')
manifest=verify_installed(skia,load_lock(root/'r1-full-skia-sdk.lock.json'),'web-wasm-webgl2',root/'tools/skia/profiles/r1-full-v1.json','release')
emsdk=Path('C:/Users/Chado/AppData/Local/Axiom/toolchains/emsdk-6.0.6')
args=[a for a in args if not a.startswith('-DCANVAS_BUILD_INK_PLAYGROUND=')]+['-DCANVAS_BUILD_INK_PLAYGROUND=OFF']
args+=['-DCANVAS_RF01_BUILD_TESTS=OFF','-DCANVAS_RENDER_BUILD_TESTS=OFF',f'-DCANVAS_SKIA_SDK_ROOT={skia.as_posix()}',f'-DCMAKE_TOOLCHAIN_FILE={(emsdk/"upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake").as_posix()}',*consumer_cmake_arguments(Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(facts['AXIOM_PROTOC']))]
env={**os.environ,**facts,'EMSDK':str(emsdk),'EM_CONFIG':str(emsdk/'.emscripten'),'EMSDK_PYTHON':sys.executable,'EMSDK_NODE':str(emsdk/'node/24.19.0_64bit/node.exe'),'PYTHONUTF8':'1'}
env['PATH']=str(emsdk/'upstream/emscripten')+';D:/Softwares/Git/cmd;'+env['PATH']
(root/'out/selection-batch2/configure-web.json').write_text(json.dumps(args,indent=2))
commands=[]
for name,command in [('release-web-configure',args),('release-web-build',['cmake','--build','out/selection-batch1/web','--target','axiom_canvas_demo_web','--parallel','6'])]:
 with (root/f'out/selection-batch2/{name}.log').open('w',encoding='utf-8') as log:r=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
 commands.append({'name':name,'argv':list(map(str,command)),'exit_code':r.returncode})
 (root/'out/selection-batch2/release-web-commands.json').write_text(json.dumps(commands,indent=2))
 print(name,'exit',r.returncode,flush=True)
 if r.returncode:
  print((root/f'out/selection-batch2/{name}.log').read_text()[-3500:]);raise SystemExit(r.returncode)
