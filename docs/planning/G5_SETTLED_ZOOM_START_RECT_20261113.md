# Settled zoom startup: rectangular exposure repair (r2)

Status: `READY_FOR_CONTROL_REVIEW_WITH_AA_EDGE_LIMITS`. This is a scoped,
uncommitted candidate, not an overall G5, P34, release, or performance PASS.

## Owner and change

The existing `SkiaNavigationTiles` job still owns ordered sources, prepared
geometry leases, coverage, material, validity, and publication. Relative to the
retained r1 candidate, the only production change is in
`runtime/render/src/skia_navigation_tiles.cpp`.

Zoom exposure already culled primitives using the bounding demand. r2 actually
clears and replays **every ordered source, mask, and material within that demand**
under a simple rectangular scissor. It publishes the rectangle's validity only
after the existing job completes. This replaces the disjoint `SkRegion` clip;
it does not certify unpainted corners. Sharp retained canonical content continues
to win by density. The pan path, r1 GPU eligibility, query safety gate, job
metadata, quotas, time/step/memory budgets, and public APIs are unchanged.

r1 remains sealed as `BLOCKED_UNMET_PERFORMANCE`. Its source preimages, SDKs,
176 artifact hashes, results, drivers, and old demos were preserved.

## Frozen comparison

All three SDK lanes restored the same actual snapshot and metadata for each
ordinary/pixel case, then followed natural settling, one second quiet, and the
original 24-wheel sequence at DPR2, CSS1200×800. Two headed Chrome starts used
opposite lane orders; one isolated IAB page ran ordinary and pixel documents
separately. No additional browser runs were made.

First default-target content GL API call after wheel input is a **submission
proxy**, not physical first photon, GPU completion, or a canonical receipt:

| Case | Chrome baseline → r2 median | IAB baseline → r2 |
| --- | --- | --- |
| One stroke extending outside viewport | 58.3 → 17.5 ms | 97.6 → 32.1 ms |
| Three strokes, after partial erase | 34.55 → 25.7 ms | 41.6 → 18.5 ms |
| Central short stroke control | 2.45 → 2.1 ms | 4.3 → 3.7 ms |

Primary missed opportunities decreased from 3→1 and 2→1 in Chrome, 6→2 and
3→1 in IAB. Primary post-wheel max RAF remained below18ms; candidate input
lateness did not increase. The longest masked Chrome RAF-start gap was26.8ms
versus18.7ms baseline; this finite scheduling variation is retained, not omitted.
Final settling remains separate: IAB one-stroke last-wheel-to-stable was276.8ms
versus248.1ms, so this does not establish a general convergence-tail win.

The final conservative ledger charges168/480seconds: Chrome25+24 and IAB119.
IAB charge includes server bootstrap and completion-to-cleanup time; its actual
workload completed in55seconds. r1's120seconds remain unchanged.

## Correctness and limits

All36 saved state rows retained the actual source objects, body/config/pressure/
seed/masks and document identity. Navigation emitted zero operations; initial and
final cameras matched across lanes. All runtimes closed; no input/frame failure
or context loss was observed. The snapshot field comparison corroborates real
SnapshotCodec restore/capture and is not another codec authority.

The three final Web images are not all byte-identical to baseline. The primary
cases differ at4 and5 pixels (12 and13 channels, max76); central short is exact.
These differences were independently projected from the unchanged snapshot's
full-history contour/masks and camera. Every changed pixel falls inside the
existing fixed1devicepixel contour boundary. Every pixel outside that boundary
matches the independent full CPU reference **exactly**, including all channels.
This is the user's existing AA-edge allowance, not a global2LSB tolerance or a
candidate-derived exclusion band. Control preserved the literal package and
recorded the clarification separately:

`.aegis/results/G5-settled-zoom-start-rect-control-r2/final-pixel-acceptance-clarification.json`.

The first three Web opportunity readbacks cover a finite central ROI only. They
show earlier movement but do not prove the entire first physical frame. Native
tests independently compare the full first completed viewport with a cold
absolute-grid/density reference plus the actual retained canonical resample,
at Metal DPR1/2/4 and Raster DPR1. No overlap or boundary strip is skipped.

Ten focused native CTests passed, as did strict frontier checks, three Node
metric checks, Python tool syntax/offline analysis, and120 runtime boundaries.
Control independently relinked and passed the same ten native tests. Locked
prebuilt Semantic/Skia packages were consumed; no third-party rebuild or full
project-suite PASS is claimed.

## Reproduction and artifacts

Result root: `.aegis/results/G5-settled-zoom-start-rect-repair-r2/`.
Build/raw buffers: `out/G5-settled-zoom-start-rect-repair-r2/`.

`verification/runners/g5/settled_zoom_start_rect_boundary.cpp` uses the existing
corner-join snapshot/full-history boundary oracle. The adjacent
`settled_zoom_start_rect_boundary_build.py` reuses the current native build's
compile/link arguments and locked dependency archives. The reproducible build
recipe and three exact output comparisons are recorded under
`offline-pixel/reproducible/`. The first temporary tool's rejected const
assignment is retained as a harness compile failure, not a product failure.

The offline `settled_zoom_start_rect_analyze.mjs` and
`settled_zoom_start_rect_pixels.py` regenerate timing and semantic/pixel reports
without starting a browser. Original sampled drivers/requests/source/SDK assets
are bound by before/after freeze hashes. Later additions are offline tooling and
the new demo server only.

Ordinary candidate for human testing: <http://127.0.0.1:4335/example.html>.
The41 non-HTML HTTP assets match the sampled SDK. Only the HTML response receives
a candidate label; SDK files remain unchanged. Servers4331–4334 and user
documents are untouched. No commit/push, new repair iteration, extra browser
sampling, or follow-on work is authorized by this result.
