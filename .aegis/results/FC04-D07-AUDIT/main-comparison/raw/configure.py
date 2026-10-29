from pathlib import Path
import json,sys,subprocess,os
root=Path.cwd();out=root/'out/fc04-d07';sys.path.insert(0,str(root));from tools.semantic.cmake_consumer import consumer_cmake_arguments
facts=json.loads((out/'environment.json').read_text())['environment'];os.environ.update(facts)
sys.path.insert(0,str(root/'tools/skia'));from fetch import verify_installed;from consumer import load_lock
skia=Path('C:/Users/Chado/.codex/worktrees/fc08-verification-materialization/canvas/.deps/skia-sdk/windows-x64-d3d12/release')
verify_installed(skia,load_lock(root/'r1-full-skia-sdk.lock.json'),'windows-x64-d3d12',root/'tools/skia/profiles/r1-full-v1.json','release')
args=['cmake','-S','.', '-B','out/fc04-d07/build','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DBUILD_TESTING=ON','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RENDER=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DCANVAS_BUILD_INK_PLAYGROUND=OFF','-DAXIOM_BUILD_DEBUG_UI=OFF','-DAXIOM_FC08_QUALIFICATION_HOOKS=OFF','-DCANVAS_SKIA_SDK_VARIANT=release',f'-DCANVAS_SKIA_SDK_ROOT={skia.as_posix()}',f'-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES={(root/"verification/fc04_d07/inject.cmake").as_posix()}',*consumer_cmake_arguments(Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(facts['AXIOM_PROTOC']))]
(out/'configure.json').write_text(json.dumps(args,indent=2))
with (out/'configure.log').open('w',encoding='utf-8') as log:r=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
print('configure',r.returncode)
if r.returncode:print((out/'configure.log').read_text()[-4000:])
raise SystemExit(r.returncode)
