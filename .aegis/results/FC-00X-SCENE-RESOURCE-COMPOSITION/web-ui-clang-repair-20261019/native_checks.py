import json
from pathlib import Path
import re
import subprocess

root = Path.cwd()
out = root / 'out/p32-web-ui'
selected = json.loads((root / 'out/p32-touch/selected-commands.json').read_text())[1][4][2:-2].split('|')
selected += ['G3WebSurfaceHost']
files = ['runtime/input/CMakeLists.txt', 'runtime/interaction/CMakeLists.txt',
         'runtime/render/CMakeLists.txt', 'runtime/composition/CMakeLists.txt',
         'apps/minimal_runtime_host/CMakeLists.txt', 'apps/axiom_canvas_demo/CMakeLists.txt',
         'debug_ui/CMakeLists.txt']
pairs = dict(p for name in files for p in re.findall(r'add_test\(NAME\s+(\w+)\s+COMMAND\s+(\w+)', (root / name).read_text()))
assert all(name in pairs for name in selected)
targets = sorted({pairs[name] for name in selected} | {
    'canvas_semantic_apply_plan_test', 'canvas_semantic_atomic_apply_test',
    'canvas_semantic_operation_engine_apply_test', 'canvas_semantic_resource_manifest_test'})
regex = '^(' + '|'.join(selected) + r'|ApplyPlan\..*|AtomicApply\..*|OperationEngineApply\..*|resource_manifest\..*)$'
commands = [
    ['cmake', '--build', 'out/p32-touch/build', '--target', *targets, '-j', '4'],
    ['ctest', '--test-dir', 'out/p32-touch/build', '-R', regex, '--output-on-failure', '--output-junit', str(out / 'native-tests.xml')]]
(out / 'native-commands.json').write_text(json.dumps(commands, indent=2))
for name, command in zip(['native-build.log', 'native-tests.log'], commands):
    with (out / name).open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    print(f'{name}: exit={result.returncode}', flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
