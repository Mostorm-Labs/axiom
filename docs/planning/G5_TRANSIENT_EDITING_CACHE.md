# Transient editing over accumulated ink

User reports rectangle drag freezes after many strokes. Repository
Mostorm-Labs/axiom, ancestor04b3f56cc35811c177f3f11d700ed51861bc7025; preserve
prior dirty scheduling/draw-after-zoom changes. Uses existing G5 RenderGroup/cache,
Runtime transform sessions and shared Skia/provider authority. No public SDK API.

Reproduced:64 revision3 strokes, create/select rectangle,20 pointer drag events;
maximum pointer callback1657ms. renderTransientOverlay draws the full scene Direct
from every pointer; creation replays ink Direct too. Derived frame rebuilding is
not canonical mutation and must not bypass shared rendering or receipts.

Owners: Runtime owns selection/transform overrides and dirty state; Composition
services the latest transient frame on deferred hosts; SkiaRenderer/SkiaRenderCache
reuse unchanged ink with existing content/clip/camera/resource/target validity.
No new canonical store, brush algorithm, renderer or platform editing logic.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: transient scene and creation ink pass consume existing isolated ink
  cache; preserve traversal, erase, full replacement, text/connector/chrome pixels.
  Investigation amendment: on deferred hosts use the canonical target's existing
  cache for approximate full replacement editing; the independent overlay context
  is cold and duplicates all raster work at gesture start. This is transient
  feedback only, never a canonical receipt. No cross-context resource sharing.
- required: deferred platform pointer path only marks latest transient demand;
  frame opportunity renders it; no intermediate semantic/History commit or
  canonical receipt; terminal commit/cancel/lifecycle behavior stays unchanged.
- required: active editing does not duplicate the same transient work in canonical
  every tick; committed terminal frame follows existing presentation/handoff.
- tests: fail-first unchanged ink not rerasterized during shape transform and
  creation; Direct byte parity, erase/order/camera/content/resource/target changes;
  real Runtime deferred demand (latest state, cancel/up), G5 native suite, actual
  WebGL corpus, matching SDK browser/handoff/navigation, boundaries/workspace.
- evidence:0/64 strokes, rectangle creation/selection/drag, pointer durations,
  frame callbacks, final pixels/state/artifact hashes; new independent Web demo.
- forbidden: shared SkSurface across canonical/preview, cross-context SkImage
  sharing, alternate brush geometry, public API/semantic/dependency lock changes,
  fake canonical visibility or clearing a committed overlay before replacement.
- hosted: none required; physical platforms remain BLOCKED_EVIDENCE, no P34 PASS;
  historical root failures separate. Terminal: implementation/test/evidence/demo
  or exact authority/environment/oracle blocker. No release/push requested.
