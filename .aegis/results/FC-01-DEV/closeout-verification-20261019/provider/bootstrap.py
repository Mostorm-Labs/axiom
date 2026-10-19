import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path.cwd()
out = root / 'out/fc01-closeout'
sys.path.insert(0, str(root))
from tools.semantic.cmake_consumer import consumer_cmake_arguments

def git(*args):
    return subprocess.check_output(['git', *args], cwd=root, text=True).strip()

head = git('rev-parse', 'HEAD')
assert git('remote', 'get-url', 'origin') == 'https://github.com/Mostorm-Labs/axiom.git'
assert git('status', '--porcelain') == ''
for rev in ['2942c63b375366ed977b834aa0a48fe2fb551c6e',
            '62a99c439e39636088c55e43cf7d76963bfe20eb',
            'c98110c015f975012f25db5efa66c9679da35630',
            'e42c907f827e33864d80a47ba7f9d5938ba14e3a']:
    subprocess.run(['git', 'merge-base', '--is-ancestor', rev, head], check=True)
package = json.loads((root / '.aegis/packages/FC-01/package.json').read_text())
bindings = []
for name, ref in package.items():
    if name.endswith('_ref') and isinstance(ref, dict) and 'sha256' in ref:
        path = '.aegis/packages/FC-01/' + ref['path']
        actual = hashlib.sha256(subprocess.check_output(['git', 'show', head + ':' + path])).hexdigest()
        assert actual == ref['sha256'], path
        bindings.append({'path': path, 'sha256': actual})
(out / 'preflight.json').write_text(json.dumps({'source': head, 'repository': 'Mostorm-Labs/axiom',
    'branch': git('branch', '--show-current'), 'clean': True, 'bindings': bindings,
    'package_materialization_ref': 'c98110c015f975012f25db5efa66c9679da35630'}, indent=2) + '\n')

vcvars = Path('C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
env = os.environ.copy()
raw = subprocess.check_output(f'cmd.exe /d /s /c ""{vcvars}" >nul && set"', text=True)
for line in raw.splitlines():
    if '=' in line and not line.startswith('='):
        key, value = line.split('=', 1)
        env[key] = value

commands = []
def run(args, name):
    print(name, flush=True)
    with (out / (name + '.log')).open('w') as log:
        result = subprocess.run(args, env=env, cwd=root, stdout=log, stderr=subprocess.STDOUT)
    commands.append({'name': name, 'command': args, 'exit_code': result.returncode})
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    print(name, result.returncode, flush=True)
    if result.returncode:
        print((out / (name + '.log')).read_text(errors='replace')[-5000:])
        sys.exit(result.returncode)

facts = json.loads((out / 'build-environment.json').read_text())['environment']
run([sys.executable, 'tools/setup_build_environment.py', '--semantic', '--offline', '--status'], 'sdk-offline')
args = ['cmake', '-S', '.', '-B', str(out / 'build'), '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl',
        '-DCANVAS_BUILD_POC01=OFF', '-DCANVAS_BUILD_INK_PLAYGROUND=ON',
        '-DCANVAS_BUILD_SEMANTIC=ON', '-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON',
        '-DAXIOM_BUILD_DEBUG_UI=ON', '-DAXIOM_DEBUG_UI_PROFILE=full',
        '-DCANVAS_BUILD_RENDER=ON', '-DCANVAS_BUILD_RF01=ON',
        '-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON', '-DCANVAS_SKIA_SDK_VARIANT=release',
        '-DCANVAS_SKIA_SDK_ROOT=C:/Users/NearHub/Documents/ChatGPT/axiom/.deps/skia-sdk/windows-x64-d3d12/release',
        *consumer_cmake_arguments(Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']), Path(facts['AXIOM_PROTOC']))]
run(args, 'configure')
targets = ['canvas_semantic_fc01_shape2_validation_test', 'canvas_semantic_fc01_shape2_apply_test',
           'canvas_semantic_codec_test', 'canvas_semantic_codec_operation_ingress_test',
           'canvas_semantic_g1_06_snapshot_test', 'canvas_semantic_g1_06_snapshot_bootstrap_test',
           'canvas_semantic_brush_snapshot_v2_test', 'canvas_semantic_brush_engine_codec_test',
           'canvas_semantic_operation_engine_15_operation_conformance_test',
           'axiom_fc01_creation_session_tests', 'axiom_fc01_creation_control_binding_tests',
           'axiom_fc01_snap_creation_differential_tests', 'axiom_fc01_host_publication_truth_tests',
           'axiom_fc01_selected_actions_tests', 'axiom_editor_history_tests',
           'axiom_fc00x_runtime_facade_tests', 'axiom_fc00x_runtime_composition_tests',
           'canvas_fc01_derived_geometry_test', 'axiom_minimal_runtime_host_windows']
# Select the exact available target spelling for the pinned raster/Axis tests.
target_help = subprocess.check_output(['cmake', '--build', str(out / 'build'), '--target', 'help'], env=env, text=True)
(out / 'target-help.txt').write_text(target_help)
for line in target_help.splitlines():
    target = line.split(':', 1)[0].strip()
    if target.endswith('.exe'):
        continue
    if any(token in target for token in ['skia_headless_backend_test', 'operation_engine_axis_publication_truth_test']):
        if '/' not in target and target not in targets:
            targets.append(target)
run(['cmake', '--build', str(out / 'build'), '--target', *targets, '--parallel', '4'], 'build')
