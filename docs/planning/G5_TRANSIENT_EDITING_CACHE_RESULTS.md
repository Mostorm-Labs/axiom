# Rectangle drag over accumulated ink

Local implementation result: PASS_WITH_FINDINGS. No release, cross-platform
qualification or P34 claim. Scope: G5_TRANSIENT_EDITING_CACHE.md. Based on pushed
04b3f56cc35811c177f3f11d700ed51861bc7025 plus retained dirty navigation and
draw-after-zoom changes; source and artifact hashes are recorded separately.

The report reproduces in Chrome WebGL2, ANGLE Metal Apple M2, 800x500 logical
pixels at DPR2, with 64 deterministic round-handdrawn revision3 strokes. Create
a rectangle, select it, and move it across 20 pointer observations. The old
pointer callback peaks at 1657.3ms, with first GL feedback submission 1622.4ms
after down. renderTransientOverlay replayed every background stroke Direct
synchronously from pointer input. Creation's topmost ink pass did the same.

SkiaRenderer now uses the existing RenderGroup/SkiaRenderCache for transient
scene and creation ink passes. Camera, clip, resource, content, target and
generation validity remain enforced. Deferred pointer processing retains the
latest transient demand; RuntimeComposition renders it at a frame opportunity
without duplicating canonical work during creation, transform or region selection.
Semantic mutation still occurs through the existing terminal operation/history
path, never on each move.

Reusing a cache on the independent preview provider alone still incurred a cold
full-scene raster at gesture start: its WebGL context cannot use canonical GPU
images. Deferred full replacement editing therefore uses the canonical provider
and its renderer's warm cache as approximate feedback. ARC brush preview retains
its independent surface. No image crosses contexts, no surface is shared, and
transient editing does not issue CanonicalVisible. The terminal exact frame and
platform completion retain the existing handoff rules. Cancellation restores
canonical pixels without applying a transform; stale overlay is hidden.

| Observed case | Max pointer callback | Max live render RAF | First GL submission | Max pointer interval |
| --- | ---: | ---: | ---: | ---: |
| Before, 64 strokes | 1657.3ms | 2.1ms | 1622.4ms | 1658.9ms |
| Final, 64 strokes | 1.1ms | 2.0ms | 18.4ms | 18.7ms |
| Final, empty background | 1.9ms | 1.5ms | 17.9ms | 18.2ms |

The before RAF figure excludes the expensive pointer callback; moving work off
the pointer path alone would not explain the improvement. A diagnostic run showed
the warm canonical target retaining 64 ink entries (4,481,280 bytes), no additional
ink misses during drag, and hits increasing per frame. That instrumentation was
removed from production. The final run has one semantic generation increment,
the expected selected-object transform, no canonical attempt during live drag,
and no runtime errors. Before/final RGBA SHA256 is identical:
7f343f1364e87041ad12946cd08c40125bb9591873fbc0aaac9962df20b93213.
Measurements describe synthetic pointer CPU callbacks and GL submission, not
physical input-to-photon latency or guaranteed budgets.

Verification: final native G5 suite 27/27, actual WebGL Direct byte parity,
matching SDK browser acceptance, pending-fence/resize/context-loss handoff,
Shell/Axis, demo and navigation regressions, facade/frame retry, Runtime boundary
and workspace checks. Fail-first cache and redundant-canonical tests are retained.
The handoff test now creates its real delayed canonical fence using zoom before
creation: active creation intentionally no longer submits a canonical frame.
An early concurrent-link probe attempt used mismatched artifacts and is invalid;
the recorded final GPU probe uses a completed build and exits zero.

Limits: traversal/reference encoding still scales with visible scene size; cold
targets, camera changes, cache eviction or unsupported groups can still be costly.
This does not solve the independent CPU mutable brush-tail bottleneck documented
in G5_DRAW_AFTER_ZOOM_RESULTS.md. Historical full-root/G3 failures are unchanged;
this was a focused G5/SDK regression run, not a new full-root pass. Android and
Windows physical-platform evidence remains BLOCKED_EVIDENCE. The isolated 4193
demo preserves 4192 and its drawings. No commit/push was requested for this fix.
