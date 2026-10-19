import json
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path.cwd()))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
facts = json.loads(Path('out/build-environment.json').read_text())['environment']
args = ['cmake', '-S', '.', '-B', 'out/p32-touch/build', '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl',
        '-DCANVAS_BUILD_POC01=OFF', '-DCANVAS_BUILD_INK_PLAYGROUND=OFF',
        '-DCANVAS_BUILD_SEMANTIC=ON', '-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON',
        '-DAXIOM_BUILD_DEBUG_UI=ON', '-DAXIOM_DEBUG_UI_PROFILE=full',
        '-DCANVAS_BUILD_RENDER=ON', '-DCANVAS_BUILD_RF01=ON',
        '-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON', '-DCANVAS_SKIA_SDK_VARIANT=release',
        '-DCANVAS_SKIA_SDK_ROOT=C:/Users/NearHub/Documents/ChatGPT/axiom/.deps/skia-sdk/windows-x64-d3d12/release',
        *consumer_cmake_arguments(Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']), Path(facts['AXIOM_PROTOC']))]
print(subprocess.list2cmdline(args), flush=True)
sys.exit(subprocess.call(args))
