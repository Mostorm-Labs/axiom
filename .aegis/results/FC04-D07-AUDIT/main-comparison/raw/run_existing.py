from pathlib import Path
import json,subprocess
root=Path.cwd();out=root/'out/fc04-d07';build=out/'build'
targets=['axiom_editor_history_tests','axiom_fc08_section_history_tests','axiom_fc08_section_history_allocation_tests','canvas_semantic_operation_engine_boundary_test','canvas_semantic_operation_engine_generation_change_set_test','canvas_g2_a6_atomic_publication','canvas_g2_a6_recovery','axiom_fc00x_runtime_composition_tests']
data=subprocess.check_output(['ctest','--test-dir',str(build),'--show-only=json-v1'],text=True);(out/'test-discovery.json').write_text(data)
rows=json.loads(data)['tests'];names=[t['name'] for t in rows if Path(t.get('command',[''])[0]).stem in targets]
assert names and {Path(t['command'][0]).stem for t in rows if Path(t.get('command',[''])[0]).stem in targets}==set(targets)
(out/'existing-names.txt').write_text('\n'.join(names)+'\n');print('existing registered cases',len(names),flush=True)
command=['ctest','--test-dir',str(build),'--tests-from-file',str(out/'existing-names.txt'),'--no-tests=error','--output-on-failure','--timeout','180','--output-junit',str(out/'existing.xml')]
with (out/'existing.log').open('w',encoding='utf-8') as log:r=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
(out/'existing-command.json').write_text(json.dumps({'argv':command,'exit_code':r.returncode},indent=2));print('existing exit',r.returncode,flush=True);raise SystemExit(r.returncode)
