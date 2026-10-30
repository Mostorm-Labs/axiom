"""Native existing-owner compile/regression; no FC08 Scene/UI feature claim."""
import argparse, json, os, subprocess, sys, hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
sys.path.insert(0,str(ROOT/'tools/skia'))
from fetch import verify_installed
from consumer import load_lock
p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--phase',required=True);p.add_argument('--skia',required=True);a=p.parse_args()
assert Path(a.repo).resolve()==ROOT
out=ROOT/'.aegis/results/FC-08-SECTION-C'/a.phase;out.mkdir(parents=True,exist_ok=False)
skia=Path(a.skia).resolve();manifest=verify_installed(skia,load_lock(ROOT/'r1-full-skia-sdk.lock.json'),'windows-x64-d3d12',ROOT/'tools/skia/profiles/r1-full-v1.json','release')
(out/'skia-identity.json').write_text(json.dumps({'root':str(skia),'sdk_id':manifest['sdk_id'],'manifest_sha256':hashlib.sha256((skia/'manifest.json').read_bytes()).hexdigest(),'read_only_reuse':True},indent=2))
facts=json.loads((ROOT/'out/fc08-section-c/environment.json').read_text());env={**os.environ,**facts['environment'],'PYTHONUTF8':'1'}
build=ROOT/'out/fc08-section-c/native';journal=[]
def run(name,argv):
 r=subprocess.run(list(map(str,argv)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
 for suffix,value in [('stdout',r.stdout),('stderr',r.stderr)]: (out/(name+'.'+suffix+'.log')).write_text(value,encoding='utf-8')
 journal.append({'argv':list(map(str,argv)),'exit_code':r.returncode});print(name+': '+str(r.returncode),flush=True);return r.returncode
code=run('configure',['cmake','-S',ROOT,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DBUILD_TESTING=ON','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_BUILD_RENDER=ON','-DCANVAS_RF01_BUILD_TESTS=OFF','-DCANVAS_RENDER_BUILD_TESTS=OFF','-DAXIOM_BUILD_RUNTIME_QUALIFICATION=OFF','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DCANVAS_SKIA_SDK_VARIANT=release',f'-DCANVAS_SKIA_SDK_ROOT={skia.as_posix()}','-DAXIOM_FC08_QUALIFICATION_HOOKS=OFF',*consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(env['AXIOM_PROTOC']))])
targets=['axiom_editor_history_tests','axiom_fc05_connector_facade_tests','axiom_fc02_text_tests','axiom_fc03_image_facade_tests','axiom_fc00x_runtime_facade_tests','axiom_brush_batch_frame_retry_tests']
if code==0:code=run('build',['cmake','--build',build,'--target',*targets,'--parallel','6'])
if code==0:code=run('discover',['ctest','--test-dir',build,'--show-only=json-v1'])
if code==0:
 tests=json.loads((out/'discover.stdout.log').read_text())['tests'];names=[t['name'] for t in tests if Path(t.get('command',[''])[0]).stem in targets]
 assert names
 (out/'ctest-names.txt').write_text('\n'.join(names)+'\n')
 code=run('ctest',['ctest','--test-dir',build,'--tests-from-file',out/'ctest-names.txt','--output-on-failure','--timeout','60','--output-junit',out/'ctest.xml'])
(out/'invocations.json').write_text(json.dumps({'cwd':str(ROOT),'commands':journal,'exit_code':code,'environment':facts['environment']},indent=2)+'\n')
sys.exit(code)
