import json
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path.cwd()))
from tools.semantic.cmake_consumer import consumer_cmake_arguments

root = Path.cwd()
facts = json.loads((root / 'out/p32-web/build-environment.json').read_text())['environment']
cxx_flags = '-fexceptions'
if '--local-baseline-filter' in sys.argv:
    header = root / 'out/p32-web/unchanged-foundation-diagnostic.hpp'
    original = root / 'runtime/foundation/include/canvas/foundation/stable_order_key.hpp'
    assert subprocess.check_output(['git', 'diff', 'f1d72f22ad20209c8ccdbdd7474d86c651434914',
                                    'HEAD', '--', str(original)], text=True) == ''
    header.write_text('#pragma clang diagnostic push\n'
                      '#pragma clang diagnostic ignored "-Wsign-compare"\n'
                      f'#include "{original.as_posix()}"\n'
                      '#pragma clang diagnostic pop\n')
    cxx_flags += f' -include {header.as_posix()}'
args = ['cmake', '-S', '.', '-B', 'out/p32-web/build', '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
        '-DBUILD_TESTING=OFF', '-DCMAKE_CXX_SCAN_FOR_MODULES=OFF',
        f'-DCMAKE_TOOLCHAIN_FILE={(root / ".deps/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake").as_posix()}',
        f'-DCMAKE_CXX_FLAGS={cxx_flags}', '-DCMAKE_EXE_LINKER_FLAGS=-fexceptions',
        '-DCANVAS_BUILD_POC01=OFF', '-DCANVAS_BUILD_INK_PLAYGROUND=ON',
        '-DCANVAS_BUILD_SEMANTIC=ON', '-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON',
        '-DAXIOM_BUILD_DEBUG_UI=ON', '-DAXIOM_DEBUG_UI_PROFILE=full',
        '-DCANVAS_BUILD_RENDER=ON', '-DCANVAS_BUILD_RF01=ON',
        '-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON', '-DCANVAS_SKIA_SDK_VARIANT=release',
        f'-DCANVAS_SKIA_SDK_ROOT={(root / ".deps/skia-sdk/web-wasm-webgl2/release").as_posix()}',
        *consumer_cmake_arguments(Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']), Path(facts['AXIOM_PROTOC']))]
if '--local-baseline-filter' in sys.argv:
    assert subprocess.check_output(['git', 'diff', 'f1d72f22ad20209c8ccdbdd7474d86c651434914',
                                    'HEAD', '--', 'runtime/semantic/src/apply_plan.cpp'], text=True) == ''
    args.append(f'-DCMAKE_PROJECT_INCLUDE={(root / "out/p32-web/local-diagnostics.cmake").as_posix()}')
(root / 'out/p32-web/configure-command.json').write_text(json.dumps(args, indent=2) + '\n')
print(subprocess.list2cmdline(args), flush=True)
sys.exit(subprocess.call(args))
