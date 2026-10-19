# Web Minimal Runtime Host repair

Source/result revision: `b5ae620fffdd4fdcdd17cc2c15268f263e1e5c78`. Branch: `codex/p32-windows-minimal-touch`.
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

| ID | Check | Expected | Actual | Result |
|---|---|---|---|---|
| WEB-01 | DOM Down/Move/Up renders preview and canonical | Three DOM samples commit exactly one object; preview and canonical contain pixels; preview handoff retires. | Fixture assertions hold, ON/OFF | PASS |
| WEB-02 | ten strokes retain sampling and independent commits | Ten subsequent strokes create ten independent objects without loss or duplicate commits. | Fixture assertions hold, ON/OFF | PASS |
| WEB-03 | Facade Shape presets and return to Ink | Five creation presets (Rectangle/Ellipse/Triangle/Line/Arrow) commit; next Ink commits. | Fixture assertions hold, ON/OFF | PASS |
| WEB-04 | Facade Undo and Redo read back | Undo removes one object and Redo restores it; Facade receipt reads Applied. | Fixture assertions hold, ON/OFF | PASS |
| WEB-05 | cancel, identity reuse and stale generation rejection | Cancel creates no object; reused ID advances generation; stale generation cancel is rejected. | Fixture assertions hold, ON/OFF | PASS |
| WEB-06 | frozen AutoIntent: within 250 ms zoom; later MultiInk | Second contact at +100 ms zooms with no Ink; at +300 ms two contacts commit two Ink objects. | Fixture assertions hold, ON/OFF | PASS |
| WEB-07 | resize cancels active input and preserves canonical | Resize cancels the active contact, preserves objects; 501.5x347.5 at DPR 2 is exactly 1003x695 pixels; next Ink works. | Fixture assertions hold, ON/OFF | PASS |
| WEB-08 | Debug UI sibling uses Facade and cannot penetrate canvas | ON shows Debug UI pixels and UI pointer does not reach canvas; OFF hides the toggle and omits the debug library. | Fixture assertions hold, ON/OFF | PASS |
| WEB-09 | idle does not continuously submit canonical frames | After flush, four further animation frames do not increment canonical presented-frame count. | Fixture assertions hold, ON/OFF | PASS |
| WEB-10 | capture/focus loss cancel input and the next stroke works | Lost capture and blur cancel without commit; next complete stroke succeeds. | Fixture assertions hold, ON/OFF | PASS |
| WEB-11 | failed Down permits tool switch and next drawing | A fixture capture failure on Down leaves no persistent error; tool switch and next Ink work. | Fixture assertions hold, ON/OFF | PASS |
| WEB-12 | surface loss/rebind preserves canonical and permits drawing | Composition surface loss cancels input; rebind preserves canonical pixels and permits next Ink. | Fixture assertions hold, ON/OFF | PASS |
| WEB-13 | mouse/pen sources, coalesced history and tool controls use the same owner | Ink size/opacity, preset and Select return through Facade; mouse/pen strokes commit and coalesced pen history is retained. | Fixture assertions hold, ON/OFF | PASS |

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
cmd /d /c out\p32-web\repair_off_build.cmd
python out/p32-web/capture_variant.py off
node out/p32-web/run_browser_test.mjs repair-off-final axiom_canvas_demo_off
cmd /d /c out\p32-web\repair_on_build.cmd
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
