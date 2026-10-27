from pathlib import Path
import subprocess,json,sys,hashlib,os
root=Path.cwd();out=root/'out/selection-batch2';build=root/'out/selection-batch1/native'
targets=['axiom_selection_coverage_tests','axiom_render_brush_outline_skia_path_tests','axiom_render_headless_skia_tests','canvas_fc03_image_scene_test','canvas_fc01_derived_geometry_test','axiom_minimal_runtime_host_windows','axiom_selection_session_tests','axiom_region_selection_tests','axiom_selection_input_integration_tests','axiom_transform_session_tests','axiom_transform_handle_drag_tests','axiom_editor_history_tests','axiom_session_conflict_tests','axiom_session_lifecycle_tests','axiom_gesture_arbiter_tests','axiom_viewport_gesture_tests','axiom_platform_interaction_ingress_tests','axiom_multi_contact_coordinator_tests','axiom_c0_canvas_interaction_coordinator_tests','axiom_c0_viewport_interaction_controller_tests','axiom_fc08_section_consumer_tests','axiom_fc08_section_geometry_tests','axiom_fc00x_runtime_facade_tests','axiom_brush_batch_frame_retry_tests','axiom_fc02_text_tests','axiom_fc03_image_facade_tests','axiom_fc05_connector_facade_tests','axiom_minimal_windows_input_pipeline_tests','axiom_minimal_windows_touch_pipeline_tests','axiom_minimal_windows_text_tool_entry_tests','axiom_minimal_section_debug_ui_tests','axiom_debug_ui_control_router_tests','axiom_debug_ui_workbench_tests','axiom_dui_m1_control_tests','axiom_render_editing_overlay_tests']
os.environ.update(json.loads((out/'environment.json').read_text())['environment'])
os.environ['PATH']='D:/Softwares/Git/cmd;'+os.environ['PATH']
commands=[]
def run(name,args):
 with (out/(name+'.log')).open('w',encoding='utf-8') as log:r=subprocess.run(list(map(str,args)),stdout=log,stderr=subprocess.STDOUT)
 commands.append({'name':name,'argv':list(map(str,args)),'exit_code':r.returncode});(out/'native-regression-commands.json').write_text(json.dumps(commands,indent=2));print(name,r.returncode,flush=True)
 if r.returncode:print((out/(name+'.log')).read_text()[-8000:]);raise SystemExit(r.returncode)
sys.path.insert(0,str(root))
from tools.semantic.cmake_consumer import consumer_cmake_arguments
facts=json.loads((out/'environment.json').read_text())['environment']
args=json.loads((root/'out/selection-batch1/configure-native.json').read_text())
args=[a for a in args if not a.startswith(('-DAXIOM_PROTOC=','-DCMAKE_PREFIX_PATH=','-DCMAKE_FIND_ROOT_PATH='))]
args+=consumer_cmake_arguments(Path(facts['AXIOM_SEMANTIC_RUNTIME_ROOT']),Path(facts['AXIOM_PROTOC']))
(out/'configure-native.json').write_text(json.dumps(args,indent=2))
run('native-configure',args)
run('native-regression-build',['cmake','--build',build,'--target',*targets,'--parallel','6'])
raw=subprocess.check_output(['ctest','--test-dir',build,'--show-only=json-v1'],text=True)
(out/'native-test-discovery.json').write_text(raw);tests=json.loads(raw)['tests']
selected=[t for t in tests if Path(t.get('command',[''])[0]).stem in targets]
assert selected
missing=(set(targets)-{'axiom_minimal_runtime_host_windows'})-{Path(t['command'][0]).stem for t in selected};assert not missing,missing
names=[t['name'] for t in selected];(out/'native-ctest-names.txt').write_text('\n'.join(names)+'\n')
print('registered tests',len(names),flush=True)
run('native-regression-ctest',['ctest','--test-dir',build,'--tests-from-file',out/'native-ctest-names.txt','--no-tests=error','--output-on-failure','--timeout','90','--output-junit',out/'native-regression.xml'])
