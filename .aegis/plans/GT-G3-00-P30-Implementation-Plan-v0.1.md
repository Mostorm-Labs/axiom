# GT-G3-00 — G3 Basic Canonical Canvas P30 Implementation Plan v0.1

Status: READY_FOR_P31. This is an implementation plan, not a P31 execution package and not an implementation authorization.

## Authority and dependency

- G2 closure: `notion://3dd4c57a-590c-81a9-827e-e348102413d0`, bound to canonical `main@07654767eac7b021ca6e89004e36448ccebb727d`.
- Current G3 execution authority: `notion://3c84c57a-590c-81b6-bab3-c36fc6842c4f` (v0.2).
- Boundary inputs: G3 Readiness Gap, Render Boundary, Resource Budget, Verification Integration, and Boundary Completion Review refs recorded in `.aegis/authorities.json`.

G3 consumes the G2 RuntimeScene/SceneQuery contract. It must not recreate RuntimeScene or alter G2 frozen contracts.

## Objective

Deliver the first renderer-neutral canonical canvas vertical slice: `RuntimeScene → per-view FrameState → Render-owned Visibility → Direct/NonTiled Reference Source → minimal frame plan → private backend → Canonical Surface → PresentSubmitted → Presented feedback`, with Headless, Windows reference, and Web reference hosts sharing the same production Render Core.

## Non-goals and forbidden work

- No duplicate RuntimeScene, SemanticDocument traversal, or second scene compiler.
- No Tile, RenderGroup, RasterCache, production Scheduler, or ResourceBudget optimization implementation (G5).
- No Arc/Ink handoff or production selection/editor semantics (G4/G6).
- No final FrameGraph ABI or final Windows/Web surface topology freeze.

## Reviewer-sized work packages

1. **G3-01 per-view Render Core foundation** — immutable `FrameState`, view/camera/metrics/surface generations, and target wiring; prove two views share a scene generation while remaining camera/surface independent.
2. **G3-02 Render-owned VisibilityResolver** — consume G2 SceneQuery through the Render Core; preserve deterministic ordering and locality; Demo/Shell must not supply visible IDs as renderer truth.
3. **G3-03 Direct/NonTiled Reference Source** — map all G2 V1 kinds to a backend-neutral deterministic draw plan and digest; preserve the permanent reference path.
4. **G3-04 minimal frame orchestration/backend contract** — define gate-local frame plan and private `IRenderBackend` seam; keep Skia types private and avoid final FrameGraph ABI claims.
5. **G3-05 private Skia Headless backend** — render the direct plan through the locked prebuilt SDK; add deterministic raster/golden observation without making Headless a shortcut.
6. **G3-06 surface generation and PresentationTracker** — model acquire/resize/present feedback, stale generation rejection, and `PresentSubmitted` versus `Presented`.
7. **G3-07 Headless Canvas Demo CLI** — open fixed fixtures, render through production Render Core, emit digest/golden/evidence artifacts.
8. **G3-08 demo camera and hit/select harness** — pan/zoom/hit/select as transient per-view/demo state; assert canonical semantic and RuntimeScene state remain unchanged.
9. **G3-09 Windows reference surface host** — native reference lifecycle and generation binding; no CPU-readback-to-GDI live path and no semantic traversal in the adapter.
10. **G3-10 Web WASM/WebGL2 reference host** — thin host bridge, dynamic resize/DPR/surface generation, no fixed-size/global POC state and no JS scene traversal.
11. **G3-11 cross-platform oracle and Gate Report** — integrate current G0 verification workspace; require structural/runtime/render/performance evidence, locality, stale-generation negatives, and explicit BLOCKED when required platform evidence is absent.

Each WP follows TDD: failing test, observed RED, smallest authorized change, focused regression, and independently reviewable evidence/commit. P31 must split these WPs into an execution closure with exact files, tests, evidence, terminal blockers, and `continue_until_terminal_state: true`.

## Verification outline for P31

- Semantic: operation replay equals expected semantic document and remains unchanged by camera-only actions.
- Runtime: incremental render input equals a fresh/full reference plan and preserves RuntimeScene generation/content.
- Rendering: Headless golden and approved platform pixel-tolerance/readback evidence.
- Performance: locality counters and bounded working-set observations; no whole-document traversal for local visibility.
- Lifecycle: stale surface/frame/presentation generations deterministically reject or defer.

## Entry result

G3 Entry Readiness is complete: architecture boundary PASS, verification boundary PASS, implementation scope DEFINED, hidden execution gap NONE_FOUND. Next stage is P31 task packaging. No G3 implementation is authorized by this plan.
