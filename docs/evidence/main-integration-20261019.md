# Main integration preflight, 2026-10-19

Integration is incomplete. Main has not been changed. This is a build/integration
record, not an FC-01 Gate, independent Control Review, PHYS or Android acceptance.

## Scope and source

- Repository: `Mostorm-Labs/axiom`.
- PR: [#148](https://github.com/Mostorm-Labs/axiom/pull/148), merge commit intended.
- Base main: `cc985b174cafc5d5dd743bd66701cfe117278de8`.
- Inspected head: `36e45bbb7e63c7822472c309e3d202bffd83d460` on
  `codex/p32-windows-minimal-touch`; main is its ancestor (0/395 commits).
- Worktree was clean before this preflight. The original checkout and its
  untracked files, including imgui.ini, were left untouched.
- The first complete PR CI snapshot for that head had 33 successful, 28 failed
  and 7 skipped checks. Several jobs report the same underlying failure. These
  failures remain recorded; the local checks below do not imply full CI closure.

## Narrow repairs in this commit

| Observed RED | Repair | Local result |
| --- | --- | --- |
| [Semantic consumer](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295678/job/117953756235): GCC /Werror-equivalent missing aggregate initializers and range-loop copies in FC-01 tests | Value-initialize Snapshot fixtures, set their intended fields, iterate tuple references | Target builds; 44/44 focused semantic/selection tests; strict Clang syntax check exits 0 |
| [Runtime discovery](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295790/job/117953756680): configuration fixture misses `src/sha256_internal.cpp` | Source-name extraction accepts digits | 11/11 CMake orchestration tests |
| [Semantic SDK consumer](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295689/job/117953952673): isolated validator targets cannot link `validateResourceManifestEntry` | Include production resource-manifest and SHA-256 source closure; keep tests independent of ObjectStore/mutation helpers | Three affected targets build and run in the focused 44-test set |
| [RF-01](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295707/job/117953756824): misleading indentation in reorder code | Split the statements into their actual control flow | Editor and Windows Host rebuild; selected-action regression passes |
| [Windows reference host](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295571/job/117953755420): signed/unsigned `GDI_ERROR` comparisons | Compare the int return value with an explicit int sentinel | Reference host builds |
| [Windows verification](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295624/job/117954239316): verification-only configuration unexpectedly requires ImGui | Only enable implicit Playground Debug UI when Playground is actually built | Verification configuration/build and 8/8 native contract tests |
| [RF-01 Windows](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295707/job/117953756520): no-Skia configuration unconditionally builds D3D12 provider | Gate provider/tests on enabled Skia consumer; resolve the published package in this consumer's CMake scope | No-Skia configuration excludes the provider; Skia Minimal Host builds; 5/5 Windows input/surface tests |

The first guard experiment used sibling imported-target visibility and removed
the intended Minimal Host target. The verification caught this; the final change
uses the enabled renderer options and explicitly consumes the locked package in
the Minimal Host directory. Both target membership and the real host build were
checked after that correction.

## Remaining integration failures

1. [Descriptor comparison](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295592/job/117953755821)
   compares current proto files against the historical frozen descriptor. The
   old SHA-256 is `8c1da1c19b754f9345472f96452e58505ebd18c0874b04480e378eb95a38654d`;
   the reproducibly generated current value is
   `ecf2265cde5f4698eaaadaf1d31b8fde417b2c57cd66923c531a1281780fd3b3`.
   The parsed delta adds resource-manifest operations/metadata and Snapshot
   resource/recovery fields. Existing RichText messages are unchanged. Neither
   the proto nor the historical descriptor was edited in this preflight. Current
   schema checking needs a separately scoped alignment that preserves the
   historical freeze and verifies the accepted extension rather than disabling
   the comparison or silently overwriting its baseline.
2. [Web reference browser test](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295693/job/117953756602)
   calls removed `host.present()`. Its expected legacy facts must be reconciled
   with the current Minimal Host `flush()`/Runtime frame observation contract.
3. [Playground Windows qualification](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295688/job/117953756006)
   still has rejected legacy stroke/resize/frame fixtures;
   [G4.7](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295653/job/117953757947)
   reports a scenario insertion failure. These are FAIL, not physical pending.
4. [Legacy Web build identity](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295585/job/117953755969)
   has Skia provider/renderer unresolved symbols in its Playground configuration.
5. [Android bridge build](https://github.com/Mostorm-Labs/axiom/actions/runs/39220295585/job/117953755544)
   uses an unqualified `sourceEpoch` in `submitBatch(AndroidHost& value, ...)`;
   the member is on `value`. Android source/status has not been changed here.
   A compile repair would still require a target build and would not constitute
   device acceptance.

The user was asked whether this integration may expand to the current-schema
check alignment and Android/legacy CI repairs, given the earlier restrictions.
No main merge or main-based FC-02 branch was performed while that answer was
pending. A separate uncommitted design draft was prepared under
`out/fc02-transition/fc02-richtext-runtime-boundaries.draft.md`; it assigns text
session/projection to Interaction, canonical text to Semantic, geometry to the
single TextLayout service and physical IME/focus to thin hosts.

## Reproduction and local verification

Use VS 2026 / MSVC 19.51.36260 x64 and the emitted environment from the locked
Semantic SDK resolver. Reuse the existing accepted Skia SDK and configured build;
no Semantic/Protobuf/Skia source build was performed. Diagnostic build outputs
are working-tree builds, not a sealed new release.

```powershell
python -m unittest verification.tests.test_semantic_cmake_contract
cmake --build out/fc01-closeout/build --parallel 4 --target canvas_semantic_fc01_shape2_apply_test canvas_semantic_payload_structure_test canvas_semantic_g1_04_c_geometry_overflow_source_test canvas_semantic_leaf_structure_validation_test axiom_canvas_demo_windows axiom_minimal_runtime_host_windows axiom_fc01_selected_actions_tests
ctest --test-dir out/fc01-closeout/build --output-on-failure -R '^(FC01Shape2|FC01Shape2Closeout|G1CGeometryOverflowSource|PayloadStructure|LeafStructure|FC01SelectedActions)'
python .deps/emsdk/upstream/emscripten/em++.py -std=c++20 -Wall -Wextra -Wpedantic -Werror -fsyntax-only -Iruntime/semantic/include -Iruntime/foundation/include -I.deps/googletest/googletest/include -I.deps/googletest/googletest runtime/semantic/tests/fc01_shape2_apply_test.cpp
cmake -S . -B out/fc02-transition/windows-contract -G Ninja -DCANVAS_BUILD_POC01=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release -DAXIOM_BUILD_VERIFICATION=ON -DCANVAS_BUILD_RF01=OFF
cmake --build out/fc02-transition/windows-contract --parallel 4
ctest --test-dir out/fc02-transition/windows-contract --output-on-failure
cmake -S . -B out/fc02-transition/rf01-no-skia -G Ninja -DCANVAS_BUILD_POC01=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release -DCANVAS_BUILD_RF01=ON -DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=OFF
cmake --build out/fc01-closeout/build --parallel 4 --target axiom_minimal_windows_touch_pipeline_tests axiom_minimal_windows_input_pipeline_tests axiom_minimal_windows_surface_binding_tests
ctest --test-dir out/fc01-closeout/build --output-on-failure -R '^FC00X(Windows|S2Windows|PreviewCanonicalCoordinates)'
```

Logs and the full descriptor delta are in the local `out/fc02-transition/`.
No new human touch/IME observation was made. Historical Gate/PHYS/Android records,
Authority, Golden and SDK locks were not modified by this preflight repair.
