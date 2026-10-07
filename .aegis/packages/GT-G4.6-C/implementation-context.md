# GT-G4.6-C P31 v0.2 - qualification/materialization

Original package: 68a6c4370efa589e58a33540b745d8c851ef711f.
Execution branch: codex/g4-6-c-qualification.
Implementation baseline: 7de87160aa5586d2cbf53a0fafeef1eea7d5b7d0.
A accepted implementation: 58868e060d3c0327bcff7823c924188cd808ee6b.
B accepted implementation: d9423fe90cfb6109fcb3914e69b4c27c01b646b4.
B later materialization changes only result/evidence files; retain its historical review metadata.

This revision repairs P31 binding completeness. Independent P31 Control Review remains pending;
it is not C implementation acceptance or an overall G4.6 verdict.

## Current owner flow
Windows has RuntimeFacade -> InkPlaygroundHost -> SelectionSession / TransformHandleDrag ->
TransformSession / TransientSceneOverride -> existing SetTransforms/history -> render.
Camera uses the existing ViewportInteractionController/FrameState owner.
Web/Android do not yet expose all editing entry points; C must wire existing owners and
report required unsupported capabilities, never create replacement interaction semantics.
Laser is owned by existing G4.5 programmable brush/Arc paths, not the ordinary brush catalog.
Inspect those real owners before composing Pen/Laser qualification.

## First incomplete action after package acceptance
Repository/package/evidence binding reconciliation, then ImplementationDesignPreflight for the
scenario selector, HUD/export, platform adapters and workload replay. No C production work
has been completed by this P31 materialization.

## Frozen physical checklist
1. Chrome size/crispness at 50/100/200/400% and DPR/resize.
2. Mouse/touch/pen handle acquisition with existing independent visual/hit geometry.
3. Move/resize/rotate/cancel continuity, capture, commit transition and stale overlay.
4. Slow/fast pan, wheel/pinch/anchor zoom, presets and fit document/selection.
5. Snap engage/release hysteresis and guide continuity.
6. Existing Pen/Laser coexistence with editing chrome on Windows/Web/Android.

Physical availability does not block producing the automated qualification bundle, but all
required observations remain blocking for C terminal success. Pending records are not passes.

## Downstream observations
Real scene/camera generation deltas, overlay updates, transient transforms, candidates,
canonical operations, invalidation/damage footprint, frame p50/p95/p99, CPU render/query
observations and deterministic replay digests where applicable. Preserve all 11 original
workload categories, including their 100K profiles. Do not implement G5 optimization.
