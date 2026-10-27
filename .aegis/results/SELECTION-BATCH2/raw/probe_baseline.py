from pathlib import Path
import json,re,subprocess,shutil
root=Path.cwd();out=root/'out/selection-batch2';probe=out/'baseline-raster-probe';probe.mkdir(exist_ok=True);build=root/'out/selection-batch1/native'
source=probe/'baseline_skia_headless_backend.cpp';source.write_bytes(subprocess.check_output(['D:/Softwares/Git/cmd/git.exe','show','5daf623e:runtime/render/src/skia_headless_backend.cpp']))
rows=json.loads((build/'compile_commands.json').read_text());row=next(r for r in rows if r['file'].endswith('skia_headless_backend.cpp'))
obj=probe/'baseline_backend.obj';command=row['command'];command=re.sub(r'/Fo\S+',lambda m:'/Fo'+str(obj),command);command=re.sub(r'/Fd\S+',lambda m:'/Fd'+str(probe/'baseline_backend.pdb'),command)
command=re.sub(r' -c ("[^"]+"|\S+)',lambda m:' -c "'+str(source)+'"',command);assert str(source) in command
commands=[]
with (probe/'compile.log').open('w') as f:r=subprocess.run(command,shell=True,cwd=build,stdout=f,stderr=subprocess.STDOUT)
commands.append({'command':command,'cwd':str(build),'exit_code':r.returncode});print('baseline renderer compile',r.returncode,flush=True)
if r.returncode:print((probe/'compile.log').read_text()[-1500:]);raise SystemExit(r.returncode)
s=(build/'build.ninja').read_text().splitlines();i=next(i for i,l in enumerate(s) if l.startswith('build ') and 'axiom_render_headless_skia_tests.exe:' in l)
libs=next(l.split(' = ',1)[1] for l in s[i:i+15] if l.startswith('  LINK_LIBRARIES = '))
rsp=probe/'link.rsp';rsp.write_text('"'+str(obj)+'" "'+str(build/'runtime/render/CMakeFiles/axiom_render_headless_skia_tests.dir/tests/skia_headless_backend_test.cpp.obj')+'" '+libs)
exe=probe/'baseline_probe.exe';args=['link.exe','/nologo','@'+str(rsp),'/out:'+str(exe),'/machine:x64','/INCREMENTAL:NO','/subsystem:console']
with (probe/'link.log').open('w') as f:r=subprocess.run(args,cwd=build,stdout=f,stderr=subprocess.STDOUT)
commands.append({'argv':args,'cwd':str(build),'exit_code':r.returncode});print('baseline link',r.returncode,flush=True)
if r.returncode:print((probe/'link.log').read_text()[-1500:]);raise SystemExit(r.returncode)
with (probe/'run.log').open('w') as f:r=subprocess.run([str(exe),'fc01'],stdout=f,stderr=subprocess.STDOUT)
commands.append({'argv':[str(exe),'fc01'],'exit_code':r.returncode});(probe/'commands.json').write_text(json.dumps(commands,indent=2));print('baseline fc01 exit',r.returncode);print((probe/'run.log').read_text()[-600:])
