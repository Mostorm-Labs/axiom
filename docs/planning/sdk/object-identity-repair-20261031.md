# SDK generated-object identity repair — 2026-10-31

This repair addresses F1 in the user-supplied SDK review, on
`codex/sdk-shell-axis-bridge` starting at
`dcc55e53dac51441e4e7e0ac142a8d58cd8fe075`. It is a source correction and
regression result, not a Gate or release-readiness decision.

## Cause and implementation

Composition already supplies fresh 128-bit entropy on every open, including the
candidate owner used for snapshot restoration. The earlier OperationId repair
used that origin, but pointer-created records still derived ObjectId from
instance-local numeric seeds. Restoring a rectangle reset the seed; independent
writers of one Document also started with the same seed. The canonical duplicate
identity rejection was correct.

The Interaction owner now allocates generated ObjectIds from one monotonic
counter combined with that existing origin. Objects and RichText identities
share this counter, with a domain distinct from OperationIds. Allocation skips
zero/live-object collisions and fails on counter exhaustion. IDs are injective
within an owner and collision-resistant across fresh owners/processes; this is
not an absolute distributed uniqueness guarantee.

- Shape, Connector and Sticky previews receive the allocated identity before
  commit. Their existing numeric ordering seeds remain separate.
- Brush records receive the allocated identity, and their inverse DeleteObjects
  operations reference that actual identity.
- Automatically generated connector targets use the same allocator.
- RichText objects, paragraphs and Sticky text children use the same allocator
  through the existing text identity port. Automatic image creation also uses
  this port.
- Existing snapshot records, remote/replay operations, caller-specified IDs and
  resource/package identities retain their values. Unbound standalone
  qualification and caller-specified legacy stroke ports keep their numeric
  fixture/compatibility contract.

No wire/schema, exported C ABI, dependency lock, validator, SDK transport or
callback rewriting change is involved. The old text fixture's nonzero-high-byte
assertion now checks the actual nonzero ObjectId contract.

## Correctness evidence

`SDKPublicObjectIdentity` exercises 18 GTest cases through public Composition,
Facade and real pointer contact APIs:

- Brush, Rectangle, Ellipse, Triangle, Line, Arrow and Sticky creation after
  restoring the same checkpoint into three fresh owners, including Undo/Redo.
- Two independent writers of one Document creating their first object, merging
  the original operations in both directions, and propagating Undo/Redo without
  local echo. Genuine duplicate ObjectIds still reject.
- A checkpoint containing the numeric rectangle identity observed in the
  released SDK: its existing record remains unchanged and a new rectangle
  commits after recovery.
- Automatic connector sibling targets, RichText objects/paragraphs and Sticky
  RichText children from independent writers.

`SDKPublicObjectIdentityProcessIsolation` launches three fresh processes. Each
process runs all seven pointer-tool merge cases and exports observed identities
in GTest XML. All 21 exported ObjectIds must be nonzero and distinct.

The original implementation failed the new pointer regressions. A controlled
re-enable of legacy numeric allocation also fails both new CTest entries,
including the legacy checkpoint regression; the production allocator was then
restored and rebuilt. Existing `SDKPublicOperationIdentity` also passes.

Final targeted verification passes all eight `SDKPublic*` CTest entries. The
longer `FC02Text` entry also passes (79 GTest cases, 215.6 seconds).

Validation uses macOS arm64, AppleClang 21, the locked Semantic v2 Store packages
and the locked R1 Full Skia release SDK. Semantic resolution succeeds offline.
Runtime C ABI manifest (54 functions / 49 structs / 163 enum constants), module
boundaries (104 files) and whitespace checks pass. CRLF source files are checked
with `git -c core.whitespace=cr-at-eol diff --check`.

```sh
cmake --build out/object-identity --target axiom_sdk_public_object_identity_tests \
  axiom_sdk_public_operation_identity_tests --parallel 4
ctest --test-dir out/object-identity -R '^SDKPublic' --output-on-failure
python3 tools/check_runtime_abi_manifest.py --root .
python3 tools/check_runtime_boundaries.py --root .
```

## Web SDK comparison

