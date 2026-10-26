"""Build/test Section A against locked published dependencies, with raw evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime,timezone

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments

def main():
 p=argparse.ArgumentParser();p.add_argument('--phase',required=True);p.add_argument('--targets',nargs='+',default=['axiom_fc08_section_codec_tests']);p.add_argument('--regex',default='^fc08.section_a.');p.add_argument('--selected-target-tests',action='store_true');a=p.parse_args()
 out=ROOT/'.aegis/results/FC-08-SECTION-A'/a.phase;out.mkdir(parents=True,exist_ok=False)
 facts=json.loads((ROOT/'out/fc08-section-a/environment.json').read_text());env={**os.environ,**facts['environment'],'PYTHONUTF8':'1'}
 build=ROOT/'out/fc08-section-a/build';commands=[]
 def run(name,argv):
  started=datetime.now(timezone.utc).isoformat();r=subprocess.run(list(map(str,argv)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
  (out/(name+'.stdout.log')).write_text(r.stdout,encoding='utf-8');(out/(name+'.stderr.log')).write_text(r.stderr,encoding='utf-8')
  commands.append({'name':name,'argv':list(map(str,argv)),'exit_code':r.returncode,'started_utc':started,'ended_utc':datetime.now(timezone.utc).isoformat()})
  print(name+': '+str(r.returncode),flush=True);return r.returncode
 configure=['cmake','-S',ROOT,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=ON',
  '-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON',
  '-DCANVAS_BUILD_INK_PLAYGROUND=OFF','-DCANVAS_BUILD_RENDER=OFF','-DCANVAS_BUILD_RF01=OFF','-DCANVAS_SKIA_SDK_VARIANT=release',
  '-DAXIOM_FC08_QUALIFICATION_HOOKS=OFF',*consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(env['AXIOM_PROTOC']))]
 code=run('configure',configure)
 if code==0:code=run('build',['cmake','--build',build,'--target',*a.targets,'--parallel','6'])
 if code==0:
  selection=['-R',a.regex]
  if a.selected_target_tests:
   code=run('discover',['ctest','--test-dir',build,'--show-only=json-v1'])
   if code==0:
    tests=json.loads((out/'discover.stdout.log').read_text())['tests']
    names=[t['name'] for t in tests if Path(t.get('command',[''])[0]).stem in a.targets]
    if not names:raise RuntimeError('No tests selected for the built targets')
    (out/'ctest-names.txt').write_text('\n'.join(names)+'\n',encoding='utf-8')
    selection=['--tests-from-file',out/'ctest-names.txt']
  if code==0:code=run('ctest',['ctest','--test-dir',build,*selection,'--output-on-failure','--output-junit',out/'ctest.xml'])
 (out/'invocations.json').write_text(json.dumps({'phase':a.phase,'cwd':str(ROOT),'commands':commands,'exit_code':code,'SDK_environment':facts['environment'],
  'MSVC_environment_initialized':bool(env.get('VCToolsInstallDir')),'source_head':subprocess.check_output(['D:/Softwares/Git/cmd/git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip()},indent=2)+'\n')
 raise SystemExit(code)

if __name__=='__main__':main()
