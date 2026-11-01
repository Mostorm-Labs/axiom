# Exact settle stage timing slice

Repository: GitHub Mostorm-Labs/axiom. Anchor: 89cbd51e1930c8fbd95637614190d7de17aa975c.
User authorizes measurement before preparation-context reuse. This slice consumes
G5 v0.3 WP-11/18/20 and existing G5_PINCH_FEEDBACK_RESULTS, not formal Gate closure.

Owners: CanvasEditingRuntime owns canonical frame attempts; Render owns derived
source/cache/raster stages; provider owns acquire/present. Instrument these paths
without adding consumer APIs, changing draw algorithms, thresholds, input,
semantic mutation, cache admission, receipt rules, or dependency locks.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: opt-in compile-time diagnostic instrumentation, default off; one
  bounded numeric record per canonical attempt with full identity, outcome,
  counts and nested inclusive/exclusive monotonic elapsed stage durations.
- required: distinguish CPU work submission from GPU execution and qualified
  feedback; report frame preparation retries separately from final present.
- required: fail-first real navigation-settle observation test, disabled-build
  absence check, accounting validation, output parity, root SDK browser workload
  at 16/64 strokes and DPR1/2 plus direction reversal and artifact identities.
- required: default G5 component regression, timing-enabled native tests, actual
  WebGL/root SDK verification, module/workspace checks; preserve existing failure
  records without representing them as closed. No new formal platform PASS.
- evidence: raw rows, aggregation script, source/artifact SHA256, pre-existing
  build failures, staged timing percentages, caveats and recommendation under
  verification/evidence/performance/g5-settle-timing and this document.
- isolation: separate branch/worktree from in-progress brush tuning; do not alter
  the original dirty worktree or replace the interactive SDK artifacts.
- terminal success: runnable diagnostic build and reproducible attributed data;
  explain whether repeated source/plan work or raster dominates. Local reviewable
  artifacts are sufficient; publication is not required by this measurement task.
- terminal blockers: authority conflict, locked SDK unavailable, new regression,
  invalid accounting, instrumented/reference output mismatch or missing browser.
