# Chalk/watercolor revision 3 local evidence

Source is the dirty descendant of 1229d0a1 on codex/chalk-watercolor-material-v3.
The user requested moving from sdk-shell-axis-bridge to this G5 baseline; scope
and closure are in docs/planning/ink-v2/chalk-watercolor-v3.md. Exact changed-source
hashes, artifact SHA256, full failure names and observations are in summary.json.
Historical pre-migration tests are not used as final G5 evidence.

Both profiles use evaluator6/signal3/material4 with only compact centerline and
quantized pressure persisted. Confirmed chunks and replaceable tail are simplified
with .25 world-unit and .019 pressure error bounds, shared by preview and seal.
Seed/resource/version are captured. Matched Chrome DPR1/2:105 checks each; actual
WebGL/Ganesh cache/Direct/resumed/navigation probe passes. New materials use G5
pooled target-compatible coverage, preserving hybrid legacy AA and frame timing.

Full Debug CTest:219/246.27 failures are explicitly listed in summary.json.22 match
prior baseline; the four extra direct UI failures reproduce on a clean1229d0a1
checkout with matching Debug/locked SDK settings; FC04.MSE-18 invokes two of them.
Storage extension initially exposed over-retained new centreline points, now fixed;
final full-suite storage passes. Crayon3 unsimplified carrier was also diagnosed,
but its old revision is deliberately unchanged and excluded from new-profile scope.

Native material tests cover fixed-width pressure deposition, clustered chalk pores,
continuous translucent wash, sample subdivision, seed, opacity once, taper, recorded
camera replay, ranges, live preview/seal, cache/rebind and Scene/FramePlan erase mask.
The final focused10 tests pass. Pixel swatch has old2 on left/new3 on right, chalk
then watercolor, using real BrushSession traces. Browser captures are synthetic
mouse input, not physical touch or user visual acceptance. Timings include CDP and
two RAFs; no frame-rate/Gate/Windows qualification claim. No SDK publication.
