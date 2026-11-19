from pathlib import Path
import json,hashlib,subprocess,shutil,time,os
W=Path.cwd();O=W/'out/G5-settled-navigation-recovery-r1';E=W/'.aegis/results/G5-settled-navigation-recovery-r1';B=Path('/Users/qing/.codex/worktrees/g5-erase-feedback-frame');source=B/'out/G5-erase-feedback-frame-r1/source-C-probe';S=O/'source-B-progress'
if not S.exists():
 shutil.copytree(source,S,symlinks=True)
 for n in subprocess.check_output(['git','diff','--name-only'],text=True).splitlines():shutil.copyfile(W/n,S/n)
manifest={str(p.relative_to(S)):hashlib.sha256(p.read_bytes()).hexdigest()for root in ['runtime','sdk','apps','pocs','debug_ui','cmake','schema','assets']for p in(S/root).rglob('*')if p.is_file()};(E/'source-B-progress.json').write_text(json.dumps(manifest,indent=2)+'\n')
args=json.loads((B/'.aegis/results/G5-erase-feedback-frame-r1/C-probe-build.json').read_text())[0]['argv'];args[args.index('-S')+1]=str(S);args[args.index('-B')+1]=str(O/'build-B-progress');env=os.environ.copy();R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');env['EMSDK_PYTHON']='/opt/homebrew/bin/python3.12';env['EMSDK_NODE']=str(R/'.deps/emsdk/node/24.19.0_64bit/bin/node');env['PATH']='/opt/homebrew/opt/python@3.12/libexec/bin:/opt/homebrew/bin:'+str(R/'.deps/emsdk/upstream/emscripten')+':'+str(R/'.deps/emsdk/node/24.19.0_64bit/bin')+':'+env['PATH'];steps=[]
for name,cmd in [('configure',args),('build',['cmake','--build',str(O/'build-B-progress'),'--target','axiom_js_runtime','axiom_cpu_worker_kernel','--parallel','4']),('install',['cmake','--install',str(O/'build-B-progress'),'--prefix',str(O/'install-B-progress')])]:
 t=time.monotonic()
 with(O/('B-progress-'+name+'.log')).open('w')as f:p=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT)
 steps.append({'name':name,'argv':cmd,'exit':p.returncode,'seconds':time.monotonic()-t});(E/'B-progress-build.json').write_text(json.dumps(steps,indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
root=O/'install-B-progress/js';(E/'B-progress-SDK.json').write_text(json.dumps({'root':str(root),'files':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()for p in root.rglob('*')if p.is_file()}},indent=2)+'\n')
