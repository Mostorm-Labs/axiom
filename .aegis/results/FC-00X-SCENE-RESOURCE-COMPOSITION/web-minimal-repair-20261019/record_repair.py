"""Materialize additive Web Minimal repair evidence; never edit earlier results."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from datetime import datetime, timezone

root = Path.cwd()
out = root / 'out/p32-web'
destination = root / '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/web-minimal-repair-20261019'
destination.mkdir(exist_ok=True)
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
def write(name, value):
    (destination / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

files = '''repair-red-result.json repair-real-first-result.json
repair-pixel-diagnostic-4-result.json repair-layout-check-2-result.json
repair-final-precommit-result.json repair-on-final-result.json repair-off-final-result.json
repair-on-final-stderr.log repair-off-final-stderr.log
repair-on-artifacts.json repair-off-artifacts.json
repair-on-configure.log repair-off-configure.log repair-on-build.log repair-off-build.log
repair-on-link.txt repair-off-link.txt repair-on-bridge-compile.txt repair-off-bridge-compile.txt
repair-on-dependencies.txt repair-off-dependencies.txt
repair-off-query-configure.log repair-on-query-restore.log
native-final-command.json native-final-tests.log native-final-tests.xml
native-web-surface-build.log
repair-environment.json repair-sdk-offline.log configure-command.json
local-diagnostics.cmake unchanged-foundation-diagnostic.hpp
repair_on_build.cmd repair_off_build.cmd capture_variant.py run_browser_test.mjs
serve.py browser-launch-final.json http-artifact-verification.json tool-versions.txt'''.split()
for name in files:
    shutil.copy2(out / name, destination / name)
previous = destination.parent
for folder, names in {
    'windows-touch-20261019/facts': ['machine.json', 'displays.json', 'package-bindings.json'],
    'web-entry-reconciliation-20261019': ['reference-entry-audit-result.json', 'input-diagnostic-result.json'],
    'web-local-20261019': ['strict-build-FAIL.log', 'header-filter-build-FAIL.log'],
}.items():
    for name in names:
        # Baseline diagnostics are referenced if their durable names differ; do not rewrite history.
        candidates = [previous / folder / name, previous / folder / 'logs' / name, out / name]
        found = next((path for path in candidates if path.is_file()), None)
        if found:
            shutil.copy2(found, destination / name)
shutil.copy2(previous / 'web-local-20261019/seal_evidence.py', destination / 'seal_evidence.py')

source = read(out / 'repair-on-artifacts.json')['source_revision']
on = read(out / 'repair-on-final-result.json')
off = read(out / 'repair-off-final-result.json')
for result, enabled in [(on, True), (off, False)]:
    assert result['status'] == 'PASS' and len(result['results']) == 13
    state = result['observation']
    assert state['revision'] == source and state['dirty'] == 0 and state['debugUiEnabled'] == enabled
    assert state['activePointers'] == 0 and not state['pendingFrame'] and not state['failure']
changed = subprocess.check_output(['git', 'diff', '--name-only', '621388791a809ec19d721b5a67b14f3d6afa72aa', source], text=True).splitlines()
assert len(changed) == 6 and all(path.startswith('apps/axiom_canvas_demo/') for path in changed)

oracles = [
    'Three DOM samples commit exactly one object; preview and canonical contain pixels; preview handoff retires.',
    'Ten subsequent strokes create ten independent objects without loss or duplicate commits.',
    'Five creation presets (Rectangle/Ellipse/Triangle/Line/Arrow) commit; next Ink commits.',
    'Undo removes one object and Redo restores it; Facade receipt reads Applied.',
    'Cancel creates no object; reused ID advances generation; stale generation cancel is rejected.',
    'Second contact at +100 ms zooms with no Ink; at +300 ms two contacts commit two Ink objects.',
    'Resize cancels the active contact, preserves objects; 501.5x347.5 at DPR 2 is exactly 1003x695 pixels; next Ink works.',
    'ON shows Debug UI pixels and UI pointer does not reach canvas; OFF hides the toggle and omits the debug library.',
    'After flush, four further animation frames do not increment canonical presented-frame count.',
    'Lost capture and blur cancel without commit; next complete stroke succeeds.',
    'A fixture capture failure on Down leaves no persistent error; tool switch and next Ink work.',
    'Composition surface loss cancels input; rebind preserves canonical pixels and permits next Ink.',
    'Ink size/opacity, preset and Select return through Facade; mouse/pen strokes commit and coalesced pen history is retained.'
]
checks = []
for i, (a, b, expected) in enumerate(zip(on['results'], off['results'], oracles), 1):
    assert a['name'] == b['name'] and a['result'] == b['result'] == 'PASS'
    checks.append({'id': f'WEB-{i:02}', 'name': a['name'], 'expected': expected,
                   'actual': 'All named fixture assertions hold in both actual generated WASM variants.',
                   'result': 'PASS', 'evidence': ['repair-on-final-result.json', 'repair-off-final-result.json'],
                   'limit': 'Synthetic DOM/capture fixture; not physical touch evidence.'})
matrix = []
groups = {
 'A': [
  ('Single-finger continuous Ink', 'Preview follows the finger.'),
  ('Release Ink', 'Canonical and preview positions agree.'),
  ('Ten consecutive strokes', 'Sampling and visual continuity do not degrade.'),
  ('Rectangle/Ellipse/Triangle/Line/Arrow', 'Each selected primitive previews and commits.'),
  ('Ink after Shape', 'Ink stays above Shape.'),
  ('Repeated Arrow/Line/Shape', 'Each consecutive action commits independently.'),
  ('Partial eraser', 'Only Ink is erased; Shapes remain unchanged.'),
  ('Undo/Redo/selection/move/scale/alignment/cancel', 'Each enabled action has the expected visible result.')],
 'B': [
  ('Two-finger pan', 'Within the frozen Pending window, navigation pans without Ink.'),
  ('Two-finger zoom', 'Second finger joins Pending within 250 ms; zoom without Ink.'),
  ('Slow second contact', 'Within 250 ms navigation; after 250 ms second Ink, according to existing AutoIntent/slop rules.'),
  ('Fast second contact', 'Both Down events reach Runtime.'),
  ('One finger lifted', 'Remaining contact follows the frozen navigation/MultiInk continuation policy.'),
  ('Navigation capture/focus/window loss', 'All affected sessions cancel without residual pointer state.'),
  ('Ink after navigation', 'A new single-finger stroke previews and commits.'),
  ('Frozen MultiInk', 'Late second contact (>250 ms) writes a second Ink stroke.'),
  ('Identity reuse', 'Old source/pointer/generation events cannot affect the new gesture.'),
  ('Touch Debug UI', 'UI touch operates controls without canvas penetration.')],
 'C': [
  ('Ink / Shape tools', 'Enabled tool controls apply and read back through RuntimeFacade.'),
  ('Brush / Marker presets', 'Enabled presets apply and read back through RuntimeFacade.'),
  ('Color / width / opacity', 'Enabled options apply and read back through RuntimeFacade.'),
  ('Undo / Redo', 'Controls operate through RuntimeFacade.'),
  ('Eraser modes', 'Enabled modes apply through RuntimeFacade.'),
  ('Selection / move / scale', 'Enabled operations apply through RuntimeFacade.'),
  ('Viewport status', 'Facade state agrees with observed viewport.'),
  ('Frame / retry status', 'Enabled diagnostics show current owner state.'),
  ('Provider / resource / lifecycle', 'Enabled diagnostics show current owner state; unavailable sections remain unavailable.'),
  ('Debug UI toggle', 'Button/F12 toggles the sibling UI; drawing still works after closing it.')],
 'D': [
  ('Resize', 'Drawing works at several window dimensions.'),
  ('Maximize / restore / minimize', 'Canonical persists and preview/drawing recover.'),
  ('Multiple monitors / DPI', 'Pointer, preview and canonical coordinates stay aligned.'),
  ('Preview / canonical offset', 'No positional handoff offset.'),
  ('Idle', 'No unexpected sustained redraw.'),
  ('Capture loss recovery', 'Next stroke succeeds.'),
  ('Brush handoff retirement', 'Committed handoff overlay retires.'),
  ('WebGL loss / hidden / unavailable surface', 'Retry/rebind preserves canonical and does not fabricate completion.')]
}
for group, values in groups.items():
    for index, (name, expected) in enumerate(values, 1):
        matrix.append({'id': f'{group}{index}', 'name': name, 'expected': expected,
                       'actual': 'Not manually observed on the repaired Web consumer.', 'result': 'PHYSICAL_PENDING'})
write('manual-matrix.json', matrix)
result = {
 'recorded_at': datetime.now(timezone.utc).isoformat(),
 'status': 'AUTOMATED_FUNCTIONAL_CHECKS_PASS_PHYSICAL_PENDING',
 'result_revision': source,
 'materialized_ref': 'The evidence commit containing this result; resolve with git log -1 --format=%H -- ' + str(destination.relative_to(root)).replace('\\', '/'),
 'branch': 'codex/p32-windows-minimal-touch',
 'upstream_baseline': 'f1d72f22ad20209c8ccdbdd7474d86c651434914',
 'start_revision': '621388791a809ec19d721b5a67b14f3d6afa72aa',
 'changed_source_files': changed,
 'consumer': {'target': 'axiom_canvas_demo_web', 'owner': 'WebSurfaceHost -> RuntimeComposition',
              'url': 'http://127.0.0.1:8765/axiom_canvas_demo/?revision=' + source,
              'debug_ui': 'ON/full, shared DebugController borrowing composition-owned RuntimeFacade',
              'playground_core_linked': False},
 'checks': checks,
 'native': {'result': 'PASS', 'count': 49, 'evidence': 'native-final-tests.xml',
            'scope': 'Existing native Runtime/Windows regression binaries plus G3WebSurfaceHost; Runtime/native sources unchanged.'},
 'on_final_state': {key: value for key, value in on['observation'].items() if key != 'trace'},
 'off_final_state': {key: value for key, value in off['observation'].items() if key != 'trace'},
 'artifacts': {'on': read(out / 'repair-on-artifacts.json'), 'off': read(out / 'repair-off-artifacts.json')},
 'physical_matrix': 'manual-matrix.json',
 'remaining_blockers': [
  'Repaired Web consumer has not been physically observed; all Web manual items remain PHYSICAL_PENDING.',
  'Unfiltered strict Emscripten build remains FAIL on two unchanged baseline diagnostics; functional builds use the exact local exceptions recorded here.'
 ],
 'next_narrow_fix': 'Only if physical testing finds a reproducible defect, or separately authorized baseline strict-diagnostic cleanup.',
 'claims_excluded': ['Gate PASS', 'PHYS PASS', 'independent Control Review', 'monitor scanout observation', 'rotation support']
}
write('result.json', result)
rows = ['| ID | Check | Expected | Actual | Result |', '|---|---|---|---|---|']
for check in checks:
    rows.append(f"| {check['id']} | {check['name']} | {check['expected']} | Fixture assertions hold, ON/OFF | PASS |")
report = f'''# Web Minimal Runtime Host repair

Source/result revision: `{source}`. Branch: `codex/p32-windows-minimal-touch`.
The commit containing this additive evidence directory is the materialized ref.

The requested consumer is `axiom_canvas_demo_web`, owned by `WebSurfaceHost -> RuntimeComposition`.
Its optional Debug UI borrows the same composition-owned RuntimeFacade. The old `/ink_playground/`
entry is not this consumer. Both ON/OFF link inventories exclude its core library.

The exact six changed source/test files are in result.json. Authority, Golden, historical result
directories, PHYS, Android, Windows code and frozen Runtime gesture policy were not modified.

## Root causes and repair

- The former page passed Number timestamps to the uint64 WASM boundary: three BigInt TypeErrors,
  zero samples. The bridge now uses BigInt timestamp/generation values and full pointer keys.
- The reference host had no Minimal workflow controls or qualified completion loop. It now binds
  one canonical/preview provider pair, obtains exact composition work tokens, and completes them
  after browser yield plus WebGL fence completion. It keeps retry demand and does not redraw idle.
- An intermediate RED exposed a startup status/header size change cancelling the first Down.
  Fixed layout and readiness prevent that cancellation. Another RED exposed queued receipt readback;
  the bridge now reads the request's actual Facade receipt. These FAIL records remain in this folder.
- PointerEvents retain provided history, pressure and contact facts; one client-to-view mapping.
  Source classes are browser mouse/pen/touch classes, not fabricated physical device identifiers.
  No mouse listener duplicates promoted touch. Runtime keeps gesture recognition and generations.
- Capture/focus/resize/surface loss and tool changes cancel owner input. Failed Down remains recoverable.
  Shared DebugController controls use RuntimeFacade; Debug UI owns its pointer sequences.

Frozen AutoIntent is unchanged: second contact within the 250 ms Pending window navigates;
after the window it is second Ink/MultiInk. The existing Ink slop rule is retained. No rotation claim.

## Executed automated checks

{chr(10).join(rows)}

Both variants contain the exact source revision above and dirty=0. The ON run and OFF run each
passed all 13 fixture checks. OFF checks hidden/omitted UI rather than claiming UI interaction.
The original RED and intermediate FAILs are historical outcomes; they are not rewritten as pending.

`repair-*-final-result.json` preserves DOM input ordering, source/pointer/generation, timestamps,
view positions and the final composition/Facade state, object/operation/frame/retry/rejection counters.
Assertions and event sequences are in the committed HTML fixture. These are synthetic DOM tests:
capture is stubbed because untrusted events cannot acquire physical capture. Composition loss/rebind
does not prove actual browser WebGL context restoration. All such physical cases remain pending.

Fresh native CTest: 49/49 PASS (`native-final-tests.xml`). These reuse built native regressions;
all native Runtime/Windows production and test sources are unchanged by this Web repair.

## Reproduction and build limits

Run from the recorded isolated worktree. Locked SDK resolution was verified offline for web-wasm32;
repair-environment.json contains actual AXIOM_PROTOC, AXIOM_SEMANTIC_RUNTIME_ROOT and CMAKE_PREFIX_PATH.
configure-command.json contains the full initial configuration; the .cmd scripts perform final
ON/OFF configuration and target builds. Emscripten/Clang versions are in tool-versions.txt.
No Semantic, protobuf or Skia SDK was rebuilt.

```
python tools/setup_build_environment.py --semantic --target web-wasm32 --offline --facts-output out/p32-web/repair-environment.json --status
cmd /d /c out\\p32-web\\repair_off_build.cmd
python out/p32-web/capture_variant.py off
node out/p32-web/run_browser_test.mjs repair-off-final axiom_canvas_demo_off
cmd /d /c out\\p32-web\\repair_on_build.cmd
python out/p32-web/capture_variant.py on
node out/p32-web/run_browser_test.mjs repair-on-final
```

Native command: native-final-command.json. Browser tests use real animation-frame/browser yields,
not a virtual-time dump-DOM timeout. Run with Node 24+ and local server on port 8765.
The local helper scripts are copied here; their recorded commands expect them under out/p32-web.

Unfiltered strict baseline build remains **FAIL**: unchanged stable_order_key.hpp sign-compare and
apply_plan.cpp dangling-else. The local forced-include header limits the first exception to that header;
the local CMake include limits -Wno-error=dangling-else to that one source. Original raw failures are
preserved here or in the prior web-local evidence. New bridge code retains -Wall -Wextra -Wpedantic
-Werror; upstream SDK headers are SYSTEM. This is a functional consumer result with recorded baseline
exceptions, not an unfiltered strict-build PASS.

WebGL fence feedback proves backend execution after a browser yield. It does not measure physical
monitor scanout, and no display timestamp is invented. See the
[WebGL 2 specification](https://registry.khronos.org/webgl/specs/latest/2.0/) and
[browser rendering loop](https://html.spec.whatwg.org/multipage/webappapis.html).

## Device, artifacts and remaining manual work

Device facts and DPI/display captures are copied with their original same-day capture timestamps.
Browser-launch-final.json records current Chrome/server executable paths, PIDs, hashes and exact URL.
Artifact paths/hashes for both variants are in repair-on-artifacts.json and repair-off-artifacts.json.
The page was opened in the existing visible Chrome session by CLI; no computer-use API was used.
A reused browser process does not identify a tab renderer PID or prove physical interaction.

Use the page's Debug UI button (or F12 if the browser delivers it). Verify physical drawing on this
correct entry. Every A/B/C/D item is listed with expected/actual/PHYSICAL_PENDING in manual-matrix.json.
The user's earlier Windows success report does not establish repaired Web acceptance.

No independent Control Review, Gate PASS, PHYS PASS or complete manual acceptance is claimed.
'''
(destination / 'REPORT.md').write_text(report, encoding='utf-8')
shutil.copy2(Path(__file__), destination / 'record_repair.py')
print(f'Materialized {len(list(destination.iterdir()))} evidence files for {source}.')
