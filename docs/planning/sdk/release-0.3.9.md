# SDK 0.3.9 release

The user authorized main's fast-forward to the completed G5 integration and a
new numeric SDK release. Main retains merge `21270d3388dd9160ea1262791b901e11875e198f`,
its geometry-navigation history and the coincident-pinch correction. This patch
continues the 0.3.x version line with identical Web/API/RN/platform versions.

The v0.3.8 candidate at `b7e513ced64a31c3e0c05a000b6103460d120892` exposed an
Android NDK strict-compiler failure: the optional render-owned VisibilityResult
chunkSelections vector lacked an explicit default member initializer. Apple
Clang accepted the aggregate factory, while Android Clang rejected its omitted
field under -Werror. The fix explicitly initializes that vector to the same empty
value; it changes no layout, SDK interface, ABI version, Operation or Snapshot.
Warnings were not disabled and no dependency SDK was rebuilt.

Both v0.3.8 workflows were cancelled before release completion, and its tag and
exact failure evidence were retained. Failure record:
`verification/evidence/releases/0.3.8/android-compile-failure.json`.
The exact installed NDK 27.2.12479018 compiler reproduces RED with the v0.3.8
header, then compiles the production source GREEN with this one-member fix;
logs and command scope are under out/release-0.3.9-android-compile.

Local checks cover 17 focused native geometry, chunk-range, navigation, material,
visibility, selection, image, text and gesture cases, plus the three directly
affected native cases; 37 release-package tests; 15 workflow/server tests; strict
consumer TypeScript; Runtime boundaries; and matching Chrome WASM consumer and
navigation scenarios. The accepted Semantic SDK resolves from the persistent
Store, and the existing Skia/Emscripten/build directories are reused.

The existing tag-triggered hosted gates must build Web/Windows SDKs and demos,
publish GitHub Release and Web npm, build all six RN native targets, and publish
all five RN npm packages from the exact same source/version. Physical device,
input-to-photon and formal Gate qualification remain separate evidence. Known
allocation probes and unrelated legacy failures are not claimed fixed here.
