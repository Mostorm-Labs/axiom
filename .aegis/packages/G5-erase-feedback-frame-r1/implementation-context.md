# G5 erase feedback-frame cost, item 1 only

Current source B is saved/pushed bfceeab0 (4348). Stable accepted ancestry63e9ec33 remains; B has scoped correctness and immediate-navigation gains, but100ms worst-gap was103.13ms in one pinch replicate. This is not full Gate PASS.

Control attribution of exact D2:42 actual-render frames in last2s before Up, Render median33.6/p9536.5ms, exclusive median32.95ms. Presentation adaptation median.3ms, pending owner0ms. Body-build/compile/coverage counters all zero in these42 frames; does not exclude driver/GPU work. First post-Up owner37.7ms, Render35.8ms. New matched probes must establish the cause; old D2 is not a new timing baseline.

At bfcee skia_renderer.cpp:1466 retains pendingNavigation display, disallowing use of the same surface as scratch;1471 acquireTransientSurface;1495 inherits prior full image into distinct scratch before damage clipping;1563 snapshot;1606 gpu.finish;1608 provider.present;1627 copy/pin feedback source roster;1648 prepareQueryPrefixes after publication. Cache acquireWorkSurface around1213 allocates canvas.makeSurface. WebGlSurfaceProvider::present flushes/submits nonblocking. These are candidates, not assumed causes.

Editing/active-sealed contribution ownership stays CanvasEditingRuntime. RuntimeComposition coordinates opportunity; existing SkiaRenderer/cache/GpuResourceRetirement owns derived pixels and leases. OperationEngine alone commits atomically. No consumer SDK API change.

Frozen local-mask conversion cache, charged pending feedback plane, true AutoIntent twofinger cancel repair and full latest-suffix/no-hit equivalence are already implemented. Do not redo or weaken them. Active per-object fulltrace conversion and frozen-hit deep copies still exist, but adaptation is small in current D2; broad prefix/mask transfer is later item2, Scene/History/commit refactor later item3.

If allocation/fullcopy dominates, a charged reusable pool is permissible only with immutable retained snapshot, full accumulated-damage catch-up and true GPU-lastuse safety. If retained roster/full comparisons dominate, use immutable reuse under complete identity. If driver submit dominates, measure the exact command/flush path before removing redundant work; never remove completion/lifetime guarantees. Keep same profile512/1024/128MiB; do not substitute blind memory/LOD changes for cause.

First incomplete action: new isolated task tree from bfcee, package/identity/source/SDK/storage preflight, B probe-only attribution then minimal measured repair and complete EFF1..5. Final metrics have explicit relative gains and100/33ms absolute targets; no tuning after frozen ordinary matrix. Every old failure retained. No new commit/push/tag authorized.

Preserve existing4342..4348 including user IAB4348 and r2 source/SDK/evidence. Use isolated Chrome; preview first unused>=4349 after normal exported root SDK smoke. Explicitly notify control at preview and terminal, continue without ordinary ACK. Do not stop at design/code/native alone. Return READY_FOR_CONTROL_REVIEW only after full frozen closure, or a named genuine blocker with exact evidence.
