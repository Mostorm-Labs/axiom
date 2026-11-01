# Render Damage / compact RasterCache results

Implemented on `64be2cc86da72e535bb7ae38a4ca9d45dfcefc52` plus the
preserved Scene-locality worktree delta. Exact final source file hashes are
in `verification/evidence/performance/g5-render-cache/source-final.json`.
Classification: `PASS_WITH_FINDINGS`; no G5/P34 or platform Gate acceptance.

## Production behavior

- New Render-owned DamageTracker consumes generation-bound SceneDelta or
  old/new visual contributions. Local moves retain the two exact extents;
  bounded merging is conservative. Unknown backdrop/effect extent promotes
  full redraw. Exact union area is explicitly unavailable for large batches.
- Scene keeps its atomic RF-01 change journal under `SceneChangeJournal`.
  The old header is a source-compatibility alias. The journal is not the new
  per-view/cache damage policy. Existing Scene receipts/query behavior stays.
- RenderGroup metadata separates content, transform, clip, effect, order and
  resource revisions. External traversal order does not invalidate isolated
  raster content. Metadata is private to the existing renderer instance.
- Skia-private cache reuses isolated compact-stroke SrcOver output. All drawing
  still calls the existing canonical command/material/mask helpers. A miss,
  oversized scratch/entry, unsupported command or allocation failure falls
  back to Direct. Representation uses actual cropped allocations; an LRU count
  and byte cap bound residency. It can be disabled/cleared.
- View/metrics/provider/surface generation changes invalidate the device-space
  representation. Failed present clears it. Duplicate/zero render source IDs
  are rejected before drawing. Cache content cannot issue a visibility receipt;
  every frame still uses the existing provider/present/tracker chain.

## Measured workload

Native: three alternating cache-disabled/enabled runs, 64 strokes × 16 samples,
800×500, `round-handdrawn@1`, identical executable/locked SDKs. Median of each
run's final-16-frame medians: **40.49ms Direct → 0.93ms cached**. Each enabled
run records 64 rasterizations, 2016 hits, 64 resident entries, 1,477,376 pixel
bytes. This is host raster software timing, not physical latency or an SLO.

Web: three final-source runs of 32 measured strokes plus one warmup, eight
samples/stroke, matching root SDK/WASM/resources, Chrome/ANGLE Metal Apple M2.
Canonical callback p50: **48.2 / 48.1 / 32.1ms**; final-object callback:
**48.5 / 48.0 / 47.4ms**. LongTask count: **0 / 0 / 0**. The preceding Scene
slice recorded 182–198ms p50 with the same workload; those are historical
measurements, not a freshly interleaved browser cache-off benchmark. Current
native cache-on/off and independent GPU pixel parity supply the controlled
comparison. Machine load materially affects wall time; earlier concurrent
build diagnostics are preserved separately in `out/g5-render-cache`.

## Verification

- Native component suite: 23/23; final focused journal/Skia/provider/cache: 5/5;
  preceding focused material/provider regression: 14/14.
- Exact cache-on/off pixels on CPU and real WebGL2/Ganesh: material v1/v2,
  round/marker/chalk/crayon/watercolor, affine transforms, overlap, reorder,
  delete, clip, mask, background, transparency, LRU/byte fallback, lost/restore,
  stale generation, failed present, resource reset and invalid identities.
- Runtime boundary checker: 115 Scene/foundation/semantic contracts plus new
  Render contract checks. Workspace valid; G5 Python 8/8, Node 5/5.
- Whole repository CTest: 212/234 passing; the same 22 non-passing names as
  the preceding Scene slice. FC02Text timed out at 240s. Full names/logs are in
  `regression-comparison.json` and `regression-ctest.log.gz` in the bundle.
- Python: 193 tests, four prior failures, two skipped (descriptor mapping,
  missing golden probe, imported-font count, legacy opaque-byte assertion).
  Node workspace retains C8 `spawnSync git ENOBUFS`. No all-green claim.

## Remaining boundary

This is the first compact-stroke RenderGroup realization. Scene semantic Groups,
arbitrary hierarchy recording, text/image caching, resource-driven general
RasterCache, automatic promotion, and generic 100K-child group compilation are
not delivered. The aggregate revision value is child-count independent; its
100K-child metadata test is not a production 100K hierarchical renderer proof.

Final target composition remains complete. Damage is used for validity and
diagnostics; it does not yet schedule partial target raster/present. This avoids
assuming retained platform backbuffers. Device-space rasters invalidate on
transform changes; transform-independent reuse is only a resolver capability.
Metadata still visits/copies visible references. Pixel budgets do not account for
metadata, driver allocation granularity or transient Skia allocations; the full
scratch is separately size-capped. Tile/Scheduler/unified ResourceBudget remain
outside this slice. Android/Windows physical evidence is `BLOCKED_EVIDENCE`.

Evidence inventory: `verification/evidence/performance/g5-render-cache/SHA256.json`.
