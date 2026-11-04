# Web navigation stutter investigation

Result: **presentation stutter reproduced; render defect not fixed in this slice**.
Base: `391bb8a6699b23c5458d6b21bbd6b64afb55b7fb`, controlled worktree
`axiom-geometry-navigation/axiom`, branch `codex/geometry-navigation-integration`.
No user page reload, document mutation, SDK release or Gate acceptance.

## Observations

| Case / phase | Maximum RAF interval | Maximum draw-submission gap during input |
| --- | ---: | ---: |
| Default SDK, mixed scene wheel, repeat-0 | 21.3 ms | 267.0 ms |
| Default SDK, mixed scene + 16K-point strokes, cold touch | 37.4 ms | 520.1 ms |
| Same touch Runtime after warming, repeat-0 | 19.6 ms | 28.6 ms |
| Deterministic selected control, repeat-0 | 20.1 ms | 265.6 ms |
| Deterministic unselected control, repeat-1 | 20.5 ms | 267.4 ms |

The instrumented cold touch case had 53 incomplete-coverage opportunities and
52 without presentation among 73 navigation opportunities during input. A warm
repeat had no incomplete-coverage opportunities. The wheel control reproduces
the same behavior without a selected object. Full per-phase tails, checks and
counter attribution are in `analysis.json`; raw records and console rows remain
in each run directory.

The source-level mechanism is the full-viewport retained-image eligibility check
in `SkiaRenderer::renderNavigation`, followed by `SkiaNavigationTiles::render`
waiting for complete coverage before composition. Nonzero pan at unchanged
scale exposes an edge outside the old screenshot. New tile preparation is
cooperative, so input/RAF can proceed while presentation remains frozen. GPU
backpressure can extend preparation without blocking the input callback.

Both default-SDK 60-second foreground idle cases had zero additional RAF callbacks
and zero additional draws. Browser-reported JS heap decreased after collection.
No sustained idle loop or post-idle degradation was observed in these cases.
This does not rule out hours-long lifetime, ongoing-input history growth, physical
trackpad behavior, OS sleep/lock, context eviction, GPU memory or other apps.

## Evidence boundary

- `off/`: default installed consumer SDK, two cases, 60-second idle each, six
  repeated routes plus interrupted selection and lifecycle recovery.
- `on/`: existing instrumented SDK, same scenario classes, one-second idle;
  corroborates coverage/presentation counters, not an ON/OFF speed comparison.
- `selected-control/`, `unselected-control/`: same final fixture source and SDK,
  deterministic identities, wheel-only, zero idle; isolates selection dependency.
- `compatibility/`: existing cold/warm runner with corrected settlement on both
  selected/unselected long-line cases, four phases, default SDK. Measured cleanly.
- Each run records source before/after and full artifact SHA256. All were stable
  within the run. Initial runs predate the deterministic-ID/canonical-canvas-only
  collector refinement; their original source identities are preserved. Every
  case has one final pixel hash across its camera-return phases. No pixel parity
  is inferred across random-seed cases or differing selection chrome.
- The default installed SDK's every file matches the prior prepared-geometry
  evidence. Prior build-only objects/internal duplicate resources are correctly
  excluded from the consumer install; `analysis.json` lists them.
- `MEASURED` means evidence execution/invariants succeeded, **not** acceptable
  navigation latency. No physical input-to-photon or GPU execution time is claimed.

## Measurement repair and verification

The old harness could treat a selection completion during navigation input as
the new camera's completion and report a negative wait. A RED unit test observed
`-50` instead of unavailable. Browser waiting and summary now require a completion
after input ends. The root SDK's coarse onFrame count still does not identify a
camera pose; even a positive wait does not by itself qualify that pose.

Focused Node verification: 10/10 passed. The actual default root-SDK compatibility
run measured two cases/four phases without exceptions or changed semantic/selection
state. `git diff --check` passed. Production Runtime/render code is unchanged.

Full `verification/npm test` was attempted but blocked by missing `tsc`, exit 127.
Affected workspace runs: platform-conformance-cli, platform-harness-android,
platform-harness-apple, platform-harness-protocol, platform-harness-runner,
platform-harness-scripted-adapter, platform-harness-transport, platform-harness-web
and semantic-conformance-cli. Full-suite success is not claimed; the complete log
is `npm-test.log.gz`.

## Next implementation target

Prioritize bounded visible-frontier/coverage preparation and verify actual
submission continuity during cold pan, fast pan and eviction. Preserve complete
coverage correctness, source identity, GPU backpressure and qualified canonical
receipts. Increasing synchronous budgets or ignoring incomplete coverage is not
an evidence-backed fix. The current data does not establish a general memory leak.

Runner: `verification/runners/g5/navigation_lifecycle.mjs`. Reproduction and owner
analysis: `docs/planning/G5_NAVIGATION_LIFECYCLE_INVESTIGATION.md`. `analyze.py`
archives ignored `out/g5-navigation-lifecycle/` runs and rebuilds the artifact
manifest; it never modifies the measurements.
