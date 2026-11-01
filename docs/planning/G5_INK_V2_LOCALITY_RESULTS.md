# G5 Ink v2 locality slice results

This report records the bounded locality slice after Ink v2 revision
`6c46f540c3fe5a3a38f4d0d5147e64f81a95dd0b`. It is not a G5 Gate or P34
decision and does not authorize the later Damage, RenderGroup, Tile, or
Scheduler work packages.

## Production changes

- `RuntimeScene` publishes a stable ObjectId lookup index alongside each
  projection. The index preserves the original record traversal order and is
  swapped together with the publication gate's stable image.
- Compact material rasterization computes finite device-space support before
  allocating coverage tiles. It skips empty tiles and clips material shading to
  the support rectangle. Perspective transforms conservatively retain the old
  full-clip behavior.
- `CompactRasterWork` is an internal diagnostic only. It is not a consumer API
  and does not change brush, semantic, or package formats.

## Correctness evidence

- Isolated native CTest: 16/16 passed, including lookup locality, compact
  coverage locality, projection observation, atomic publication, recovery,
  Ink v2 material modes, and Ink v2 runtime.
- Compact raster differential: 450 family/scale/path/transform cases passed
  exact pixel comparison against the independent full-clip implementation.
  Cases include rotation, skew, reflection, and perspective fallback.
- Short-stroke diagnostic: old path allocated 6 tiles and shaded 356,321
  pixels; localized path allocated 1 tile and shaded 2,240 pixels for the same
  800x500 target. The exact pixel output remained equal.
- Runtime boundary checker: 114 contract files passed. Verification workspace
  validator: valid. G5 Python preflight tests: 8/8 passed. G5 Node tests: 5/5
  passed.

## Actual remaining Scene locality finding

`prepareIncremental()` still calls the full projection path. An instrumented
ObjectStore observation measured one changed object as follows:

| Population | `allObjects()` calls | copied objects | classification |
| ---: | ---: | ---: | --- |
| 1,000 | 1 | 1,000 | `FULL_PROJECTION_FINDING` |
| 10,000 | 1 | 10,000 | `FULL_PROJECTION_FINDING` |

The production editor compiler also still returns
`kRequiresFullRebuild` for its delta path. This is intentionally reported as
the next Foundation/Scene dependency-closure task; it was not relabeled as an
incremental pass by this slice.

## Matching Web evidence

The stable Web measurement uses the same built root SDK consumer, Chrome CDP
native mouse input, ANGLE Metal on Apple M2, and the same `round-handdrawn`
workload. Its result is diagnostic browser evidence, not physical input-to-
photon latency or a product SLO. The measurement records canonical callback
latency by accumulated object count and preserves all input events, long tasks,
and two independent canvas targets.

The reference and localized runs are bound to separate source/artifact
manifests. A run is invalidated as `SOURCE_CHANGED` if any source-bound file
changes between configure, build, and browser execution.

## Boundary

The G5 v0.3 prerequisite audit remains
`BLOCKED_UNVERIFIED_PREREQUISITES`. No official G5, P34, or platform Gate PASS
is claimed. The full repository CTest exposes failures whose exact reference-baseline
comparison is recorded with the run,
including the G3 headless visibility expectation mismatch and legacy
playground assertions. The Python workspace suite retains four known authority
or baseline failures. Those results are preserved separately and are not
silently attributed to the locality changes.

## Full regression record (local verification)

The first complete local CTest invocation selected 230 tests: 206 passed and
24 did not pass. `InkV2NativeSession` was initially not run because its target
is excluded from the default build; after explicitly building that existing
target, its rerun passed (207 selected tests have passing observations). `FC02Text` was terminated after exceeding a four-minute
diagnostic budget while continuing slow font cases; this is interrupted evidence,
not proof of an infinite loop. The remaining failure names are preserved in
`out/g5-regression/build/Testing/Temporary/LastTestsFailed.log` and the CTest log.

Python discovery ran 193 tests: 187 passed, four failed, two skipped:
`G102RDescriptorRefreezeTest.test_every_descriptor_change_maps_to_a_closed_machine_projection_defect`
(missing `G45-C4-SR01`),
`G102RSemanticDifferentialTest.test_leaf_fixture_differential_compares_wire_and_semantic_projection`
(default probe artifact absent),
`ImportedFontCatalog.test_catalog_bytes_licenses_and_ids_are_exact` (13 vs 11), and
`SemanticPublicBoundaryTest.test_object_record_leaf_values_are_typed_and_not_opaque_blobs`
(old byte-vector structure assertion).

`npm run build` passed using the existing locked workspace dependency installation.
`npm run validate` passed its initial workspace tests and reached the semantic
conformance workspace; it failed
`C8 generator consumes the native observation envelope and emits exactly eight artifacts`
with `spawnSync git ENOBUFS`. The initial attempt before building failed because
workspace `dist` files were absent. Both attempts are retained; the temporary
`node_modules` symlink was removed after verification.

These whole-repository failures are not fixed by this bounded performance slice.
Reference baseline results must be checked before attributing any of them to the
five modified production files. No all-tests-green claim is made.

The unoptimized `6c46f540` baseline was configured and built independently
with the same Debug/locked release-Skia settings. Its initial complete 230-test
selection returned the same 24 non-passing test names. This establishes the
failure-set comparison, not successful verification of those failed features.
Both FC02Text runs were interrupted during slow font cases at unequal times;
they cannot be used for relative performance claims. The exact comparison is
`out/g5-comparison/regression-differential.json`.

## Stable Web comparison

Three before/after runs each were measured with matching source and artifact
bindings. Each run drew 32 eight-sample strokes plus one warmup stroke. At 33
actual objects, canonical callback delays were:

| | Repeat 1 | Repeat 2 | Repeat 3 | Median |
| --- | ---: | ---: | ---: | ---: |
| Reference | 331.2 ms | 347.9 ms | 380.8 ms | 347.9 ms |
| Localized | 331.8 ms | 331.9 ms | 464.0 ms | 331.9 ms |

Across all measured strokes, the median of the per-run callback p50 values was
197.6 ms before and 200.0 ms after. There is **no established browser latency
improvement** in this corpus. Different visible workloads, GPU compositing/fence
waits, and machine-load variance remain to be attributed. Some measurement
windows overlapped a separate integration build; source identity stayed stable,
but these wall times are diagnostic rather than a performance acceptance claim.

The permanent result bundle is
`verification/evidence/performance/g5-ink-v2-locality/`: comparison, raw browser
observations, source file hashes, SDK/resource/lock identities, regression
failure-set differential, component results, logs, and a SHA256 manifest.
Original run manifests describe their original run directories; `SHA256.json`
is the authoritative inventory of this bundled subset.

## Current visual-commit integration

During this work the primary worktree advanced to visual/material commit
`ff5b0739`. Its brush algorithm/resource changes were preserved. The same five
performance changes remain as the integration delta. A separate native component
run on that tree is `MEASURED`; its existing 16 locality/material/runtime tests
passed. Material v1/v2 exact pixel cases are verified separately from the fixed
`6c46f540` before/after timing corpus. No old baseline timing is relabeled as
`ff5b0739` evidence.

Material v1/v2 integration verification completed: 900 exact pixel cases passed,
and the latest 16-test CTest run passed. Integration G5 Python 8/8, Node 5/5,
and the 114-file Runtime boundary check also passed. The integration component
run selected revision 1 packages deliberately; revision 2 is covered here by the
material oracle and the existing Ink v2 visual/material regressions, not by a
new revision-2 end-to-end timing claim. Integration evidence is kept in
`ff5b0739-integration/` inside the result bundle.
