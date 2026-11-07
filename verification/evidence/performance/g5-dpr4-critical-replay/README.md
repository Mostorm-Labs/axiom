# DPR4 critical replay admission repair

Status: **READY_FOR_CONTROL_REVIEW** for this bounded implementation and local
engineering corpus. This is not official G5/P34, statistical, physical input,
other-platform or release acceptance. The earlier blocked evidence is preserved.

## Frozen inputs and ownership

- Repository: GitHub Mostorm-Labs/axiom.
- Worktree: `/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom`.
- Branch: `codex/geometry-navigation-integration`.
- Trusted anchor: `1b22b23c8087b1c7cc997706e8154b348d21cc3a`.
- Package and expected-RED test commit: `4d665c23`.
- Fixed code/artifact revision: `5ab41d5f8782958f8bdcbf329f81929da55c2f03`.
- Execution contract: `docs/planning/G5_DPR4_CRITICAL_REPLAY_2026_11_07.json`.
- Before artifacts: clean `c05d3a2f54fcb44c771d797bbe3255f46dcac962`, whose production
  implementation matches the trusted anchor. After artifacts: clean `5ab41d5f`.
- Both profiles use matching installed SDK 0.4.3 wrappers/types/WASM/resources,
  36 files, root-only exports and no installed native headers/archives. Complete
  package hashes are in `raw/packages.json`.

Runtime/OperationEngine/Scene continue to own document, editing and derived Scene
state. Render's existing PreparedGeometryCache owns source-bound contour replay;
ResourceBudgetManager owns admission and leases; Skia/provider own drawing and
presentation. No public API/ABI, SDK version, Brush geometry, semantic codec,
dependency lock or third-party SDK build was changed.

## Root cause and implementation

The prior DPR4 trace showed many short navigation attempts without present. The
immediate cause was not a single long CPU call. Mandatory rendering targets and
resources already put total charged usage above the optional 32MiB ceiling while
the 96MiB shared hard cap still had room. Small-v4 prepared path staging/residency
used optional admission, even when coarse coverage required that geometry.
Prepared misses were denied, leaving the scalar fallback to traverse source
knots across RAF opportunities. Conservative coarse-cell queries included ink
outside the exposed strip, which still had to be processed without local bounds.

The sole production file changed is `runtime/render/src/skia_prepared_geometry.cpp`:

- Reuse the already-qualified compact checkpoint replay builder for short v4
  critical misses when optional path staging or residency admission fails.
- Bind the required source with the same critical priority. Admit checkpoint
  storage and bounded replay scratch against the existing shared hard cap.
- Release any optional full-path staging before admitting its compact replacement.
- Preserve ordinary optional admission, entry/global caps, immutable source
  equality, owner invalidation, scalar fallback, drawing order and raster phase.

The existing navigation caller already distinguishes missing coarse coverage from
optional refinement. No new scheduler, surface, geometry algorithm or admission
class was added. Query, step, GPU primitive, time and byte limits are unchanged.

In the current ON pair's selected-pan phase:

| Observation | Before | After |
| --- | ---: | ---: |
| Maximum GL content-call gap |316.9ms|18.6ms|
| Incomplete-coverage attempts |18|0|
| Prepared geometry denied |4|0|
| Prepared geometry resident |0|104,336 bytes|

The new local replay lets existing culling/frontier dependency checks recognize
offscreen ink. The source history is still compiled to its qualified checkpoint
index; it is not discarded or replaced with a different Brush algorithm.

## Tests and build evidence

Two real behavioral oracles failed against the old implementation, then passed:

1. `shortCriticalReplayAdmission()` in `prepared_geometry_test.cpp`: short
   round/marker/cap/taper strokes above the soft limit but below the hard cap;
   direct full-history contour pixel parity under clipping/reflection/shear;
   locality, sharing, live-consumer retention, release, entry denial and hard
   denial. It does not pre-retain the source to work around the defect.
2. `checkCriticalNavigationPressure()` in `render_cache_test.cpp`: conservative
   queries containing offscreen ink must publish complete visible coverage within
   the bounded opportunity despite soft pressure. It uses real navigation,
   provider, budget and Skia components. The known visible result is white;
   culling must not paint any offscreen source into it. It runs in the full suite
   and through `--critical-replay-only`.

RED diagnostics were respectively:
`visible short replay denied despite hard-cap headroom` and
`offscreen short ink delayed visible coverage under soft pressure`.
The first RED ran before implementation. The integration RED used exactly the old
anchor geometry source with the new test, then restored only this task's owned
fixed source. Exact input identities and procedure are in `execution-inputs.json`.

Native full relinking completed every buildable target; the same four prior
Semantic switch-enum compile failures remain. Full configured CTest is
**1010/1076 passed**, with the **same 66 failed/not-built names**, no new failures
and no omitted names. PreparedGeometry, RenderCache, Damage and the existing
affine/erased/navigation/retirement tests passed. 39 G5 Node and 119 boundary
checks passed. Full build/RED/GREEN/CTest/Node/boundary logs are under `logs/`;
`raw/native-comparison.json` lists the existing failures.

## Browser evidence

Ten fresh bounded Chrome workers used the installed public root SDK and the
unchanged compound post-edit runner. Before/after DPR4 were interleaved, followed
by a second ON candidate, ON/OFF DPR1/2, and ON DPR4 context restoration. There
were no builds or native tests during latency sampling. Source and complete
artifact hashes were stable before/after every worker.

