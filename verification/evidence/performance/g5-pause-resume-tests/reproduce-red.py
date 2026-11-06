from pathlib import Path
import json,shlex,subprocess,shutil,hashlib
R=Path.cwd();B=R/'out/host';D=R/'out/g5-reentry-tests-red';D.mkdir(exist_ok=True)
source=D/'skia_renderer.cpp';source.write_bytes(subprocess.check_output(['git','show','108151fb:runtime/render/src/skia_renderer.cpp']))
entry=next(x for x in json.loads((B/'compile_commands.json').read_text())if x['file'].endswith('runtime/render/src/skia_renderer.cpp'))
args=shlex.split(entry['command']);args[args.index('-o')+1]=str(D/'skia_renderer.cpp.o');args[args.index('-c')+1]=str(source);args+=['-I',str(R/'runtime/render/src')]
log=(D/'build.log').open('w');subprocess.run(args,cwd=B,stdout=log,stderr=subprocess.STDOUT,check=True)
archive=D/'libcanvas_runtime_render.a';shutil.copyfile(B/'runtime/render/libcanvas_runtime_render.a',archive)
subprocess.run(['ar','r',str(archive),str(D/'skia_renderer.cpp.o')],stdout=log,stderr=subprocess.STDOUT,check=True)
link=shlex.split((R/'out/g5-reentry-tests-link.txt').read_text())[2:-2]
link[link.index('-o')+1]=str(D/'red-test');link=[str(archive)if x=='runtime/render/libcanvas_runtime_render.a'else x for x in link]
subprocess.run(link,cwd=B,stdout=log,stderr=subprocess.STDOUT,check=True);log.close()
p=subprocess.run([str(D/'red-test'),'--mixed-reentry-only'],capture_output=True,text=True)
(D/'run.log').write_text(p.stdout+p.stderr)
result={'rendererRevision':'108151fb','testSourceSha256':hashlib.sha256((R/'runtime/render/tests/render_cache_test.cpp').read_bytes()).hexdigest(),'returncode':p.returncode,'expectedFailure':'mixed paused navigation waited for cold coverage','matched':p.returncode!=0 and 'mixed paused navigation waited for cold coverage' in p.stdout+p.stderr,'isolation':'Old renderer compiled/linked only under out/g5-reentry-tests-red;controlled source and normal build untouched.'}
(D/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));assert result['matched']
