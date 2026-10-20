import ctypes, hashlib, json, os, pathlib, subprocess, time
root = pathlib.Path(__file__).resolve().parents[2]
out = root / 'out/fc02-resume'
build = out / 'build'
vs = r'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
raw = subprocess.check_output(f'call "{vs}" -arch=x64 -host_arch=x64 >nul && set', shell=True, text=True)
env = {k.upper(): v for k, v in os.environ.items()}
for line in raw.splitlines():
    if '=' in line and not line.startswith('='):
        key, value = line.split('=', 1)
        env[key.upper()] = value
clang = pathlib.Path(r'C:\Users\Chado\.codex\worktrees\fc-01-creation-tools-p32\canvas\.deps\llvm\bin\clang-cl.exe').resolve()
parse = ctypes.windll.shell32.CommandLineToArgvW
parse.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
parse.restype = ctypes.POINTER(ctypes.c_wchar_p)
entries = json.loads((build / 'compile_commands.json').read_text())
results = []
for name in ['apps/minimal_runtime_host/windows_main.cpp', 'apps/minimal_runtime_host/tests/windows_text_tool_entry_test.cpp', 'runtime/composition/src/runtime_composition.cpp', 'runtime/composition/tests/fc02_text_geometry_test.cpp', 'runtime/interaction/src/canvas_text_geometry.cpp', 'runtime/interaction/src/canvas_editing_runtime.cpp', 'runtime/render/src/skia_renderer.cpp']:
    entry = next(e for e in entries if pathlib.Path(e['file']).resolve() == (root / name).resolve())
    count = ctypes.c_int()
    args = parse(entry['command'], ctypes.byref(count))
    values = [args[i] for i in range(count.value)]
    ctypes.windll.kernel32.LocalFree(args)
    command = [str(clang)]
    for value in values[1:]:
        if value in ['-c', '/c', '/FS'] or value.startswith(('/Fo', '/Fd')):
            continue
        command.append(value)
    command += ['/Zs', '/W4', '/WX', '-Wextra', '-Wpedantic', '-Werror']
    logpath = out / ('strict-' + pathlib.Path(name).stem + '.log')
    with logpath.open('w', encoding='utf-8') as log:
        result = subprocess.run(command, cwd=entry['directory'], env=env, stdout=log, stderr=subprocess.STDOUT)
    results.append({'source': name, 'sha256': hashlib.sha256((root/name).read_bytes()).hexdigest(), 'command': command, 'cwd': entry['directory'], 'exit_code': result.returncode, 'log': str(logpath)})
    print(name, result.returncode, logpath.read_text(encoding='utf-8',errors='replace')[-3000:])
(out / 'strict-clang.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
raise SystemExit(any(r['exit_code'] for r in results))