Every worker completed eight edits (copy/paste, transform, three partial erases,
sticky text, table text, image replacement), Undo/Redo pixel round trips and
twelve navigation phases. Context restoration adds one phase. The mixed document
includes the same 2,334,857-byte synthetic image and 4096-input stroke; it is not
the user's original image. All document/operation/selection/camera/receipt checks
passed. Before/after and ON/OFF pairings have identical per-edit exports,
per-phase final canvas pixels and final exports; see `paired-pixels.json`.

| Worker | Selected pause→pan gap | Maximum content-call gap across all phases | Maximum final receipt wait |
| --- | ---: | ---: | ---: |
| Before OFF DPR4 |333.8ms|333.8ms|234.7ms|
| After OFF DPR4 |18.1ms|35.1ms|218.6ms|
| Before ON DPR4 |316.9ms|316.9ms|217.3ms|
| After ON DPR4 |18.6ms|34.6ms|232.9ms|
| After ON DPR4 repeat |18.5ms|35.3ms|233.2ms|
| After ON DPR4 restore |17.3ms|34.9ms|231.4ms|
| After ON/OFF DPR1 |17.7/18.5ms|18.7/18.7ms|166.5/166.5ms|
| After ON/OFF DPR2 |18.3/18.3ms|34.4/35.3ms|167.6/167.9ms|

The candidate DPR4 selected-pan observations all meet the package's **local
engineering endpoint <150ms**. That endpoint is not a universal or statistical
SLO. The repeated ON and OFF cases corroborate the deterministic admission and
culling oracles; they do not constitute statistical inference about all sessions.
DPR1/2 meet the frozen local nonregression guards against the prior observations,
with unchanged pixels. See `nonregression-guards.json` for exact margins and scope.

DPR4 actual WebGL context loss/restoration also passed: surface unavailable while
lost, generation 1 → 2 in ON diagnostics, unchanged document and export pixels, and
first restored receipt 122.5ms after the restore request. The deliberate 100ms loss
pause and later quiet waits are excluded from that first-receipt measurement.

Submission continuity and exact completion remain distinct. Final receipts still
include settle scheduling; the existing DPR4 canonical CPU coverage work remains
about 66ms over the most expensive paused phases. It is not declared eliminated.
Maximum observed shared-budget occupancy rose from 66.72MiB in the ON baseline to
76.62MiB in the first ON candidate as more useful preparation could reside; all
observed usage stays under the unchanged 96MiB hard cap. No claim of zero memory
tradeoff is made. OFF has unavailable/null native timing and budget diagnostics.

GL calls, Runtime local accepted submissions, canonical receipts and physical
display are different observations. `cameraQualified=false` is retained. Final
readback/export checkpoints are outside latency loops. No new readback, GPU wait,
background loop or renderer was introduced. Synthetic wheel and a 1000ms pause do
not qualify physical Mac input, prolonged idle or every cold resource workload.

## Scale and admission review

The additional public-SDK ON DPR2 smoke workers completed 10K objects and ten 100K
strokes (1M document points, 16K point centres in the viewport). They reuse the
previous hash-bound valid fixtures. `scale-smoke.json` and compressed raw results
record source points, counts, pixels, receipts, budget and setup. These are
corroborative regression observations, not upgrades of blocked object tiers.

`docs/planning/G5_LARGE_DOCUMENT_ADMISSION_REVIEW_2026_11_07.md` records the next
boundary review. Snapshot wire occurrence 65,535, ObjectRecord 16MiB and single
CompactStroke 262,144-knot rules remain unchanged. Codec/snapshot/CompactStroke
sources, SDK version and dependency locks match the anchor (`preserved-inputs.json`).
Changing the intended large-document admission range needs its own Semantic/product
scope and memory/error/atomic-restore proof. No direct-store or SDK-private escape
hatch is proposed. Earlier 100K/1M-object rejected/capped evidence is preserved.

All 12 final workers completed cleanup. Native/browser-family containment remains
2GiB/3GiB and 180s per worker. RSS sums may count shared pages more than once;
they are containment observations, not unique physical memory measurements.

## Reproduction and review

Use the exact package and clean code revision with the accepted prebuilt Semantic
and Skia SDKs. Matching ON/OFF build/install logs are archived. The complete
sequential sampler and collector are retained in `logs/g5-dpr4-execute.py` and
`logs/g5-dpr4-collect.py`; use fresh output paths and wait for builds/tests to end.
The runner invocation remains:

```sh
python3.12 verification/runners/g5/bounded_worker.py \
  --prefix out/recheck-worker --timeout 180 --rss-mib 3072 -- \
  node verification/runners/g5/post_edit_navigation.mjs \
  out/g5-dpr4-off/js out/recheck 4
```

Set `G5_CONTEXT_RESTORE=1` for the restore phase. Test counts, source/artifact
identities, raw timing, correctness, worker stages and outcomes are retained;
large local snapshot/SDK binaries are represented by their hashes and deterministic
fixture inputs rather than copied into this bundle. `SHA256.json` covers all
archived files except itself. The user's 4242 Demo and older evidence are intact.

A separate default-OFF candidate Demo is served at
`http://127.0.0.1:4260/example.html` from the frozen `out/g5-dpr4-off/js` package.
This is a local convenience, not durable physical testing evidence.
