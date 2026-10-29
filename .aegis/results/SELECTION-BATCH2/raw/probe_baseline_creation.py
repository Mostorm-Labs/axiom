from pathlib import Path
import json,re,subprocess
root=Path.cwd();out=root/'out/selection-batch2';probe=out/'baseline-creation-probe';probe.mkdir(exist_ok=True);build=root/'out/selection-batch1/native';git='D:/Softwares/Git/cmd/git.exe'
rows=json.loads((build/'compile_commands.json').read_text());objects=[];commands=[]
for name in ['runtime/scene/src/bounds_system.cpp','runtime/interaction/src/creation_session.cpp']:
 source=probe/Path(name).name;source.write_bytes(subprocess.check_output([git,'show','5daf623e:'+name]));row=next(r for r in rows if r['file'].replace('\\','/').endswith(name));obj=probe/(source.stem+'.obj');objects.append(obj)
 command=re.sub(r'/Fo\S+',lambda m:'/Fo'+str(obj),row['command']);command=re.sub(r'/Fd\S+',lambda m:'/Fd'+str(probe/(source.stem+'.pdb')),command);command=re.sub(r' -c ("[^"]+"|\S+)',lambda m:' -c "'+str(source)+'"',command)
 with (probe/(source.stem+'-compile.log')).open('w') as f:r=subprocess.run(command,shell=True,cwd=build,stdout=f,stderr=subprocess.STDOUT)
 commands.append({'command':command,'cwd':str(build),'exit_code':r.returncode});print('compile',name,r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
s=(build/'build.ninja').read_text().splitlines()
for target,test in [('axiom_fc01_snap_creation_differential_tests','fc01_snap_creation_differential_test'),('axiom_fc01_selected_actions_tests','selected_actions_test')]:
 i=next(i for i,l in enumerate(s) if l.startswith('build ') and target+'.exe:' in l);libs=next(l.split(' = ',1)[1] for l in s[i:i+15] if l.startswith('  LINK_LIBRARIES = '));rsp=probe/(target+'.rsp');testobj=build/f'apps/ink_playground/CMakeFiles/{target}.dir/tests/{test}.cpp.obj'
 rsp.write_text(' '.join('"'+str(o)+'"' for o in objects)+' "'+str(testobj)+'" '+libs);exe=probe/(target+'.exe');args=['link.exe','/nologo','@'+str(rsp),'/out:'+str(exe),'/machine:x64','/INCREMENTAL:NO','/subsystem:console']
 with (probe/(target+'-link.log')).open('w') as f:r=subprocess.run(args,cwd=build,stdout=f,stderr=subprocess.STDOUT)
 commands.append({'argv':args,'cwd':str(build),'exit_code':r.returncode});print('link',target,r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
 with (probe/(target+'-run.log')).open('w') as f:r=subprocess.run([str(exe)],stdout=f,stderr=subprocess.STDOUT)
 commands.append({'argv':[str(exe)],'exit_code':r.returncode});print('run',target,r.returncode,flush=True)
(probe/'commands.json').write_text(json.dumps(commands,indent=2))
