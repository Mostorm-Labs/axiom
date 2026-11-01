# Navigation scheduling observations

The preceding hybrid coverage slice is preserved and pushed as
04b3f56cc35811c177f3f11d700ed51861bc7025 on codex/g5-settle-timing.
This follow-up implements the user-authorized feedback/admission/prewarm work in
G5_NAVIGATION_SCHEDULING.md. Runtime owns camera/Scene queries, Render owns
navigation demand and disposable work, providers own contexts/present. There is
no new public SDK API or brush/semantic/CanonicalVisible authority.

Latest complete scene-current tiles, or an eligible retained canonical image,
are presented before cold Scene queries, setup and refinement. Retained images
must cover the whole requested viewport; cold zoom-out never stretches a cropped
screenshot over new content. Provider failure and stale generations fail closed.

The owner opportunity measures query/setup as well as raster work from entry.
Existing feedback admits one new query/setup; cold coverage can admit up to four
cheap cells within the same 2ms cooperative allowance. Individual queries,
validation, GPU compilation and 32-knot steps remain atomic and can overrun.
Counters expose overruns and query/setup costs; this is not a 2ms deadline.

At most two submitted refinement batches remain uncompleted. Skia completion
callbacks release capacity, using zero-timeout polling and asynchronous submit.
Camera composition bypasses this capacity limit. Capacity callbacks never produce
canonical/ARC receipts. Completion ownership survives disposal of navigation work.

Scale hysteresis avoids level churn near a boundary. Explicit idle grants allow
at most two neighbouring/finer cells per camera demand, after required coverage.
The existing Runtime grants this only between 40ms idle and the existing exact
transition at 100ms; it cannot delay exact handoff indefinitely. Active coverage
is protected from prefetch eviction; demand changes cancel obsolete work and
record discarded knot steps. This is bounded preparation during navigation,
not a general idle cache service after every document mutation.

## Measurements and remaining findings

Chrome WebGL2, ANGLE Metal Apple M2, DPR2; revision3 round-handdrawn, deterministic
16-sample strokes, 120 synthetic ctrl-wheel inputs at RAF cadence. These are CPU
callback and default-framebuffer GL submission observations, not physical display
or trackpad input-to-photon evidence. Final RGBA digests equal the preserved hybrid
baseline in all four cases.

| Corpus | Callback maximum before → after (ms) | RAF interval p95 before → after (ms) | First submission before → after (ms) | Steady submission max before → after (ms) |
| --- | --- | --- | --- | --- |
| 16 strokes, 800×500 | 21.3 → 15.7 | 24.6 → 19.5 | 37.3 → 18.0 | 37.2 → 17.6 |
| 64 strokes, 800×500 | 20.0 → 11.4 | 51.9 → 39.6 | 36.4 → 17.7 | 80.9 → 67.7 |
| 64 strokes, reverse, 400×250 | 49.4 → 16.1 | 45.4 → 22.7 | 129.0 → 179.7 | 129.4 → 36.5 |
| 16 strokes, cold zoom-out, 400×250 | 32.8 → 16.5 | 24.7 → 21.2 | 64.7 → 84.1 | 27.9 → 17.5 |

Classification is PASS_WITH_FINDINGS for host/browser scheduling implementation.
Cold first coverage still waits longer than the baseline: reducing admitted work
and GPU queue growth trades first response against callback cost. An intermediate
one-query cold policy waited ~300ms; allowing four cheap cold cells reduced it to
84–180ms. It does not eliminate that first-response gap. A separate 64-stroke run
observed an 83.4ms steady gap and 9.6ms callback peak, showing machine/driver variance
and an unresolved display cadence problem beyond callback CPU time.

Exact settle is not reliably improved by this slice: 16 strokes 781.1→781.4ms,
64 strokes 2679.9→2747.3ms in the paired batch; a later 64-stroke run observed
1347.8ms. Do not combine these into a speedup claim. Next work should prepare
complete coarse coverage before a cold gesture, and separately profile driver/
presentation cadence and exact-work total; never weaken coverage or receipts.

The reverse run cancelled one obsolete job containing 429 processed knots;
GPU throttles occurred while feedback continued. Two idle cells were prepared
where the idle grant had capacity. Strict suspended-GPU browser tests confirm
ongoing camera draws without canonical receipts and zero wait timeouts.

## Verification

G5 native suite: 27/27. Additional fail-first regressions cover feedback-before-
query, cold admission and provider-present failure. Native and real WebGL tests
cover long-stroke cancellation, lifecycle/stale targets, bounded queries/setup,
hysteresis/prewarm, GPU lag, Direct pixel equality and unchanged document state.
The private WebGL test yields the event loop between owner ticks; Asyncify is
restricted to that verification executable, not the consumer runtime.

Matching SDK browser acceptance, context loss/resize, frame handoff, Shell/Axis,
example, navigation cadence and Node facade checks pass. Baseline Node tests
6/6 and platform comparator 43/43 pass. Runtime boundaries and workspace checks
pass. Timing-disabled renderer/navigation objects compile with existing warnings
as errors. Evidence retains exact artifact hashes and source input identities.

The previous full-root diagnostic remains historical: default Release assertion-
only test build errors, and 20 known failures in the assertion-enabled diagnostic.
This slice does not repair G3 snapshot authority or claim a new full-root PASS.
Real Android/Windows/physical trackpad evidence remains BLOCKED_EVIDENCE; no P34
PASS is issued. The live independent demo is http://127.0.0.1:4191/example.html;
4190 and existing drawings remain preserved.
