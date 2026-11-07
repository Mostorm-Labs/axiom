# Cumulative live erasure: CPU-miss investigation

**BLOCKED_ENDPOINT. The user's cumulative erasure problem is not solved.**
This bundle records one qualified partial rendering fix and the remaining failure.
It is not official G5/P34 PASS or a competitor/physical-trackpad qualification.

The earlier two25point-erasure/settled-navigation fixture did not establish live
feedback scalability. This investigation keeps7200samples and15nearly overlapping
duplicates, then uses10rounds of128point local erasure. It separates input dispatch,
RAF/frame intervals, Up, canonical recovery and later navigation. Snapshot capture
is disabled in latency workers. `inputs.json.gz`, `setup.json.gz` and independent
outer before/after fingerprints survive a worker deadline; no incomplete worker
is promoted to a passing observation.

The prior diagnostic profiled live phases took about54/52seconds; Up was about
0.4/0.8seconds, and post-Up canonical CPU coverage was0ms. Exact matching function
body hashes, rather than assuming relinked WASM indices stay stable, attribute
about61%of sampled self time to portable SkRasterPipeline functions. The symbol
relink, correspondence and raw CPU profiles are corroborative investigation data,
not a clean paired latency speedup claim. Whole-module code ordering changed;
unknown function bodies are intentionally not attributed.

The missing policy is concrete: `SkiaRenderCache::drawDamagedInk` called the shared
body helper without its enabled GPU coverage option on a nonresident source.
The helper therefore used CPU Direct coverage on every damage frame under source
cache pressure. Source raster invalidation can be correct while this fallback is
expensive. The patch carries the already qualified v4 option and the existing
budgeted GPU target pool through that seam and counts actual CPU/GPU coverage.
Explicit CPU, Raster and other algorithm allocation paths retain their policy.
No Brush algorithm, canonical geometry, masks, operation/history, SDK API, schema,
lock or32/96MiB constants change. Skia may still use CPU internally; the scoped
GPU counters do not claim an entirely GPU implementation.

The targeted actual Ganesh test first failed with `v4 live erase cache miss
silently used CPU coverage`, then passed with geometric-AA, independent backdrop,
permanent/transient mask, forced nonresidency and explicitCPU checks. Five existing
GPU probe modes also pass on the clean package. Native full CTest is compared by
exact failed/not-built names and unchanged executable hashes. All inherited
failures are named in `checks/native-comparison.json`; none is silently closed.

An additional, previously unqualified GPU v3 damage matrix reports a failed-present
invalidation counter assertion, despite the preceding image comparison passing.
The focused repeat also fails. This is preserved as an **unresolved finding**;
it is not labeled passed, pre-existing or a proven cause of the user's v4 stall.
The ordinary native matrix and the six listed qualified GPU modes are separate.

The GPU miss fix is insufficient. Development growth data already showed live
p95 rising from about100to297ms and Up from0.4to3.4seconds over seven completed
rounds, with the worker stopping during the eighth round. Clean paired results in
`summary.json` own the final numbers. A deadline, missing round or p95>=150ms
blocks the cumulative endpoint, even if individual semantic/receipt checks pass.
The separate settled two-erasure navigation result has a narrower scope and
cannot supersede this failure.

Next review should separate the existing derived representations: share immutable
unmasked body/prepared geometry independently of mask revision; reuse permanent
mask work and process the active append incrementally under the same budget;
profile and accelerate exact CPU erase-target filtering separately. A small
destination damage clip does not bound source geometry/mask replay. Current-Scene
navigation proof must remain valid; simply removing Scene/receipt guards would
permit stale pixels and is not an accepted repair. These are proposed follow-up
items, not implemented or newly frozen semantic authority in this bundle.

The partial-fix Demo is4272. It is not advertised as solving cumulative erasure;
4271 and older user pages were left intact. `packages.json` binds all36consumer
files of ordinary/diagnostic clean builds to the exact code revision.

Reproduce the bounded failure with the accepted locked SDK/toolchain:

```sh
G5_PROFILE=0 G5_CAPTURE_SNAPSHOTS=0 G5_ERASE_POINTS=128 G5_ROUNDS=10 python3.12 verification/runners/g5/bounded_worker.py --prefix out/cumulative --timeout 180 --rss-mib 3072 -- node verification/runners/g5/cumulative_erase.mjs out/g5-live-miss-clean-off/js out/cumulative-data 2 candidate
python3.12 verification/evidence/performance/g5-live-erase-cpu-miss/verify_evidence.py
```
