from pathlib import Path
import json,subprocess,ctypes,shlex,shutil,sys
r=Path(__file__).resolve().parent;b=Path.cwd()/'out/p32-f08-msvc1951'
es=json.loads((b/'compile_commands.json').read_text());e=next(x for x in es if x['file'].replace('\\','/').endswith('/debug_ui/tests/control_router_test.cpp'))
n=ctypes.c_int();parse=ctypes.windll.shell32.CommandLineToArgvW;parse.argtypes=[ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_int)];parse.restype=ctypes.POINTER(ctypes.c_wchar_p);v=parse(e['command'],ctypes.byref(n));args=[v[i] for i in range(n.value)];ctypes.windll.kernel32.LocalFree(v)
opts=[a for a in args[1:] if a not in ('-c','/c','--','/DNDEBUG','-DNDEBUG') and not a.startswith(('/Fo','/Fd','-clang:-M')) and a.replace('\\','/')!=e['file'].replace('\\','/')]
p=subprocess.run([args[0],*opts,'/UNDEBUG','/I'+str(Path.cwd()/'debug_ui/include'),'/I'+str(Path.cwd()/'.deps/imgui'),'/I'+str(Path.cwd()/'apps/minimal_runtime_host'),'/I'+str(Path.cwd()/'apps/axiom_canvas_demo/platform/web'),'/Fo'+str(r/'boundary_probe.obj'),'-c',str(r/'boundary_probe.cpp')],cwd=b,capture_output=True,text=True);(r/'compile.log').write_text(p.stdout+p.stderr,encoding='utf-8');print('compile_exit',p.returncode,flush=True)
if p.returncode:print(p.stdout+p.stderr);sys.exit(p.returncode)
lines=(b/'build.ninja').read_text().splitlines();libs=[]
for target in ['axiom_fc00x_runtime_facade_tests.exe:','axiom_debug_ui_control_router_tests.exe:','axiom_minimal_runtime_host_tests.exe:','axiom_demo_web_surface_tests.exe:']:
 i=next(i for i,l in enumerate(lines) if l.startswith('build ') and target in l);block=[]
 for l in lines[i+1:]:
  if not l.strip():break
  block.append(l)
 value=next(l.split('=',1)[1].strip() for l in block if l.strip().startswith('LINK_LIBRARIES =')).replace('$:',':').replace('$ ',' ')
 libs.extend(a.strip('"') for a in shlex.split(value,posix=False))
libs=list(dict.fromkeys(libs));rsp=r/'link.rsp';rsp.write_text('\n'.join('"'+a+'"' for a in [str(r/'boundary_probe.obj'),*libs]))
p=subprocess.run([shutil.which('link.exe'),'/nologo','/machine:x64','/subsystem:console','/INCREMENTAL:NO','/out:'+str(r/'boundary_probe.exe'),'@'+str(rsp)],cwd=b,capture_output=True,text=True);(r/'link.log').write_text(p.stdout+p.stderr,encoding='utf-8');print('link_exit',p.returncode,flush=True)
if p.returncode:print(p.stdout+p.stderr);sys.exit(p.returncode)
p=subprocess.run([str(r/'boundary_probe.exe')],cwd=r,capture_output=True,text=True);(r/'output.log').write_text(p.stdout+p.stderr,encoding='utf-8');print('run_exit',p.returncode);print(p.stdout+p.stderr);sys.exit(p.returncode)




