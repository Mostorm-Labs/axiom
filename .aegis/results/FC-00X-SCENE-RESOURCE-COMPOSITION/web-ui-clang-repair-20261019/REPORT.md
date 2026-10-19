# Web Debug UI width and strict Clang repair

Source/result revision: `e42c907f827e33864d80a47ba7f9d5938ba14e3a`. Evidence commit is the materialized ref returned in the task.
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
| Rebuilt native regressions | /WX, resource prepare and Runtime/host regressions | 79/79 | PASS |

The resource regression tests referenced and unrelated removal for Image, RichText, VectorStroke,
DabStroke, BrushStroke and DabBrushStroke. It also checks the prepare path does not mutate the
source object store or manifest. Test data uses the existing internal store/prepare seam to isolate
reference scanning. Native test log/XML contains the exact case names and outcomes.

## Commands and provenance

```
python tools/setup_build_environment.py --semantic --target web-wasm32 --offline --facts-output out/p32-web-ui/web-environment.json --status
python out/p32-web-ui/diagnose_clang.py   # original source, expected RED; do not rerun on fixed source
cmd /d /c out\p32-web-ui\resource_red.cmd  # original source, expected RED
python out/p32-web-ui/verify_clang.py
cmd /d /c out\p32-web-ui\strict_build.cmd
cmd /d /c out\p32-web-ui\native_checks.cmd
cmd /d /c out\p32-web-ui\final_variant_build.cmd off
python out/p32-web/capture_variant.py off
node out/p32-web/run_browser_test.mjs ui-clang-off-final axiom_canvas_demo_off
cmd /d /c out\p32-web-ui\final_variant_build.cmd on
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
