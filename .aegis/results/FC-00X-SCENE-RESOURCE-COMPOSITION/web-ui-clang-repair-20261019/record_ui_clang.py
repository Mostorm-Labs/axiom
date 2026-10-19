import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from datetime import datetime, timezone
import xml.etree.ElementTree as ET

root = Path.cwd()
out = root / 'out/p32-web-ui'
web = root / 'out/p32-web'
evidence = root / '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/web-ui-clang-repair-20261019'
evidence.mkdir(exist_ok=True)
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
def save(name, value):
    (evidence / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

source = read(web / 'repair-on-artifacts.json')['source_revision']
on = read(web / 'ui-clang-on-final-result.json')
off = read(web / 'ui-clang-off-final-result.json')
for result, enabled in [(on, True), (off, False)]:
    assert result['status'] == 'PASS' and len(result['results']) == 13
    state = result['observation']
    assert state['revision'] == source and state['dirty'] == 0 and state['debugUiEnabled'] == enabled
assert [v['panelWidth'] for v in on['uiGeometry']] == [410, 410, 344]
native = ET.parse(out / 'native-tests.xml').getroot()
native_tests = list(native.iter('testcase'))
assert len(native_tests) > 49
assert not list(native.iter('failure')) and not list(native.iter('error'))
assert any('ResourceRemovalChecksEverySupportedContentReference' in t.attrib['name'] for t in native_tests)
for name in '''clang-red.json clang-green.json stable-order-red.log apply-plan-red.log
stable-order-green.log apply-plan-green.log package-bindings.json resource-red.log resource-red-build.log
strict-configure.log strict-build.log strict-precommit-tests-result.json
native-commands.json native-build.log native-tests.log native-tests.xml msvc-version.log
strict-compile-audit.json web-environment.json sdk-offline.log
diagnose_clang.py verify_clang.py audit_build.py strict_build.cmd native_checks.py native_checks.cmd
strict-off-configure.log strict-off-build.log strict-on-final-configure.log strict-on-final-build.log
strict-off-commands.json strict-on-final-commands.json final_variant_build.py final_variant_build.cmd
resource_red.cmd native_host.cmd native-host-build.log native-host-configure.log native-host-artifact.json
browser-launch.json tool-versions.txt http-artifact-verification.json'''.split():
    shutil.copy2(out / name, evidence / name)
for name in '''ui-width-red-result.json ui-width-red-stderr.log
ui-clang-on-final-result.json ui-clang-off-final-result.json
ui-clang-on-final-stderr.log ui-clang-off-final-stderr.log
repair-on-artifacts.json repair-off-artifacts.json repair-on-dependencies.txt repair-off-dependencies.txt
repair-on-bridge-compile.txt repair-off-bridge-compile.txt'''.split():
    shutil.copy2(web / name, evidence / name)
for name in ['run_browser_test.mjs', 'capture_variant.py']:
    shutil.copy2(web / name, evidence / name)
previous = evidence.parent / 'web-minimal-repair-20261019'
for name in ['seal_evidence.py', 'machine.json', 'displays.json']:
    shutil.copy2(previous / name, evidence / name)
changed = subprocess.check_output(['git', 'diff', '--name-only', 'f4254a0d75c88267685831d4c234972014759b4c', source], text=True).splitlines()
expected = {'apps/axiom_canvas_demo/platform/web/index.html', 'apps/axiom_canvas_demo/platform/web/web_debug_ui.inl',
    'apps/axiom_canvas_demo/tests/web_minimal_runtime_test.html',
    'runtime/foundation/include/canvas/foundation/stable_order_key.hpp',
    'runtime/semantic/src/apply_plan.cpp', 'runtime/semantic/tests/apply_plan_test.cpp'}
assert set(changed) == expected
native_artifact = read(out / 'native-host-artifact.json')
native_bytes = Path(native_artifact['path']).read_bytes()
assert hashlib.sha256(native_bytes).hexdigest() == native_artifact['sha256']
assert source.encode() in native_bytes
native_artifact['embedded_source_revision_verified'] = True
save('native-host-artifact.json', native_artifact)
save('scope-audit.json', {'changed_source_files': changed, 'expected_source_files': sorted(expected),
    'result': 'PASS', 'protected_authority_golden_history_phys_android_paths_changed': [],
    'current_source_revision': source, 'old_evidence_directories_modified': False})
save('result.json', {
    'recorded_at': datetime.now(timezone.utc).isoformat(), 'result_revision': source,
    'start_revision': 'f4254a0d75c88267685831d4c234972014759b4c',
    'materialized_ref': 'The evidence commit containing this directory; returned as exact SHA in the task result.',
    'branch': 'codex/p32-windows-minimal-touch', 'changed_source_files': changed,
    'status': 'UI_WIDTH_AND_STRICT_COMPILER_REPAIR_AUTOMATION_PASS',
    'human_feedback': {'quote': '这一轮验证中，整体功能是可以的，只是debug ui的宽度非常大，几乎占据了浏览器画面的4/5，需要修复下，然后就是你报的这个clang诊断阻塞需要修复下',
        'scope': 'Overall repaired Web functional observation; known UI width defect. No per-item matrix acceptance inferred.',
        'repaired_width_physical_observation': 'PHYSICAL_PENDING'},
    'checks': [
        {'name': 'debug-ui-width', 'expected': 'Bound at 410 CSS px with matching 410 px backing and viewport; fit narrow stage',
         'actual': on['uiGeometry'], 'result': 'PASS'},
        {'name': 'strict-web-debug-on', 'expected': 'Full target builds with both old local diagnostic exceptions removed', 'actual': 'Build exit 0; audit covers every compilation command', 'result': 'PASS'},
        {'name': 'strict-web-debug-off', 'expected': 'Build exit 0; no Debug UI/Playground library', 'actual': read(web / 'repair-off-artifacts.json'), 'result': 'PASS'},
        {'name': 'web-regressions', 'expected': 'All 13 fixture cases hold for ON/OFF', 'actual': {'on': on['results'], 'off': off['results']}, 'result': 'PASS'},
        {'name': 'native-regressions', 'expected': 'Rebuilt MSVC 19.51 /WX native runtime/semantic tests pass', 'actual': {'count': len(native_tests), 'failures': 0}, 'result': 'PASS'}],
    'diagnostic_reds': ['ui-width-red-result.json', 'clang-red.json', 'resource-red.log'],
    'artifacts': {'on': read(web / 'repair-on-artifacts.json'), 'off': read(web / 'repair-off-artifacts.json')},
    'native_host_artifact': native_artifact,
    'remaining_blockers': [], 'next_narrow_fix': 'Only if physical recheck finds a reproducible issue.',
    'physical_limits': 'Synthetic DOM capture stubs do not prove physical capture; new width observation is pending.',
    'claims_excluded': ['Gate PASS', 'PHYS PASS', 'independent Control Review', 'complete per-item manual acceptance']})
report = f'''# Web Debug UI width and strict Clang repair

Source/result revision: `{source}`. Evidence commit is the materialized ref returned in the task.
Branch `codex/p32-windows-minimal-touch`. Start revision `f4254a0d75c88267685831d4c234972014759b4c`.

The user reports the previous Web consumer's overall functionality is usable and identifies excessive
Debug UI width. This is preserved as a high-level manual observation, not individual matrix PASSes.
The new panel width still needs physical visual confirmation.

## Fixed causes

Web host CSS, backing canvas, provider and ImGui viewport previously used 900 px. They now share
410 px, matching the existing Windows panel width. CSS shrinks further to fit narrow stages. The
existing pointer coordinate conversion maps the displayed rectangle into that backing size. Common
DebugController, RuntimeFacade, Runtime ownership and frozen AutoIntent are unchanged.

StableOrderKey::value() used an int conditional bound in a size_t loop. The bound is now size_t,
with the same eight-byte cap. referencesResource had a dangling else attached to the RichText inner
condition; four stroke variants consequently failed to protect referenced resources from removal.
Explicit braces restore intended reference checks. No Authority, schema or operation policy is changed.

## RED and final results

| Check | Expected | Actual | Result |
|---|---|---|---|
| Old Web panel | At most 410 CSS px | 485.5 px at fixture's 501.5 px stage; backing 900 | FAIL (preserved RED) |
| Old stable-order Clang probe | Strict compile | sign-compare error | FAIL (preserved RED) |
| Old apply-plan Clang probe | Strict compile | dangling-else error | FAIL (preserved RED) |
| Old resource removal regression | Reject referenced resource removal for all six content branches | Four stroke branches wrongly prepared removal | FAIL (preserved RED) |
| Fixed panel | Bounded/fitting width and correct backing | 1200/800/360 stage widths -> 410/410/344 CSS px; backing 410 | PASS |
| Fixed strict probes | No local diagnostic exceptions | Both compile successfully | PASS |
| Full strict Web ON/full and OFF | Build without the two prior exceptions | Both exit 0; compile audit and dependency inventories retained | PASS |
| Browser regressions | 13 checks in each variant | ON 13/13; OFF 13/13, omitted UI asserted hidden | PASS |
| Rebuilt native regressions | /WX, resource prepare and Runtime/host regressions | {len(native_tests)}/{len(native_tests)} | PASS |

The resource regression tests referenced and unrelated removal for Image, RichText, VectorStroke,
DabStroke, BrushStroke and DabBrushStroke. It also checks the prepare path does not mutate the
source object store or manifest. Test data uses the existing internal store/prepare seam to isolate
reference scanning. Native test log/XML contains the exact case names and outcomes.

## Commands and provenance

```
python tools/setup_build_environment.py --semantic --target web-wasm32 --offline --facts-output out/p32-web-ui/web-environment.json --status
python out/p32-web-ui/diagnose_clang.py   # original source, expected RED; do not rerun on fixed source
cmd /d /c out\\p32-web-ui\\resource_red.cmd  # original source, expected RED
python out/p32-web-ui/verify_clang.py
cmd /d /c out\\p32-web-ui\\strict_build.cmd
cmd /d /c out\\p32-web-ui\\native_checks.cmd
cmd /d /c out\\p32-web-ui\\final_variant_build.cmd off
python out/p32-web/capture_variant.py off
node out/p32-web/run_browser_test.mjs ui-clang-off-final axiom_canvas_demo_off
cmd /d /c out\\p32-web-ui\\final_variant_build.cmd on
python out/p32-web/capture_variant.py on
node out/p32-web/run_browser_test.mjs ui-clang-on-final
```

Final Web builds explicitly set `CMAKE_CXX_FLAGS=-fexceptions` and empty `CMAKE_PROJECT_INCLUDE`,
with Debug ON/full and OFF/off in turn. The old forced include and per-source dangling-else override
are absent from every compile command (strict-compile-audit.json). Existing repository policies for
generated protobuf sources and SYSTEM SDK headers remain standard build policy, not these exceptions.
The RED apply-plan probe temporarily permits sign-compare only to expose the second original error;
neither exception is present in any final green probe or target build.

Actual commands/logs and strict compilation entries are retained here. Both variants embed source
revision above with dirty=0. Artifact JSON files give complete local paths and SHA-256 hashes.
Web browser tests are actual generated WASM in Chrome via CLI, DPR=2. Their capture stubs are
synthetic-input limitations; no physical touch/capture or monitor scanout acceptance is inferred.

Native regressions are rebuilt, not merely rerun old binaries, because the foundation header and
semantic implementation changed. MSVC version is recorded. Locked Semantic/Skia packages are reused;
no SDK dependency source is rebuilt. Axiom's own generated protocol sources are ordinary consumer code.
The real Windows D3D12 Minimal executable is also rebuilt from the committed revision; its embedded
revision, complete path and SHA-256 are checked in native-host-artifact.json. It was not launched this turn.

User machine and display facts retain their original capture timestamps. browser-launch.json records
current Chrome/server process paths, PIDs and executable hash; HTTP assets are verified against disk.
The correct page is `/axiom_canvas_demo/`; it is opened with the source revision query string.

Only the six source/test files listed in result.json and this new result directory change. Previous
FAIL/PENDING evidence, Authority, Golden, historical Gate, PHYS, Android and other checkouts are intact.
The earlier strict-build blocker is resolved by source fixes and a real unfiltered target build.
No independent Control Review, Gate PASS or PHYS PASS is claimed.
'''
(evidence / 'REPORT.md').write_text(report, encoding='utf-8')
shutil.copy2(Path(__file__), evidence / 'record_ui_clang.py')
print(f'Recorded strict ON/OFF, width and {len(native_tests)} native results for {source}.')
