# GT-G4.6-C — Playground / Qualification / Workload Freeze

This is a qualification/materialization slice. It must consume accepted A/B behavior rather than redesign it.

## Fixed physical checklist
1. Chrome size/crispness at 50/100/200/400% and DPR/resize.
2. Mouse/touch/pen handle acquisition and hit-slop behavior.
3. Move/resize/rotate/cancel continuity; no commit jump; no stale overlay.
4. Slow/fast pan, wheel/pinch zoom, presets and fit.
5. Snap engage/release hysteresis and guide continuity.
6. Pen + Laser + editing overlay coexistence.

## Required downstream metrics
scene generation delta; camera/view generation delta; overlay update count; transient transform count; candidate count; canonical operation count; invalidation/damage footprint; frame p50/p95/p99; CPU render/query observations; deterministic replay digest where applicable.
