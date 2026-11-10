from pathlib import Path
import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
RESULT = Path(__file__).resolve().parent
TASK = RESULT.name
OUTPUT = ROOT / 'out' / TASK
OLD = ROOT / '.aegis/results/G5-UR2c-moving-preview-progress-r1'
sys.path.insert(0, str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n')


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def source_identity():
    path = ROOT / 'verification/runners/g5/moving_erase_progress_runner.py'
    spec = importlib.util.spec_from_file_location('moving_demo_source_identity', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.identity(True)


env = dict(os.environ, EMSDK_PYTHON='/opt/homebrew/bin/python3.12',
           PATH='/opt/homebrew/bin:' + os.environ['PATH'])
checks = []


def run(name, argv):
    write(RESULT / (name + '-command.json'), argv)
    start = time.monotonic()
    with (RESULT / (name + '.log')).open('w') as log:
        process = subprocess.run(argv, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
    row = dict(name=name, returncode=process.returncode, elapsed_seconds=time.monotonic() - start)
    checks.append(row)
    write(RESULT / 'web-normal-demo-build-checks.json', checks)
    print(json.dumps(row), flush=True)
    if process.returncode:
        print((RESULT / (name + '.log')).read_text()[-6000:], flush=True)
        raise SystemExit(process.returncode)


assert subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip() == 'codex/geometry-navigation-integration'
before = source_identity()
assert before['revision'] == '0b3b436e1405d8b5e98ee3ba15af4a84c83a2dad'
write(RESULT / 'web-normal-source-before.json', before)
run('web-normal-environment-offline', [sys.executable, 'tools/setup_build_environment.py',
    '--semantic', '--target', 'web-wasm32', '--offline',
    '--facts-output', str(OUTPUT / 'web-environment.json'), '--status'])
facts = json.loads((OUTPUT / 'web-environment.json').read_text())
runtime = Path(facts['environment']['AXIOM_SEMANTIC_RUNTIME_ROOT'])
protoc = Path(facts['environment']['AXIOM_PROTOC'])
original = json.loads((OLD / 'web-normal-configure-command.json').read_text())
configure = [x.replace('G5-UR2c-moving-preview-progress-r1', TASK) for x in original]
semantic_keys = ('-DCMAKE_PREFIX_PATH=', '-DAXIOM_PROTOC=', '-DCMAKE_FIND_ROOT_PATH=',
                 '-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=', '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=',
                 '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=')
configure = [x for x in configure if not x.startswith(semantic_keys)]
configure.extend(consumer_cmake_arguments(runtime, protoc))
configure.extend(['-DCMAKE_MAKE_PROGRAM=/opt/homebrew/bin/ninja','-DPython3_EXECUTABLE=/opt/homebrew/bin/python3.12'])
run('web-normal-configure', configure)
for phase in ['build', 'install']:
    argv = json.loads((OLD / ('web-normal-' + phase + '-command.json')).read_text())
    run('web-normal-' + phase, [x.replace('G5-UR2c-moving-preview-progress-r1', TASK) for x in argv])

cache = (OUTPUT / 'build-web-normal/CMakeCache.txt').read_text()
for flag, kind, value in [
    ('AXIOM_G5_UR0_DIAGNOSTICS', 'BOOL', 'OFF'),
    ('AXIOM_ENABLE_FRAME_TIMING', 'BOOL', 'OFF'),
    ('AXIOM_INSTALL_DEVELOPER_COMPONENTS', 'BOOL', 'OFF'),
    ('AXIOM_RENDER_CACHE_MIB', 'STRING', '32'),
    ('AXIOM_RENDER_SHARED_MIB', 'STRING', '96'),
]:
    assert flag + ':' + kind + '=' + value in cache, flag

sdk = OUTPUT / 'normal-install/js'
files = {str(p.relative_to(sdk)): sha(p) for p in sorted(sdk.rglob('*')) if p.is_file()}
old_manifest = json.loads((OLD / 'normal-artifact-manifest.json').read_text())
assert len(files) == 36 and set(files) == set(old_manifest['files'])
assert files['axiom.wasm'] != old_manifest['files']['axiom.wasm'], 'Expected the LI-01 engine'
engine = (sdk / 'internal/axiom.mjs').read_text()
assert '_axiom_g5_trace_control' not in engine
after = source_identity()
write(RESULT / 'web-normal-source-after.json', after)
assert after['revision'] == before['revision']
assert after['source_tree_sha256'] == before['source_tree_sha256'], 'Source changed during build'

manifest = dict(root=str(sdk.relative_to(ROOT)), files=files,
    source_tree_sha256=before['source_tree_sha256'], compiled_commit=before['revision'],
    diagnostics='OFF', frame_timing='OFF', native_test_faults='ABSENT',
    developer_components='OFF', render_cache_mib=32, render_shared_mib=96,
    source_stable_during_build=True,
    source_identity=dict(path=str((RESULT / 'web-normal-source-before.json').relative_to(ROOT)),
                         sha256=sha(RESULT / 'web-normal-source-before.json')),
    dependency_facts=dict(path=str((OUTPUT / 'web-environment.json').relative_to(ROOT)),
                          sha256=sha(OUTPUT / 'web-environment.json')),
    note='Fresh ordinary consumer Web SDK for user-requested manual testing. Candidate qualification windows have not run; this is not package acceptance.')
write(RESULT / 'normal-artifact-manifest.json', manifest)
print(json.dumps(dict(status='NORMAL_WEB_SDK_BUILT', commit=before['revision'],
                     source_tree_sha256=before['source_tree_sha256'], members=len(files),
                     wasm_sha256=files['axiom.wasm'])), flush=True)
