from pathlib import Path
import json,os,subprocess
root=Path.cwd();out=root/'.aegis/results/FC-08-SECTION-DE/create-button-fix-001'
env={**os.environ,**json.loads((root/'out/fc08-section-de/environment.json').read_text())['environment']}
args=['cmake','-S',str(root),'-B',str(root/'out/fc08-section-c/native'),'-DCMAKE_RUNTIME_OUTPUT_DIRECTORY='+str(root/'out/fc08-section-button/7c09dc7f')]
r=subprocess.run(args,cwd=root,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
(out/'output-directory.stdout.log').write_text(r.stdout,encoding='utf-8');(out/'output-directory.stderr.log').write_text(r.stderr,encoding='utf-8')
(out/'output-directory.json').write_text(json.dumps({'argv':args,'exit_code':r.returncode,'reason':'Keep running original Windows PID 251456 and its file/content intact; link the new executable to a separate revision directory'},indent=2)+'\n')
raise SystemExit(r.returncode)
