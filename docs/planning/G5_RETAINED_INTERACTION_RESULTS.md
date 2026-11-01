# Retained ordered interaction results

This slice preserves the existing latest-input/per-object reuse implementation.
The extra failure was capacity: at 1000 sources a live replacement frame replayed
budget-denied compact coverage every tick. The fail-first native test reproduced
that failure with 1000 strokes and a two-entry object cache.

SkiaRenderCache now captures contiguous eligible compact runs from pixels already
produced by cooperative exact preparation. Each retained run keeps its original
traversal slot and full content/transform/order/clip/resource/target stamps. A
separate opaque prefix preserves exact background accumulation; transparent runs
preserve front/back composition and creation ink replay. All allocations use the
existing resource budget and a bounded run count/byte policy. Ineligible transforms,
erase masks and destination-sensitive legacy sources use the existing shared Skia
mapping. No brush/material, semantic, public SDK or dependency-lock changes.

Retained transparent composition is approximate feedback only: alpha rounding can
differ by a few channel values. Tests bound that difference (2 for front/back,
3 for creation), test exact opaque-prefix parity, and require byte-identical final
canonical output. No retained run qualifies CanonicalVisible. Runtime latest-input,
cancel/up and platform handoff ownership are unchanged.

Browser measurements: Chrome156 WebGL2/Ganesh, DPR2, fixed timestamps, 100/500/1000
round-handdrawn@4 strokes plus rectangle, text and PNG; synthetic real public SDK
input, not physical input-to-photon. Before and after final RGBA SHA256 match in
all three workloads. Machine load increased substantially during the after run;
these single samples are engineering observations, not latency SLOs.

| strokes | maximum live RAF callback before/after | exact terminal receipt before/after |
| --- | --- | --- |
| 100 | 3.4 / 5.0 ms | 30.1 / 27.5 ms |
| 500 | 8.3 / 14.8 ms | 24.0 / 34.6 ms |
| 1000 | 541.5 / 16.5 ms | 865.4 / 1015.0 ms |

The full after1000 callback maximum including terminal preparation is18.9ms.
The small-scene samples do not demonstrate a speedup. The1000 case demonstrates
elimination of repeated unchanged coverage during dragging; terminal full exact
preparation still costs about one second and remains separate work.

Evidence: verification/evidence/performance/g5-retained-interaction/summary.json,
raw before/after observations, matching artifact hashes, WebGL/SDK results and
native logs. Interactive matching demo: http://127.0.0.1:4196/example.html.

Scope limits: retained runs warm during cooperative exact preparation. Cold or
budget-denied targets, camera/resource/target changes, multiselection/editing that changes several sources
inside a run, complex masked/transformed content and noncompact-heavy documents
may still fall back. This is not universal1000-object60fps qualification. Further
GeometryChunk/local raster admission and canonical settle reduction remain G5 work.
Physical Android/Windows/iOS evidence remains BLOCKED_EVIDENCE; no P34 PASS.

Completed validation: native G5 34/34; actual Chrome WebGL private renderer probe;
public SDK52 checks, seven-family ink105 checks, navigation5 checks; Node8/8;
Runtime boundary116 files, C ABI manifest54 functions/49 structs/163 enums;
workspace and diff whitespace checks. Full root diagnostic219/246: the same27
failure names as the preceding mixed-scene slice, no added/removed failure names.
Their exact-main attribution remains incomplete. All names are retained in
verification/evidence/performance/g5-retained-interaction/root-failures.json; this
is not an all-repository green result.

1000-source live render-callback start gap improved from542.3ms maximum to18.0ms
maximum (p95 17.4ms after), complementing the callback work-duration measurement.
This measures owner update opportunities, not GPU completion or physical display.

Selection refinement: exact preparation excludes the selected source from retained
runs, warming unchanged front/back segments before a selected compact stroke
starts moving. A fail-first middle-stroke drag test verifies no background
coverage rebuild, original ordering and bounded approximate pixels. Selecting a
new source can require a cooperative warmup; it does not allocate unbounded
per-source layers.

Final refinement: changing selection preserves runs that do not contain the new
active source. An intermediate artifact invalidated every run on selection
change and regressed the first live frame; its raw final-1000 trace is retained
as a finding, not accepted performance evidence. The corrected artifact is
identified by accepted-1000 and accepted-webgl-probe observations.

Accepted final artifact1000 retest: maximum live callback28.4ms,
maximum live callback start gap31.9ms, maximum callback including
terminal preparation28.4ms; exact receipt1281.0ms.
Final RGBA SHA matches before. Final actual Chrome WebGL probe:PASS.

Verification provenance: the full52 SDK checks,105 ink checks,34 G5 tests and
246 root tests were run before the narrow final selection-invalidation
refinement. The final artifact was checked with native render-cache, actual
WebGL full renderer corpus, public handoff7/navigation5,1000 browser workload,
Runtime boundary/ABI/workspace checks. Raw artifacts identify each build.

Main integration: the implementation was committed as04088ce2 and integrated
with main@07126c4c in01fb9b39. Native G5 reran34/34; actual WebGL probe and full
public SDK browser checks passed on the combined source. Full root diagnostic
improved to225/246 with21 failures, all names already in the preceding27; six
previous failures no longer reproduce. Exact-main attribution of the remaining
failures is still incomplete. Integration logs and source-tree/artifact identity
are retained under verification/evidence/performance/g5-main-integration/.
