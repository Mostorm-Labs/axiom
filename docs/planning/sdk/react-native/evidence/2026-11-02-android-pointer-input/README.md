# Android RN physical input follow-up — 2026-11-02

The connected device was a Google Pixel 7 running Android 17/API 37 with an
arm64-v8a ABI over USB. The Release consumer used bundled Hermes JS and the
Axiom package/native library built from `afa46640fc5f18ee36323d934f8e7516f2ec54bc`.
The Kotlin host fix is part of the pending source change in
`sdk/react-native/android/.../AxiomCanvasView.kt`; the staged APK and its exact
overlay hashes are recorded in `artifact.json`.

The initial physical run reported: “画不出来东西，没有preview也没有canonical”. A
real RN View/JNI/Runtime instrumentation RED run reproduced the issue with the
first Android pointer: `Android pointer ID 0 did not enter Runtime`. The shared
Runtime reserves pointer identity zero, while Android `MotionEvent` IDs are
zero-based. The host now maps each platform pointer ID to `id + 1`, preserving
identity across down/move/up/cancel and leaving Runtime ownership unchanged.

After rebuilding and reinstalling the corrected arm64 Release APK, the user
confirmed that drawing, preview and committed strokes were normal. The final
screen capture shows 11 objects, generation 13, zero active contacts and zoom
0.71. The Runtime log records physical contact transitions and committed
objects. The final result is a physical drawing/preview smoke record only.

The instrumentation GREEN run executes inside the external RN consumer and
passes through the actual Fabric View, JNI, Native Session, Runtime and GLES
surface path. It verifies pointer ID 0 admission, commit, Undo/Redo, Cancel and
contact retirement. It is synthetic input evidence, not physical touch evidence.
The repository Android pointer identity test and common SDK checks also pass.

Not established in this run: object or partial eraser behavior, full manual
lifecycle/detach matrix, native IME, stylus/real pressure, measured performance,
all-brush fidelity or Android platform qualification. Classification remains
`ANDROID_PHYSICAL_DRAWING_PREVIEW_SMOKE_RECORDED_RN_NOT_QUALIFIED`.
