"""Compile changed C++ with pinned clang-cl and the actual MSVC ABI flags."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[4]
out = root / 'out/p32-touch'
clang = Path(sys.argv[1])
baseline_header_filter = '--baseline-header-filter' in sys.argv[2:]
if baseline_header_filter:
    # Preserve the raw /WX failure separately. Isolate exactly one unchanged,
    # out-of-scope baseline header; warning policy for changed code stays /WX.
    header = out / 'unchanged-foundation-diagnostic.hpp'
    header.write_text('#pragma clang diagnostic push\n'
                      '#pragma clang diagnostic ignored "-Wsign-compare"\n'
                      '#include "canvas/foundation/stable_order_key.hpp"\n'
                      '#pragma clang diagnostic pop\n')
parser = ctypes.windll.shell32.CommandLineToArgvW
parser.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_int)]
parser.restype = ctypes.POINTER(wintypes.LPWSTR)
sources = {
    'apps/minimal_runtime_host/windows_main.cpp',
    'apps/minimal_runtime_host/tests/windows_touch_pipeline_test.cpp',
    'runtime/composition/src/runtime_composition.cpp',
    'runtime/composition/tests/brush_batch_frame_retry_test.cpp',
    'runtime/input/src/platform_interaction_ingress.cpp',
    'runtime/input/tests/platform_interaction_ingress_test.cpp',
    'runtime/interaction/src/canvas_editing_runtime.cpp',
}
commands = []
with (out / 'clang-checks.log').open('w') as log:
    subprocess.run([str(clang), '--version'], stdout=log, stderr=subprocess.STDOUT, check=True)
    for entry in json.loads((out / 'build/compile_commands.json').read_text()):
        source = Path(entry['file']).relative_to(root).as_posix()
        if source not in sources: continue
        count = ctypes.c_int()
        raw = parser(entry['command'], ctypes.byref(count))
        args = [raw[i] for i in range(count.value)]
        ctypes.windll.kernel32.LocalFree(ctypes.cast(raw, ctypes.c_void_p))
        args = [str(clang), *[a for a in args[1:] if not (
            a.startswith(('/Fo', '/Fd', '@')) or a in ('/FS', '-c', '/c'))], '/Zs']
        assert '/WX' in args
        if baseline_header_filter and any('runtime\\foundation\\include' in a or 'runtime/foundation/include' in a for a in args):
            args.extend(['/FI' + str(header)])
        commands.append(args)
        print(source, flush=True)
        log.write('\nSOURCE ' + source + '\n' + subprocess.list2cmdline(args) + '\n'); log.flush()
        result = subprocess.run(args, cwd=entry['directory'], stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print(f'FAIL {source}: {result.returncode}', flush=True)
            sys.exit(result.returncode)
        sources.remove(source)
assert not sources, sources
(out / 'clang-commands.json').write_text(json.dumps(commands, indent=2))
print('Changed translation units passed clang-cl /W4 /WX /Zs; '
      f'unchanged foundation header filter={baseline_header_filter}.', flush=True)
