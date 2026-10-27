# Batch 3 — Source Audit & Reconciliation Notes

## Repository
- Repository: `Mostorm-Labs/axiom`; default branch `main`.
- Source inspection used `main` as available during package preparation. A relevant commit search surfaced `a1e8819abe245ab7487f18771970721f480ff487` (“Simplify Debug UI and fix shared canvas rendering and Fit”). Codex must fetch actual current HEAD before edits.

## Inspected files

### Selection state
`runtime/interaction/include/canvas/interaction/selection_state.hpp`
- `SelectionSummary` currently contains only `count` and `primary`.
- `SelectionState` exposes selected IDs, primary and summary.
- Primary is the first selected ID.
Source: https://github.com/Mostorm-Labs/axiom/blob/main/runtime/interaction/include/canvas/interaction/selection_state.hpp

### Selection session
`runtime/interaction/include/canvas/interaction/selection_session.hpp`
`runtime/interaction/src/selection_session.cpp`
- `SelectionSession` owns `SelectionState`, `groupScope_` and `revision_`.
- `summary()` delegates to `SelectionState`.
- `resolveHit`, `enterGroupScope`, `leaveGroupScope`, `marquee`, `click`, `toggle`, `cancel` exist.
- Current `cancel()` clears selected IDs; do not repurpose without reconciling earlier batches and tests.
- `resolveHit` has existing Group-oriented ancestry behavior; do not replace it with a new container model.
Sources: https://github.com/Mostorm-Labs/axiom/blob/main/runtime/interaction/include/canvas/interaction/selection_session.hpp and https://github.com/Mostorm-Labs/axiom/blob/main/runtime/interaction/src/selection_session.cpp

### Scene query / bounds
`runtime/scene/include/canvas/scene/scene_query.hpp`, `runtime/scene/include/canvas/scene/scene_types.hpp`, `runtime/scene/src/scene.cpp`
- Scene query returns revision, back-to-front IDs and diagnostics.
- Hit testing has a typed kind mask and precise-test diagnostics.
- RuntimeScene records expose canonical kind, placement, transform, derived geometry and geometry/visual/world bounds.
- Scene/BoundsSystem is the source of spatial and geometry facts; UI must not duplicate those calculations.
Sources: https://github.com/Mostorm-Labs/axiom/blob/main/runtime/scene/include/canvas/scene/scene_query.hpp and https://github.com/Mostorm-Labs/axiom/blob/main/runtime/scene/src/scene.cpp

### Runtime and UI discovery points
- `runtime/interaction/src/canvas_editing_runtime.cpp`
- `runtime/composition/src/canvas_runtime_section.cpp`
- `runtime/render/include/canvas/render/editing_overlay.hpp`

Inspect actual facade/composition/Debug UI paths at current HEAD before choosing exact edit sites. The existence of `SelectionSummary` does not mean the complete summary API already exists.

## Known gap
Current `SelectionSummary` is only `{count, primary}`. It is insufficient for per-kind counts, normalized roots, consistent aggregate bounds, capability states, and mixed/partial property aggregation.

## Reconciliation required
1. Verify repository identity, actual `main` HEAD and working tree.
2. Find Batch 1 and Batch 2 branches/PRs/results and classify merged/open/superseded work.
3. Preserve valid work; do not reimplement it.
4. Map every proposed field to an existing owner/API or report the precise missing owner.
5. If selection-root semantics or required owner is missing, stop at that narrow blocker rather than inventing a parallel model.
