# Exact navigation settle: implementation and measurements

The prior camera/navigation work was committed and pushed as
`6aa330a8d27b8925f8c8be5f93625b07542c3292`. This follow-up makes independent
compact-group cache construction cooperative after navigation stops. Each owner
opportunity prepares at most two groups, checking a provisional 4ms target between
groups. Prepared pixels stay offscreen; complete exact rendering is required
before present, frame submission and CanonicalVisible qualification. New camera
or source validity replaces pending work. Residency denial, eviction or unsupported
content falls back to the existing synchronous path. Public SDK APIs are unchanged.

The Web bridge also avoids invoking preparation twice for an ordinary retry in
one RAF; same-opportunity continuation remains allowed after an outstanding GPU
fence completes. This is a private scheduling correction, not a new browser
rendering algorithm.

Chrome ANGLE Metal / Apple M2, synthetic native CDP input, 64 strokes × 16 samples,
800×500 DPR1, single before/after observation:

| CPU RAF diagnostic | Before | After |
| --- | ---: | ---: |
| Longest wheel-zoom callback | 285.1ms | 14.1ms |
| Longest two-contact callback | 367.0ms | 18.7ms |
| Wheel exact receipt after input end | 397.2ms | 1182.2ms |
| Two-contact exact receipt after input end | 482.0ms | 1180.5ms |

At 400×250, new longest callbacks were 6.0ms / 7.5ms, with exact receipts around
1183ms. The smaller viewport run used adjusted visible stroke/input coordinates;
it is an applicability observation, not a controlled before/after speedup.
This improves main-thread availability while increasing exact completion time.
It does not reduce total coverage computation: repeated visibility/reference
planning and per-group atomic CPU raster remain. The retained navigation snapshot
stays visible during preparation; newly exposed areas still await exact output.
The measurements are CPU callback/receipt diagnostics, not physical input-to-photon,
GPU timings, a stable SLO or a general Excalidraw performance comparison.

Verification: Final G5 native component suite 25/25; RenderCache
material/lifecycle oracle passes on CPU and actual WebGL/Ganesh. Coverage includes
both material revisions, five compact families, preparation budget, no target
mutation/no partial present, exact Direct pixels, camera supersession, capacity
fallback, invalid geometry, lost/restore/stale generation. Runtime navigation
regression covers retained dirty demand and absent partial canonical receipts.
Public JS browser acceptance, frame-handoff context loss/recovery, Shell/Axis and
example drawing pass; boundary checker and workspace validation pass. The full
236-test repository suite was not repeated for this bounded slice; previous
22 baseline failures are not declared repaired. Evidence records artifacts with
actual hashes; older configure-time buildInfo revision in an intermediate run
is retained and the final matched artifact was reconfigured at the current base.

Remaining work: split expensive single-group coverage; reuse immutable reference
sources during preparation; reduce coverage/material CPU cost and uploads; then
broaden Tile coverage/seam/scale applicability. Generic mixed scenes, transformed
objects and masks continue using synchronous fallback. Android/Windows physical
evidence remains BLOCKED_EVIDENCE. This is PASS_WITH_FINDINGS engineering evidence,
not G5/P34 acceptance.

An extra Node workspace run found a historical C8 exact-source generator failure:
its test uses current committed HEAD as a narrowly scoped historical C8 source
commit; `git diff` exceeds the default child-process buffer (ENOBUFS). Isolated
retry is 6/7; semantic CLI reports 137/138. It reads committed HEAD, so this
failure does not consume this slice’s dirty render changes. No historical C8
authority or generator was changed to conceal it.

Evidence: `verification/evidence/performance/g5-settle-preparation/`.
