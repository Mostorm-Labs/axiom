# Selection Batch 2 — Source Audit and Likely Change Surface

This is a source-map starting point, not a frozen allowlist. Codex must inspect current HEAD and any first-batch branch before deciding exact files.

## Confirmed source observations at package preparation

- `runtime/interaction/include/canvas/interaction/selection_session.hpp`
  - `SelectionSession` owns `SelectionState`, `groupScope_`, `revision_`, and exposes `click`, `toggle`, `marquee`, `resolveHit`, `enterGroupScope`, `leaveGroupScope`, `cancel`.
  - `marquee()` consumes `HitCandidate` values; it does not itself query Scene geometry.
- `runtime/interaction/src/selection_session.cpp`
  - `resolveHit()` walks parent ancestry, checks visibility property, and promotes hits to Group ancestors. Active `groupScope_` constrains ancestry.
  - `toggle()` adds/removes IDs; this is not the same as Shift-add semantics.
  - `cancel()` clears selected IDs; preserve this meaning for explicit clear paths unless first-batch authority already defines otherwise.
- `runtime/interaction/include/canvas/interaction/hit_test_service.hpp` and `src/hit_test_service.cpp`
  - `HitCandidate` currently carries object ID, visible, locked. `selectable()` filters zero/hidden/locked and duplicates only.
- `runtime/scene/include/canvas/scene/scene_query.hpp`
  - `Scene::query(Rect)` returns candidate object IDs in back-to-front order.
  - `Scene::hitTest(Point, tolerance, filter, maxResults)` is point-based and returns front-to-back IDs plus precise-test diagnostics.
  - Kind masks include Shape, Image, VectorPath, RichText, VectorStroke, DabStroke, Connector, Sticky, Section.
- `runtime/interaction/src/canvas_editing_runtime.cpp`
  - Runtime maps view points to world/content coordinates, queries Scene for point hit tests, resolves Sticky/RichText identity and Group scope through `SelectionSession`.
  - Selection overlay and transform paths remain mostly primary-object oriented; do not expand into full multi-object editing.
  - Section boundary drag and Connector attachment/creation paths are interwoven with selection pointer handling and need regression protection.
- `runtime/composition/src/canvas_runtime_section.cpp`
  - Facade's `readSection` delegates to Runtime Section snapshot; inspect corresponding command/runtime files before changing Section selection semantics.
- `runtime/scene/src/scene.cpp` and `runtime/scene/include/canvas/scene/derived_geometry.hpp`
  - RuntimeScene derives bounds/geometry including connector routes and Section boundaries; use existing geometry instead of inventing parallel shape logic where possible.
- `runtime/render/include/canvas/render/editing_overlay.hpp`
  - Selection chrome is per-view transient presentation; area selection preview should remain in this presentation lane.

## Expected change areas (confirm before editing)

1. Selection hierarchy/root reduction helper, preferably in interaction layer and covered by focused tests.
2. Shared area-selection query/geometry service that takes the region, queries Scene candidates, applies exact geometry tests and emits deterministic IDs.
3. Narrow Scene/Runtime bridge for selecting candidates without bypassing `Scene::query` and semantic identity resolution.
4. Runtime selection pointer integration for Section and container ordering/priority.
5. Tests in interaction and composition/scene targets; CMake wiring only where needed.

## Explicit risk points

- Group selection can be incorrectly double-counted with descendants.
- Section's large bounds can become an overbroad hit target and mask enclosed objects.
- `Scene::query` results are AABB candidates and must not be treated as exact polygon/lasso hits.
- RichText children and Sticky parents may duplicate hits or expose internal implementation identity.
- Image crop has transient presentation state; ensure canonical crop and preview crop do not disagree in hit geometry.
- Connector routing may be deferred or not ready; define a safe and explicit behavior for unavailable derived geometry.
- Strokes with erase masks need the same visible-geometry semantics as current rendering where possible.
- Group/Section selection and drag behavior must remain stable while region selection is added.