The same public-SDK probe was run in separate actual Chrome sessions against the
latest published JS SDK and the locally rebuilt wrapper/WASM/data set. SDK files
are copied outside the checkout; pointer input uses Chrome CDP mouse events,
without a replacement Runtime or pointer-capture mock. The browser profile is
headless Chrome 156/WebGL2 with SwiftShader, not physical-input/GPU qualification.

The published `axiom-sdk-preview-20261031-8e76ed25` archive is SHA-256
`672fc0d7effb7a25d42b2d95167f9aecb8d7e504f74aab577ba0b8f609fe3d25`.
Its build identity reports source `8e76ed252fdb8b8173451b14f1482c4c9568316d`,
dirty=false. It reproduces all three core findings: both first rectangles have
ObjectId `01000000000001000000000000000000`, restored pointer creation reports
`INPUT_REJECTED`, and the original Operation packets are rejected by remoteBatch.
After that rejected input, its restored owner also remains BUSY on close.

The repaired local build passes **all 10 checks**, including fresh ObjectIds,
post-snapshot pointer creation, remoteBatch merge, no echo, deduplication,
restored and collaborative Undo/Redo, and clean close. It reports base revision
`dcc55e53dac51441e4e7e0ac142a8d58cd8fe075` with dirty=true; it is a development
build containing the local repair, not an updated published preview. Asset and
source hashes and the observed IDs are retained in
[web-comparison.json](evidence/20261031-object-identity/web-comparison.json).

The existing `verification/sdk_js/verify_browser.mjs` also passes on this build:
52 general assertions, 20 Shell/Axis assertions, frame handoff/loss regressions,
and the shipped example's CDP mouse input and visible rectangle pixels.

```sh
node verification/sdk_js/verify_object_identity.mjs \
  out/object-identity-web/build/sdk/web /path/to/chrome repaired-probe.json
```

Local SDK files are in `out/object-identity-web/build/sdk/web`; raw observations
are in `out/object-identity-web/released-probe.json`, `repaired-probe.json` and
`general-browser.json`.

## Wider-suite limitations

The full configured native build is not green. Six unchanged test targets fail to compile
with the current macOS compiler:

| Test source | Compiler issue |
| --- | --- |
| `runtime/scene/tests/runtime_record_projection_test.cpp` | Missing `kSection` switch case |
| `runtime/render/tests/direct_reference_source_test.cpp` | Missing `kSection` switch cases |
| `runtime/render/tests/frame_orchestration_test.cpp` | Missing `kSection` switch cases |
| `runtime/composition/tests/fc05_connector_facade_test.cpp` | One `auto` declaration mixes `size_t` and `uint64_t` |
| `runtime/composition/tests/selection_input_integration_test.cpp` | Unsequenced `seq` modification/access |
| `verification/runtime_qualification/tests/g46c_qualification_runner.cpp` | Missing `recovery_frontier` initializer |

These also prevent dependent MSE/public-multi executables from being produced.
The full run therefore includes 35 entries that cannot execute.

The following 15 executed failures recur when legacy numeric allocation is
temporarily re-enabled, as recorded in
`out/object-identity/legacy-allocation-regression.xml`:

`canvas_fc02_shape_label_scene`, `canvas_g2_a3_full_incremental_equivalence`,
`G3HeadlessCanvasDemo`, `FC00XRuntimeComposition`, `FC01Creation-width-style`,
`G4InkPlaygroundComposition`, `G4InkPlaygroundSurface`,
`G4InkPlaygroundRepeatedStrokes`, `G4MultiPointerPlayground`,
`G4ViewportGestureState`, `G46BQueryStructure`, `G46PlaygroundTransform`,
`G47TextHost`, `G46CQualificationBehavior`, `G45BrushRuntimePipeline`.

The final configured suite contains **208 CTest entries: 158 pass, 15 fail and
35 cannot run**. The 15 failures are the same failures in the controlled legacy
allocation comparison; the new identity tests pass in the repaired build.
Full-run results are recorded in `out/object-identity/final-regression.xml` and
`out/object-identity/final-ctest.log`; build diagnostics are in
`out/object-identity/final-build.log`. The initial 60-second FC02Text run timed
out while still executing glyph/layout assertions, so the final run allows
300 seconds per test and FC02Text completes successfully. Targeted public-owner
results are in `out/object-identity/public-identity-regression.xml`.

Windows DLL rebuilding, physical input acceptance, D07/MSE closure and publication
are outside this local validation result.
