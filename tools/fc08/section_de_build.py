"""Recorded locked-SDK consumer build for Section D/E. No dependency production."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
sys.path.insert(0,str(ROOT/'tools/skia'))
from fetch import verify_installed
from consumer import load_lock

p=argparse.ArgumentParser();p.add_argument('--repo',required=True);p.add_argument('--profile',choices=['windows','web'],required=True);p.add_argument('--phase',required=True);p.add_argument('--targets',nargs='+');a=p.parse_args()
assert Path(a.repo).resolve()==ROOT
web=a.profile=='web'
out=ROOT/'.aegis/results/FC-08-SECTION-DE'/a.phase;out.mkdir(parents=True,exist_ok=False)
facts=json.loads((ROOT/'out/fc08-section-de'/('web-environment.json' if web else 'environment.json')).read_text())
env={**os.environ,**facts['environment'],'PYTHONUTF8':'1'}
skia=Path('C:/Users/Chado/.codex/worktrees/fc08-verification-materialization/canvas/.deps/skia-sdk')/('web-wasm-webgl2' if web else 'windows-x64-d3d12')/'release'
manifest=verify_installed(skia,load_lock(ROOT/'r1-full-skia-sdk.lock.json'),'web-wasm-webgl2' if web else 'windows-x64-d3d12',ROOT/'tools/skia/profiles/r1-full-v1.json','release')
(out/'skia-identity.json').write_text(json.dumps({'root':str(skia),'sdk_id':manifest['sdk_id'],'manifest_sha256':hashlib.sha256((skia/'manifest.json').read_bytes()).hexdigest(),'read_only_reuse':True},indent=2)+'\n')
build=ROOT/('out/fc08-section-de/web' if web else 'out/fc08-section-c/native')
journal=[]
def run(name,argv):
    r=subprocess.run(list(map(str,argv)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
    for suffix,value in [('stdout',r.stdout),('stderr',r.stderr)]:(out/(name+'.'+suffix+'.log')).write_text(value,encoding='utf-8')
    journal.append({'name':name,'argv':list(map(str,argv)),'exit_code':r.returncode});print(name+': '+str(r.returncode),flush=True)
    (out/'invocations.json').write_text(json.dumps({'cwd':str(ROOT),'source_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'commands':journal,'environment':facts['environment']},indent=2)+'\n')
    return r.returncode
config=['cmake','-S',ROOT,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DBUILD_TESTING=ON','-DCANVAS_BUILD_POC01=OFF','-DCANVAS_BUILD_SEMANTIC=ON','-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON','-DCANVAS_BUILD_RF01=ON','-DCANVAS_BUILD_RENDER=ON','-DCANVAS_RF01_BUILD_TESTS=ON','-DCANVAS_RENDER_BUILD_TESTS='+('OFF' if web else 'ON'),'-DCANVAS_BUILD_INK_PLAYGROUND=OFF','-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON','-DCANVAS_SKIA_SDK_VARIANT=release',f'-DCANVAS_SKIA_SDK_ROOT={skia.as_posix()}','-DAXIOM_BUILD_DEBUG_UI=ON','-DAXIOM_DEBUG_UI_PROFILE=full','-DAXIOM_FC08_QUALIFICATION_HOOKS=OFF',*consumer_cmake_arguments(Path(env['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(env['AXIOM_PROTOC']))]
if web:
    config=['-DCANVAS_RF01_BUILD_TESTS=OFF' if x=='-DCANVAS_RF01_BUILD_TESTS=ON' else x for x in config]
    emsdk=Path('C:/Users/Chado/AppData/Local/Axiom/toolchains/emsdk-6.0.6')
    env['EMSDK']=str(emsdk);env['EM_CONFIG']=str(emsdk/'.emscripten');env['PATH']=str(emsdk/'upstream/emscripten')+os.pathsep+env['PATH']
    env['EMSDK_PYTHON']=sys.executable
    env['EMSDK_NODE']=str(emsdk/'node/24.19.0_64bit/node.exe')
    config.append(f'-DCMAKE_TOOLCHAIN_FILE={(emsdk/"upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake").as_posix()}')
    run('compiler',[sys.executable,emsdk/'upstream/emscripten/emcc.py','--version'])
else:run('compiler',['cl'])
targets=a.targets or (['axiom_canvas_demo_web'] if web else ['axiom_minimal_runtime_host_windows','axiom_fc08_section_consumer_tests','axiom_fc08_section_geometry_tests','axiom_editor_history_tests','axiom_fc05_connector_facade_tests','axiom_fc02_text_tests','axiom_fc03_image_facade_tests','axiom_fc00x_runtime_facade_tests','axiom_brush_batch_frame_retry_tests',
    'canvas_rf01_scene_atomicity_test','canvas_g2_a1_runtime_record_projection','canvas_g2_a2_bounds_system_test',
    'canvas_fc05_route_test','canvas_fc02_shape_label_scene_test','canvas_g2_a3_full_incremental_equivalence',
    'canvas_g2_a6_incremental_equivalence','canvas_g2_a6_atomic_publication','canvas_g2_a6_recovery',
    'canvas_fc03_image_scene_test','canvas_fc01_derived_geometry_test','axiom_dui_m1_control_tests',
    'axiom_selection_session_tests','axiom_transform_handle_drag_tests','axiom_debug_ui_control_router_tests',
    'axiom_debug_ui_workbench_tests','axiom_render_headless_skia_tests','axiom_render_direct_reference_tests',
    'axiom_render_visibility_tests'])
code=run('configure',config)
if not code:code=run('build',['cmake','--build',build,'--target',*targets,'--parallel','6'])
if not code and not web:
    code=run('discover',['ctest','--test-dir',build,'--show-only=json-v1'])
    if not code:
        tests=json.loads((out/'discover.stdout.log').read_text())['tests'];names=[t['name'] for t in tests if Path(t.get('command',[''])[0]).stem in targets]
        assert names;(out/'ctest-names.txt').write_text('\n'.join(names)+'\n')
        code=run('ctest',['ctest','--test-dir',build,'--tests-from-file',out/'ctest-names.txt','--output-on-failure','--timeout','90','--output-junit',out/'ctest.xml'])
files=subprocess.check_output(['git','ls-files','runtime','debug_ui','apps/minimal_runtime_host','apps/axiom_canvas_demo','tools/fc08'],cwd=ROOT).decode().splitlines()
files+=subprocess.check_output(['git','ls-files','--others','--exclude-standard','runtime','debug_ui','apps','tools/fc08'],cwd=ROOT).decode().splitlines()
(out/'source-manifest.json').write_text(json.dumps([{'path':f,'sha256':hashlib.sha256((ROOT/f).read_bytes()).hexdigest()} for f in sorted(set(files)) if (ROOT/f).is_file()],indent=2)+'\n')
raise SystemExit(code)
