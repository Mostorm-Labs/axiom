# G5 navigation continuity repair

Code revision: `f959131de926065427348980ab8827f33b407b1e` on
`codex/geometry-navigation-integration`. This is local engineering evidence,
not a Gate/P34 acceptance or a physical trackpad qualification.

The additional Runtime repair releases disposable coverage after the existing
100ms navigation quiet interval, even if coarse coverage is incomplete. This
allows cooperative canonical preparation to converge without waiting on coarse
cache misses. Claimed touch gestures and sustained input still use feedback.
The dedicated quiet-settle RED/GREEN test checks both sides of that interval.
`intermediate-27f4a3fc/` preserves the earlier matrix: it improved active/idle pan
but exposed a 714–860ms post-freeze coverage wait, which prompted this repair.

The existing Runtime owns camera, document and receipts. Render now prepares the
nearest complete coarse cover in at most four world cells before fine tiles,
preempts fine work when coverage is missing, budgets long v4 geometry from actual
compiled contour counts, and checkpoints prepared draws at 32 primitives. Fine
GPU path submissions have a separate 128-primitive opportunity limit, in addition
to the existing CPU time/step limits and two in-flight GPU batches. Coarse v4
coverage masks use a leased CPU Skia surface and an implicit GPU upload; material
composition and presentation stay on the GPU. This does not claim zero-copy.

## Correctness and measurement boundaries

- `source-identity.json`, `build-facts.json` and `consumer-install.json` bind the
  candidate source and both matching consumer SDK packages. The SDK public API,
  semantic formats, accepted dependency locks and old Gate records are unchanged.
- The baseline is the installed, previously source-bound prepared-geometry SDK.
  `baseline-artifact-binding.json` verifies its complete installed file hashes
  against the earlier evidence. Its engine source differs from the sampler
  checkout recorded in lifecycle `sourceBefore`/`sourceAfter`.
- Both engines use the same corrected observer. It counts default-framebuffer
  color clear/blit submissions as well as draws. Historical `draws` fields now
  mean color submissions. Earlier draw-only gap numbers are not used as a
  directly comparable baseline. GL calls do not measure GPU execution, physical
  display completion or input-to-photon latency.
- Canonical completion is an `onFrame` after input ends. It is a coarse completion
  and does not expose a camera-qualified identity. A selection completion during
  input cannot qualify navigation settlement.
- The fixture uses the root SDK, six 4096-point v4 long strokes, image, table,
  sticky, DPR 2, 636x914 logical viewport, deterministic IDs, repeated wheel
  pan, foreground idle and CDP frozen/active recovery. It preserves all phases,
  tails, pixels and semantic/selection/operation/camera checks. It does not model
  hours of idle, OS sleep, physical wheel delivery or GPU eviction.

## Regression accounting

`checks.json` and `regression-comparison.json` contain the exact failures.
Targeted native tests pass 3/3, focused Node tests pass 14/14, and the Runtime
boundary checker passes 117 files. Full CTest passes 999/1066: 67 failed/not-run
names match the previously reproduced baseline; five existing semantic/qualification
compile targets still fail. Full `npm test` reaches all nine workspaces but is
blocked by missing `tsc` (exit 127). These runs are not represented as full PASS.

`gpu-continuity-final.json` passes the real Chrome WebGL2 primitive quota,
path/matrix/paint stream and strict byte-equal local/full contour checks for
round-handdrawn/marker v4. `gpu-full.json` fails the existing v3 denied-geometry
fallback pixel oracle. `parent-fallback.json` and its source/provenance reproduce
the identical failure using unchanged parent `51f93640` production code.

The old GPU runner incorrectly accepted native errors when process exit was zero;
`gpu_probe_status` now requires explicit native completion, code zero and no
native/browser errors. Old archived evidence is retained unchanged. The contour
oracle also isolates targets: two identical full reference streams interleaved
before readback differ at one AA pixel on this shared Ganesh context
(`gpu-interleaved.json`), while sequential references and exact drawing-stream
comparison agree. Readback synchronization is confined to tests; no blocking waits
were introduced into production. The failed diagnostic is not treated as PASS.

Raw RED logs cover coarse-first progress, long admission and primitive checkpoints.
The main execution contract is
[`G5_NAVIGATION_CONTINUITY_REPAIR.md`](../../../../docs/planning/G5_NAVIGATION_CONTINUITY_REPAIR.md).

## Browser results

`comparison.json` binds all five final/baseline cases and the exact raw observations.
Selected repeated-pan maximum submission gaps have median 267.2 -> 117.7ms
(56% lower); unselected 331.9 -> 86.2ms (74% lower). Selected foreground idle
uses 120s on the candidate versus 60s baseline: gap 200.1 -> 86.7ms and coarse
post-input completion 198.6 -> 114.3ms. After CDP thaw, selected completion is
230.7 -> 112.2ms, and unselected 230.7 -> 129.4ms. The intermediate 714–860ms
coarse wait is resolved. Timing-ON observes at most 128 fine GPU primitives per
opportunity and one exact attempt after thaw.

All final pixels match baseline and ON/OFF within the same selection state.
Document, selection, operation and camera checks pass; fence timeouts are zero.
The final matrix is `PASS_WITH_FINDINGS`: a roughly 200ms initial submission gap
still occurs after CDP thaw, and one timing-OFF interrupted-selection phase has
a 117ms long task /130.8ms RAF interval (ON cold peak38.2ms). These tails are
retained, not averaged away. This is a material improvement in the reproduced
corpus, not a claim that every navigation frame is smooth. Main timing comes
from timing-OFF packages; timing-ON corroborates owner work counters.
