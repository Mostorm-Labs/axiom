# Postcommit navigation feedback: quiet clock correction

Status: `READY_FOR_CONTROL_REVIEW_WITH_FINDINGS`. Controlled baseline:
`codex/g5-cpu-worker@911c149b18dbb26632b4273ff60259d53eb18560`.
No commit/push, dependency rebuild, public API change or overall G5/P34 PASS.

## Cause and repair

The earlier25→55ms first-GL comparison did not identify equivalent feedback.
Old first draws commonly belonged to precommit transient publication; the current
first draws belong to postcommit navigation. New-camera transient preparation
retained its progress while limited by the existing two-group slice and the
shared owner allowance. Commit changes the authoritative mask/Scene basis, so
invalidating that unfinished old-basis work is required. Actual diagnostic
body compile/replay after Up was zero; this was not a whole-geometry rebuild.

The demonstrated defect was the navigation quiet-clock origin. Composition
services pending erases before selecting transient/navigation/canonical work.
While transient editing owns the frame, navigation is ineligible. On its first
eligible postcommit opportunity, `serviceNavigationPresentation()` previously
started another100ms quiet interval even though the camera had already stopped.

The sole behavior change records the existing navigation camera generation,
change time and active flag in `CanvasEditingRuntime::invalidateViewportChange()`.
The100ms threshold, original budgets, frame routing, semantic mutation and
presentation/identity gates remain unchanged. Default-off diagnostic events
record actual route, preparation results, resets and BackendSubmissionResult
messages. SDK consumers and Render algorithms are unchanged.

## Proof and measurements

Actual RED used the restored three-stroke fixture, real C++ Worker kernel result
and OperationEngine atomic erase. At a navigation opportunity101ms after the
camera input, both old zoom/pan cases wrongly requested another navigation
window. Both now leave that window immediately. New camera input still retains
the99/100ms boundary, and exact same-camera no-op input does not restart time.
Existing synthetic-clock tests now read the owning camera clock; their guards
and cases were retained.

The frozen31 CTests plus two new tests passed33/33. Fourteen Node tests and120
runtime boundaries passed. Native full1431×1010 comparison uses independently
projected canonical objects and null-cache Direct drawing: pan is exact; zoom
has three1-channel-unit differences inside the fixed source-only1-device-pixel
contour band. Delayed display retirement, foreign surface receipt rejection,
retry pixel retention and unchanged canonical values are checked.

Two reversed ordinary Chrome runs used the same snapshot and immediate
Up→zoom0.7 or pan70,-30. They have no native timing, held Worker or pixel reads.

| Ordinary median after Up | Baseline | Candidate |
| --- | --- | --- |
| Zoom canonical-qualified root onFrame callback | 172.5ms | 128.0ms |
| Pan canonical-qualified root onFrame callback | 162.5ms | 123.3ms |
| Zoom owner opportunities | 11 | 8.5 |
| Pan owner opportunities | 10.5 | 8 |

The root onFrame observation is distinct from semantic presented-generation,
Operation publication callback and physical first photon. First-GL proxies are
mixed: zoom median58.3→63.9ms; pan38.8→36.9ms. No first-feedback or overall
performance PASS is claimed. Two diagnostic sessions independently show earlier
canonical work without body recompilation; their timings are not ordinary data.

Four separate pixel cases read a900×600 physical ROI at actual default-target
content writes. Final candidate/baseline ROI bytes are identical for zoom/pan;
non-edge pixels match the reused independent CPU reference exactly. Original
body/config/pressure/placement and added mask geometry match. Sampled frames are
nonblank, do not return to the saved old-camera image, and keep the fixed erased
interior opaque white. Navigation frames issue no canonical receipt.

Intermediate coarse images are not final exact images: zoom/pan differ from
final at2415/2780 pixels, maximum channel differences199/208. Those coarse
hashes are identical between baseline and candidate. Candidate content3 is
already final while baseline content3 remains coarse. This supports preserved
sampled behavior and earlier convergence, not absolute correctness of every
intermediate viewport pixel or every mask boundary. Native whole-frame proof
and finite Web ROI qualification are separate.

## Artifacts and testing demo

Results: `.aegis/results/G5-postcommit-navigation-feedback-r1/`.
Builds and per-row binary evidence: `out/G5-postcommit-navigation-feedback-r1/`.
`postcommit_navigation_analyze.py` regenerates the offline reports; it does not
start a browser. Recorded harness compile failures and the invalid stale-binary
CTest run are retained and explicitly excluded from successful proof.

Exactly two diagnostic, two ordinary and one pixel Chrome sessions completed,
charging28/600seconds. All sampling process families closed and freezes matched.
Old evidence and the4336 user demo were not changed.

New ordinary demo: <http://127.0.0.1:4337/example.html>. All42 HTTP assets match the
sampled candidate, including HTML. No additional browser smoke or user-page input
was performed. Broader navigation clarity, long erase tails and idle behavior
remain outside this scoped result.
