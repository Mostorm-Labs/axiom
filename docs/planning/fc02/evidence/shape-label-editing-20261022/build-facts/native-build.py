import json, os, pathlib, subprocess, sys, time
root = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
output = root / 'out/fc02-resume'
build = output / 'build'
vs = r'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
raw = subprocess.check_output(f'call "{vs}" -arch=x64 >nul && set', shell=True, text=True)
env = {k.upper(): v for k, v in os.environ.items()}
for line in raw.splitlines():
    if '=' in line and not line.startswith('='):
        key, value = line.split('=', 1)
        env[key.upper()] = value
facts = json.loads((output / 'build-environment.json').read_text())['environment']
env.update({k.upper(): v for k, v in facts.items()})
action = sys.argv[1]
if action == 'format-build':
    command = ['cmake','--build',str(build),'--parallel','6','--target','axiom_minimal_windows_text_tool_entry_tests','axiom_fc02_text_tests']
    with (output/'format-build.log').open('w',encoding='utf-8') as log:
        r=subprocess.run(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
    print((output/'format-build.log').read_text(errors='replace')[-4000:])
    raise SystemExit(r.returncode)
if action == 'format-red':
    cmds=[[str(build/'apps/minimal_runtime_host/axiom_minimal_windows_text_tool_entry_tests.exe'),'ink-double-click'],[str(build/'runtime/composition/axiom_fc02_text_tests.exe'),'--gtest_filter=FC02Geometry.TextFrameShowsWrapExtentAfterEditingEnds']]
    with (output/'format-red.log').open('w',encoding='utf-8') as log:
        codes=[subprocess.run(c,cwd=build/'apps/minimal_runtime_host',env=env,stdout=log,stderr=subprocess.STDOUT).returncode for c in cmds]
    print(codes);print((output/'format-red.log').read_text(errors='replace')[-6000:])
    raise SystemExit(any(codes))
if action == 'configure':
    command = ['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG', '-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG', '-DBUILD_TESTING=ON', '-DCANVAS_BUILD_POC01=OFF',
        '-DCANVAS_BUILD_SEMANTIC=ON', '-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON',
        '-DCANVAS_BUILD_RENDER=ON', '-DCANVAS_BUILD_RF01=ON',
        '-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON', '-DAXIOM_BUILD_DEBUG_UI=ON',
        '-DAXIOM_DEBUG_UI_PROFILE=full', '-DCANVAS_SKIA_SDK_VARIANT=release',
        '-DCANVAS_SKIA_SDK_ROOT=C:/Users/Chado/.codex/worktrees/fc00x-runtime-minimum-closure/canvas/.deps/skia-sdk/windows-x64-d3d12/release',
        *consumer_cmake_arguments(pathlib.Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']), pathlib.Path(facts['AXIOM_PROTOC']))]
elif action == 'build':
    targets = (root / 'docs/planning/fc02/evidence/host-geometry/targets.txt').read_text().split()
    targets.append('axiom_minimal_windows_text_tool_entry_tests')
    command = ['cmake', '--build', str(build), '--parallel', '6', '--target', *dict.fromkeys(targets)]
elif action == 'caret-build':
    command = ['cmake', '--build', str(build), '--parallel', '6', '--target', 'axiom_fc02_text_tests']
elif action in ['caret-red', 'caret-green']:
    command = [str(build / 'runtime/composition/axiom_fc02_text_tests.exe'), '--gtest_filter=FC02Geometry.IdleCaretBlinksWithoutEditingOrContinuousRedraw']
elif action == 'ctest':
    names = root / 'docs/planning/fc02/evidence/shape-label-entry-20261021/native/ctest-names.txt'
    command = ['ctest', '--test-dir', str(build), '--tests-from-file', str(names),
        '--output-on-failure', '--timeout', '60', '-j', '4', '--output-junit', str(output / 'ctest.xml')]
elif action == 'python':
    command = [sys.executable, '-m', 'unittest',
        'verification.tests.test_current_semantic_descriptor', 'verification.tests.test_semantic_contract',
        'verification.tests.test_fc02_host_contract', 'verification.tests.test_host_text_font_resource', '-v']
else:
    raise SystemExit('unknown action')
record = {'action': action, 'command': command, 'cwd': str(root), 'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
          'VCToolsVersion': env.get('VCTOOLSVERSION'), 'WindowsSDKVersion': env.get('WINDOWSSDKVERSION'), 'environment': facts}
(output / (action + '-command.json')).write_text(json.dumps(record, indent=2), encoding='utf-8')
with (output / (action + '.log')).open('w', encoding='utf-8') as log:
    result = subprocess.run(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
record['exit_code'] = result.returncode
record['finished_utc'] = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
(output / (action + '-command.json')).write_text(json.dumps(record, indent=2), encoding='utf-8')
print((output / (action + '.log')).read_text(encoding='utf-8', errors='replace')[-12000:])
raise SystemExit(result.returncode)
