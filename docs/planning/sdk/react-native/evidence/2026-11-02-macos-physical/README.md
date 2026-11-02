# macOS React Native physical smoke evidence — 2026-11-02

This directory records the macOS standalone Release app that the user physically tested. The app was built from `f4d179c3284022cca46d87dba348e086bdd8b207` and this evidence was organized after pulling its descendant `844ffda3c4a265f3a2bbb8b180a6575bb3355f90`, which contains the Windows evidence update. The evidence commit is a further descendant recorded by Git history. The macOS app was not rebuilt after the Windows commit; this distinction is intentional.

The tested app was `/Users/qing/Applications/Axiom RN Test.app`, an arm64 Release app using RN macOS 0.83.0 / RN 0.83.10 / React 19.2.0, bundled `main.jsbundle`, Skia/Metal, and separate canonical/preview providers. Port 8081 had no listener at launch. `physical-after-verification.png` and `accessibility-observation.txt` are the final capture. The visible state was 6 objects, generation 11, zero contacts, zoom 0.65, Partial erase inactive, Undo available.

The user confirmed the macOS trackpad-navigation follow-up passed. The final capture shows retained strokes, document/scene generation 11, no active contacts and zoom 0.65. This is a navigation smoke observation only; it does not reuse the earlier iOS checklist as macOS evidence. Native IME, stylus/real pressure, exhaustive brush material fidelity, measured performance, physical stale-generation rejection, per-operation receipts, and a complete resize/lost-context matrix remain unestablished. The classification is `MACOS_PHYSICAL_SMOKE_RECORDED_RN_NOT_QUALIFIED`; no Gate/P34/release PASS is issued.

## Implementation scope represented

- `9b7040b7`: Fabric `AxiomCanvasView` host.
- `03f0d0c3`: common run-loop frame timer and live preview retention.
- `403bdeaf`: immediate Runtime frame service for mouse samples.
- `39028aca`: preview layer above canonical layer.
- `f4d179c3`: AppKit magnify/scroll navigation through the existing Runtime viewport owner, with generation/thread/ABI validation and no semantic/history mutation.

The architecture remains AppKit event collection → Native C/POD admission → Native Session/Composition → `CanvasEditingRuntime::applyViewportNavigation` → `ViewportInteractionController` → shared canonical/preview frame service. No JS camera or second document owner was added.

## Reproduction and provenance

`consumer/` contains the exact key external-consumer sources used for the run; `native/native-manifest.json` and `package/package-manifest.json` contain the staged source/package hashes for f4d179c3. `logs/macos-release-build.log` ends with `** BUILD SUCCEEDED **`; `logs/scoped-ctest-fresh.log` records 10/10 scoped tests, `logs/controller-transport-corrected.log` records 12/12 Node tests, `logs/native-runtime-corrected.log` records the real Session/Runtime transport check, and `logs/release-package-fresh.log` records 27 Python package tests. `artifact.json` records installed signed app hashes separately because ad-hoc signing changes framework/app bytes. `manifest.json` hashes this evidence directory.

The generated consumer lock retained an old `file:../rn-macos-physical-1c8a/package` string. The actual tested Pods were regenerated from the f4d179c3 package/symlink, and the stale path must not be treated as the tested source.

## Diagnosis and reproduction notes

The earlier Debug attempts did not reliably compile the new host source: the
CocoaPods project still referred to a previous staged package. Regenerating Pods
from the consumer root corrected that source path. The final Release consumer
loads `Bundle.main/main.jsbundle` unconditionally and uses explicit dark status
text on white. The result therefore includes source-path correction and removal
of the Metro service dependency, rather than attributing the observed recovery
solely to layer z-order. `provenance-verification.json` records current Pods source
references and successful checks of all 60 native and 139 staged package entries.
The historical build directory suffix `3902` is not the tested source identity.

To reproduce, use a separate clean checkout/worktree at the exact tested revision,
consume the locked Semantic/Skia SDKs via `tools/release/build_native_sdk.py`, then
stage with `tools/release/native_package.py` at the same revision. These tools
refuse dirty/foreign package inputs; do not relabel a newer build as this run.
In an external RN macOS consumer, overlay the retained files according to
`consumer/source-layout.json`, retain the recorded RN dependency versions, and set
the file dependency, Metro watch folder and actual package symlink to that newly
staged package. The saved npm lock is original evidence with a stale local path;
refresh its local Axiom dependency before an install, then run `pod install` from
the consumer root. Confirm the generated Pods project references the same package
for `AxiomCanvasHost.mm` and `AxiomCanvasView.mm` as the framework/resources.

The final consumer build command (from the consumer directory) was:

```sh
xcodebuild -workspace macos/AxiomMacConsumer.xcworkspace \
  -scheme AxiomMacConsumer-macOS -configuration Release -sdk macosx \
  -derivedDataPath ../rn-macos-physical-3902/derived \
  CODE_SIGNING_ALLOWED=NO ARCHS=arm64 ONLY_ACTIVE_ARCH=YES -jobs 6 build
```

The resulting local App was ad-hoc signed and verified with
`codesign --verify --deep --strict`. Signed installed hashes are in `artifact.json`;
original staged hashes remain in their unmodified manifests. The key consumer
sources, project/scheme and Pod lock are retained, not generated Pods, node_modules,
credentials, or the large compiled App. All run/check outcomes are supplemental
to the explicitly scoped manual confirmation in `observation.json`.

`logs/node-invocation-note.txt` documents a verification invocation error and its
corrected runs. The existing full Debug rebind assertion and full text-suite timeout
remain historical limits noted in `design.md`; only the listed scoped tests were
executed here. The Windows evidence already pulled at `844ffda3` remains unchanged,
including its recorded partial/NOT_QUALIFIED result.

Copied Xcode/Pod logs have trailing whitespace normalized for `git diff --check`; `log-normalization.json` preserves their original local SHA-256 values and the retained hashes. Original consumer sources and staged package/native manifests are unchanged.
