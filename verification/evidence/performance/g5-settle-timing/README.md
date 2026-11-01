# Exact settle timing diagnostic

This is measurement-only at source anchor 89cbd51e. Brush tuning in the other
worktree is excluded. No optimization, algorithm, cache policy, input contract,
semantic mutation, qualification or receipt behavior is changed.

Build with `-DAXIOM_ENABLE_FRAME_TIMING=ON` for the private diagnostic. Default is
OFF; no new JS/DLL command, export or getter is provided. One bounded numeric
stderr JSON row is emitted per canonical attempt. Normal packages must use OFF.
The profile counts wall elapsed time on the owner thread; a GPU command submission
is not GPU execution. CPU timers may include OS scheduling and incidental waits.

The timing tree is attempt → render → cache preparation → source drawing → CPU
coverage/material submission. `inclusive_ns` contains children; `exclusive_ns`
subtracts them. Add exclusive stages for attribution. Each row must satisfy
`sum(exclusive_ns) == attempt.inclusive_ns`. Repeated stage calls are accumulated
without heap-allocated trace events. Diagnostics emit after the attempt timer;
logging and clock probes still have observer cost, so compare matching OFF runs.

Stage mapping:
- visibility: existing Runtime Scene query.
- reference_build: complete ordered source construction, including entries,
  canonical byte serialization and digest (separately nested).
- presentation_adapt: current transient/label/container view adaptation.
- frame_plan: immutable FramePlan validation/copy.
- cache_begin: render cache validity, RenderGroup update (includes content
  equality), Damage diagnostics and cleanup.
- cache_prepare: preparation admission scan, cached-prefix lookup and builds;
  prepare_scan separately measures repeated knot support/admission scanning.
- cache_draw: bounds, recorded source lookup/build, scratch work, pixel snapshot.
- cpu_coverage: raster tile allocation/clear and coverage execution on CPU.
- material_submit: material shader construction/submission; not GPU execution.
- provider_present: provider flush/submit CPU call, not qualified receipt.
- navigation_snapshot: retaining exact output for later feedback.
- validation: current reference geometry checks, including the final draw check.
- attempt/render/scratch_draw exclusive remainder: work outside named children.

Counters identify repeated source entries, encoded bytes, copied plan entries,
preparation knot scans, built groups, CPU coverage tile count/pixels. Identity
fields are decimal strings. Rows distinguish exact-settle retries from accepted
submissions and rejected attempts. Browser runner separately records RAF,
canonical receipt, and WebGL fence insert/poll observations; pixel readback is
performed after timed interaction and exact completion.

Reproduce with matching SDK assets:

```sh
node verification/evidence/performance/g5-settle-timing/browser_runner.mjs out/run path/to/sdk/web 64 2
python3.12 verification/evidence/performance/g5-settle-timing/check_trace.py out/run/trace.log
python3.12 verification/evidence/performance/g5-settle-timing/summarize.py out/run
```

The runner sends fixed-timestamp synthetic pointer facts through the root SDK,
then 120 RAF-paced ctrl-wheel zoom events. This is not physical trackpad latency.
Use final RGBA SHA256 equality for ON/OFF output comparison. Wall-clock latency
comparisons require low-contention repeat runs and remain diagnostics, not SLOs.

Verification retained here: timing-enabled G5 27/27, unchanged baseline G5 25/25,
SDK Node 2/2, real WebGL root-SDK browser/handoff/Shell/Axis/example and navigation
regressions. `native-trace.log` uses a real cooperative settle, not fabricated
expected rows. `browser-red.log` shows the unchanged OFF path fails the missing
instrumentation oracle. The original 236-test suite was not rerun in this
measurement slice and its previously documented failures are not closed.
