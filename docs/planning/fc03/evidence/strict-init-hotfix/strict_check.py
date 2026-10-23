
import ctypes,json,subprocess,pathlib,sys
base=pathlib.Path(__file__).resolve().parents[2]
out=pathlib.Path(__file__).parent
clang=r'D:\ax8\.deps\llvm\bin\clang-cl.exe'
common=['/nologo','/TP','/std:c++20','/MT','/EHsc','/W4','/WX','-Wextra','-Wpedantic','-Werror','/Zs']
includes=['/I'+str(base/'runtime/foundation/include'),'/I'+str(base/'runtime/semantic/include')]
rows=[]
for mode in ['red','green']:
    args=[clang,*common]
    if mode=='red': args+=['/I'+str(out/'red-include')]
    args += includes+[str(out/'aggregate_probe.cpp')]
    result=subprocess.run(args,capture_output=True,text=True,cwd=base)
    (out/(mode+'.log')).write_text(result.stdout+result.stderr)
    rows.append(dict(mode=mode,command=args,exit_code=result.returncode,output=result.stdout+result.stderr))
    print(mode,result.returncode, (result.stdout+result.stderr)[-1800:])
if rows[0]['exit_code']==0 or 'contentScale' not in rows[0]['output'] or rows[1]['exit_code']!=0:
    (out/'strict-results.json').write_text(json.dumps(rows,indent=2));sys.exit(1)
old=pathlib.Path(r'C:\Users\Chado\.codex\worktrees\fc03-image-design\canvas')
commands=json.loads((old/'out/fc03-p32-red/compile_commands.json').read_text())
sources=['runtime/composition/src/canvas_runtime_image.cpp','debug_ui/src/panels/control.cpp','runtime/composition/tests/fc03_image_facade_test.cpp','apps/minimal_runtime_host/tests/windows_image_debug_ui_test.cpp']
argc=ctypes.c_int()
split=ctypes.windll.shell32.CommandLineToArgvW
split.argtypes=[ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_int)]
split.restype=ctypes.POINTER(ctypes.c_wchar_p)
for name in sources:
    row=next(r for r in commands if pathlib.Path(r['file']).resolve()==old/pathlib.Path(name))
    raw=split(row['command'],ctypes.byref(argc))
    args=[raw[i] for i in range(argc.value)]
    ctypes.windll.kernel32.LocalFree(raw)
    filtered=[]
    for arg in args[1:]:
        if arg.lower().startswith(('/fo','/fd')) or arg in ['/FS','-c','/c']:continue
        if arg.startswith(('/I','-I')) and str(old).lower() in arg.lower():
            relative=arg[2:]
            suffix=pathlib.Path(relative).relative_to(old)
            if suffix.parts[0] not in ['out','.deps']:arg=arg[:2]+str(base/suffix)
        if pathlib.Path(arg).as_posix().lower()==pathlib.Path(row['file']).as_posix().lower():arg=str(base/name)
        filtered.append(arg)
    args=[clang,*filtered,'/Zs','-Wextra','-Wpedantic','-Werror']
    result=subprocess.run(args,capture_output=True,text=True,cwd=row['directory'])
    log=result.stdout+result.stderr
    (out/(pathlib.Path(name).stem+'.log')).write_text(log)
    rows.append(dict(source=name,command=args,exit_code=result.returncode,output=log))
    print(name,result.returncode,log[-2300:])
(out/'strict-results.json').write_text(json.dumps(rows,indent=2))
sys.exit(0 if all(r['exit_code']==0 for r in rows[1:]) else 1)
