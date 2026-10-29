import json,os,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path.cwd()))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
facts=json.loads(Path('out/sdk-cpp-first-web-facts.json').read_text())
env=os.environ.copy(); env.update(facts['environment'])
emsdk=Path('C:/Users/Chado/AppData/Local/Axiom/toolchains/emsdk-6.0.6')
env['EM_CONFIG']=str(emsdk/'.emscripten')
env['EMSDK_PYTHON']=sys.executable
env['EMSDK_NODE']=str(emsdk/'node/24.19.0_64bit/node.exe')
env['EMSDK']=str(emsdk)
env['PATH']=str(emsdk/'upstream/emscripten')+os.pathsep+env['PATH']
root=Path(facts['environment']['AXIOM_SEMANTIC_RUNTIME_ROOT']);protoc=Path(facts['environment']['AXIOM_PROTOC'])
skia=Path('C:/Users/Chado/.codex/worktrees/next-stage-main/canvas/.deps/skia-sdk/web-wasm-webgl2/release')
args=['cmake','-S','.','-B','out/sdk-cpp-first-web','-G','Ninja',f'-DCMAKE_TOOLCHAIN_FILE={(emsdk/"upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake").as_posix()}','-DCMAKE_MAKE_PROGRAM=D:/git/depot_tools/ninja.exe','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_INK_PLAYGROUND=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RENDER=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_BUILD_CONTROL=ON','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DAXIOM_BUILD_DEBUG_UI=ON','-DAXIOM_DEBUG_UI_PROFILE=full','-DCANVAS_SKIA_SDK_VARIANT=release',f'-DCANVAS_SKIA_SDK_ROOT={skia.as_posix()}',*consumer_cmake_arguments(root,protoc)]
if '--build' in sys.argv: args=['cmake','--build','out/sdk-cpp-first-web','--target','axiom_canvas_demo_web','--parallel','6']
print('COMMAND',json.dumps(args),flush=True)
p=subprocess.run(args,env=env,check=False);sys.exit(p.returncode)
