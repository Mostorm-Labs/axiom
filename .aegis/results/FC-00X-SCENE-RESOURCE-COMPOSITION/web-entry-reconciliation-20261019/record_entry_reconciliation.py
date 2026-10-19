import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path.cwd()
out = root / 'out/p32-web'
destination = root / '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/web-entry-reconciliation-20261019'
destination.mkdir(parents=True, exist_ok=True)
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
assert subprocess.run(['git', 'diff', '--exit-code', '--', 'apps', 'runtime', 'debug_ui']).returncode == 0
assert subprocess.run(['git', 'diff', '--exit-code', 'bd3eaf8af26bf710de7a0458a5fc9920d71b5589',
                       'HEAD', '--', 'apps', 'runtime', 'debug_ui']).returncode == 0
reference = json.loads((out / 'reference-entry-audit-result.json').read_text())
assert reference['status'] == 'FAIL' and reference['afterSamples'] == 0
assert len(reference['errors']) == 3
assert all('BigInt' in entry['message'] for entry in reference['errors'])
artifacts = []
for app, name in [('ink_playground', 'axiom_ink_playground_web.wasm'),
                  ('axiom_canvas_demo', 'axiom_canvas_demo_web.wasm')]:
    path = out / 'build/apps' / app / name
    artifacts.append({'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
checks = [
    {'name': 'selected-browser-entry', 'expected': 'P32 Web Minimal consumer using RuntimeComposition',
     'actual': 'Opened /ink_playground/, a qualification consumer using InkPlaygroundHost directly', 'result': 'FAIL'},
    {'name': 'user-reported-drawing', 'expected': 'Drawing visible on the requested Web consumer',
     'actual': 'User: 什么都画不上去，只有debug ui可以打开; observed on the mistakenly selected Playground page', 'result': 'FAIL'},
    {'name': 'correct-web-composition-owner', 'expected': 'One existing RuntimeComposition, no Playground core dependency',
     'actual': 'axiom_canvas_demo_web -> axiom_canvas_demo_common -> RuntimeComposition; WebSurfaceHost owns composition and exposes facade()', 'result': 'PASS'},
    {'name': 'web-debug-on-controls', 'expected': 'Usable controls through RuntimeFacade for the requested Minimal workflow',
     'actual': 'Web reference HTML contains only 320x180 canvases; Web executable target has no optional canvas_debug_ui linkage or facade control bindings', 'result': 'FAIL'},
    {'name': 'correct-reference-pointer-js-wasm', 'expected': 'Three Down/Move/Up samples enter Runtime',
     'actual': reference, 'result': 'FAIL'},
    {'name': 'product-source-preservation', 'expected': 'Identify the right consumer before changing product code',
     'actual': 'No apps/runtime/debug_ui source changes from f3df2879; product code also unchanged from the built bd3eaf8a snapshot', 'result': 'PASS'},
    {'name': 'correct-minimal-human-interactions', 'expected': 'Physical matrix executed against correct Minimal Web consumer',
     'actual': 'Not observed; entry is not ready for the complete requested workflow', 'result': 'PHYSICAL_PENDING'},
]
record = {'source_revision': revision, 'scope': 'Entry/owner reconciliation and diagnostic RED only; no product fix',
          'status': 'WEB_MINIMAL_CONSUMER_NOT_READY', 'checks': checks, 'artifacts': artifacts,
          'previous_results': {'materialized_ref': revision,
              'startup_pass_scope': 'Only the legacy Ink Playground page; not Minimal Web functional evidence',
              'eight_source_contract_cases': 'Playground source checks; not Minimal Web interaction coverage',
              'G3WebSurfaceHost': 'Native WebSurfaceHost contract test; not browser drawing/UI equivalence'},
          'protected_history_modified': False, 'official_gate_or_phys_status_changed': False,
          'next_repair_scope': 'Existing apps/axiom_canvas_demo/platform/web consumer, its optional Debug UI/facade wiring and relevant tests; no legacy Playground repair or Runtime gesture redesign'}
(destination / 'result.json').write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
for name in ('reference-entry-audit.html', 'run_entry_audit.py', 'reference-entry-audit-result.json',
             'reference-entry-audit-stderr.log', 'input-diagnostic.html', 'run_input_diagnostic.py',
             'input-diagnostic-result.json', 'input-diagnostic-stderr.log', 'record_entry_reconciliation.py'):
    shutil.copy2(out / name, destination / name)
dependencies = (out / 'reference-target-dependencies.txt').read_text()
assert 'libaxiom_ink_playground_common' not in dependencies
assert 'libcanvas_debug_ui' not in dependencies
shutil.copy2(out / 'reference-target-dependencies.txt', destination / 'reference-target-dependencies.txt')
print(json.dumps({'source_revision': revision, 'status': record['status'], 'evidence': str(destination)}), flush=True)
