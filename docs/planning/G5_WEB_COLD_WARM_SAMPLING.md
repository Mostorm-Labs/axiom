# Web cold/warm navigation sampling

Repository: GitHub Mostorm-Labs/axiom. Execution anchor: `21270d3388dd9160ea1262791b901e11875e198f`.
User-authorized measurement continuation; consumes the existing G5 navigation,
timing and root Web SDK contracts. This is engineering evidence, not G5/P34 acceptance.

Owners: the root SDK collects DOM facts; CanvasEditingRuntime owns input,
selection, camera and exact settle; Render owns sources/tiles/selection chrome;
the Web provider owns WebGL submission and completion. The harness uses root SDK
exports only. No public API, drawing algorithm, cache policy or dependency change.

Execution closure:

- Continue through matching Release Web builds, browser execution, analysis and
  reviewable source/artifact-bound evidence, or report an explicit blocker.
- Add opt-in diagnostic counters for existing navigation work and selection
  overlay CPU time. Disabled builds evaluate no timing work.
- Four deterministic workloads: two 2048-input long strokes; the same strokes
  plus an imported 2048x1024 PNG, a text-bearing 3x3 table, or a text-bearing
  sticky. Brush is `round-handdrawn@4`. 800x500 logical viewport, DPR2.
- Separate fresh Runtime for each workload/selection/repeat. Cold means the
  first navigation sequence after construction; warm means the identical second
  sequence in that Runtime after exact settle. Driver/OS/browser shader caches
  are not claimed cold. Three repetitions per case.
- Wheel corpus: zoom in, reverse zoom, pan out, reverse pan; 72 RAF-paced inputs,
  net camera unchanged within floating-point tolerance. Inputs use the real DOM
  collector. Browser/RAF pacing is synthetic, not physical trackpad sampling.
- Record every input/RAF interval, visible-default-framebuffer submission gap,
  query/build/knot/prefix/fallback/cancel work, overlay CPU time, fence/poll call
  times and exact receipt delay. Never treat GL calls as input-to-photon or GPU
  execution. Capture pixels only outside timed windows.
- Assert unchanged document/selection/operation identity during navigation, no
  canonical receipt inside the input loop, eventual exact settle, no exceptions,
  and instrumented/default final-pixel parity on matched inputs.
- Reuse locked Semantic/Skia artifacts; preserve the existing interactive build.
  Record source before/build/after hashes, full SDK package hashes and browser/GPU
  identity. Source or artifact drift invalidates the measurement.
- Required verification: diagnostic accounting/disabled checks, metrics tests,
  relevant navigation/cache/chunk regressions, actual Chrome ON/OFF pixel check,
  Runtime boundary/workspace validation and whitespace checks.
- Terminal blockers: unavailable locked artifact/browser, source drift, invalid
  accounting, runtime exception, receipt/selection invariant failure or ON/OFF
  pixel mismatch. Slow results are observations, not execution blockers.

Evidence: `verification/evidence/performance/g5-web-cold-warm/`.
Raw temporary runs/builds: `out/g5-web-cold-warm/` and `out/web-sampling/`.
Local durable files suffice; no publication, release or old Gate edits are required.

Execution completed on 2026-11-04. Both diagnostic and default builds completed
three repetitions of all eight cases (96 cold/warm phases total), with 72/72
matched ON/OFF pixel checks. Results, raw observations and exact source/artifact
bindings are in `verification/evidence/performance/g5-web-cold-warm/README.md`.
