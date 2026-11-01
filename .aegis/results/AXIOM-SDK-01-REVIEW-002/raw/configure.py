import json,os,subprocess,sys
from pathlib import Path
sys.path.insert(0, str(Path.cwd()))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
f=json.loads(Path('out/sdk01-review/build-environment.json').read_text(encoding='utf-8'))
os.environ.update(f['environment'])
a=consumer_cmake_arguments(Path(f['environment']['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(f['environment']['AXIOM_PROTOC']))
cmd=['cmake','-S','.','-B','out/sdk01-review/build','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=ON','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RENDER=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DAXIOM_BUILD_DEBUG_UI=ON','-DAXIOM_DEBUG_UI_PROFILE=full','-DCANVAS_BUILD_INK_PLAYGROUND=OFF','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded','-DCANVAS_SKIA_SDK_VARIANT=release','-DCANVAS_SKIA_SDK_ROOT=C:/Users/Chado/.codex/worktrees/fc00x-runtime-minimum-closure/canvas/.deps/skia-sdk/windows-x64-d3d12/release']+a
print('Configure:',subprocess.list2cmdline(cmd),flush=True)
sys.exit(subprocess.call(cmd))

