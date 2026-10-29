from pathlib import Path
import json,re,subprocess
root=Path.cwd();out=root/'out/selection-batch2';probe=out/'snap-check2';probe.mkdir(exist_ok=True);build=root/'out/selection-batch1/native';git='D:/Softwares/Git/cmd/git.exe'
rows=json.loads((build/'compile_commands.json').read_text());objects=[];commands=[]
name='apps/ink_playground/tests/fc01_snap_creation_differential_test.cpp'
source=probe/'trace_snap.cpp';content=(root/name).read_text();a=content.index('int main(');prefix=content[:a];main=content[a:]
main=re.sub(r'^(  )([a-z][a-z_]+)\(\);',lambda m:'  std::cerr<<"TRACE '+m[2]+'\\n"<<std::flush;\n'+m[0],main,flags=re.M)
# File-relative qualification headers retain their original resolution.
checker='\n#undef assert\n#define assert(x) do { if(!(x)) { std::cerr << \"CHECK FAILED \" << #x << \" line \" << __LINE__ << std::endl; std::exit(1); } } while(false)\n'
source.write_text(prefix.replace('namespace {',checker+'\nnamespace {',1)+main);row=next(r for r in rows if r['file'].replace('\\','/').endswith(name));obj=probe/'trace_snap.obj';objects=[obj]
command=re.sub(r'/Fo\S+',lambda m:'/Fo'+str(obj),row['command']);command=re.sub(r'/Fd\S+',lambda m:'/Fd'+str(probe/'trace_snap.pdb'),command);command=re.sub(r' -c ("[^"]+"|\S+)',lambda m:' -c "'+str(source)+'"',command)
command+=' -I"'+str(root/'apps/ink_playground/tests')+'"'
with (probe/'trace-compile.log').open('w') as f:r=subprocess.run(command,shell=True,cwd=build,stdout=f,stderr=subprocess.STDOUT)
commands.append({'command':command,'cwd':str(build),'exit_code':r.returncode});print('compile trace',r.returncode,flush=True)
if r.returncode:print((probe/'trace-compile.log').read_text()[-1800:]);raise SystemExit(r.returncode)
s=(build/'build.ninja').read_text().splitlines()
for target,test in [('axiom_fc01_snap_creation_differential_tests','fc01_snap_creation_differential_test')]:
 i=next(i for i,l in enumerate(s) if l.startswith('build ') and target+'.exe:' in l);libs=next(l.split(' = ',1)[1] for l in s[i:i+15] if l.startswith('  LINK_LIBRARIES = '));rsp=probe/(target+'.rsp');testobj=build/f'apps/ink_playground/CMakeFiles/{target}.dir/tests/{test}.cpp.obj'
 rsp.write_text(' '.join('"'+str(o)+'"' for o in objects)+' '+libs);exe=probe/(target+'.exe');args=['link.exe','/nologo','@'+str(rsp),'/out:'+str(exe),'/machine:x64','/INCREMENTAL:NO','/subsystem:console']
 with (probe/(target+'-link.log')).open('w') as f:r=subprocess.run(args,cwd=build,stdout=f,stderr=subprocess.STDOUT)
 commands.append({'argv':args,'cwd':str(build),'exit_code':r.returncode});print('link',target,r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
 with (probe/(target+'-run.log')).open('w') as f:r=subprocess.run([str(exe)],stdout=f,stderr=subprocess.STDOUT)
 commands.append({'argv':[str(exe)],'exit_code':r.returncode});print('run',target,r.returncode,flush=True)
(probe/'commands.json').write_text(json.dumps(commands,indent=2))
