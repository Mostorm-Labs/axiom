# Selection Batch 2 — Scope and Behavior Contract

## Purpose

Extend the shared selection chain to container hierarchy and the currently delivered object kinds. This batch follows the first public selection-chain batch, but must inspect the actual execution branch/HEAD before editing. Do not assume unfinished first-batch work exists on `main`, and do not duplicate already-completed changes.

Repository: `Mostorm-Labs/axiom`
Known inspected baseline at package preparation: `main`, repository default branch. The exact current execution HEAD must be recorded by Codex at start.

## In scope

1. Selection roots and hierarchy reduction for Group / Section / nested parent-child structures.
2. Avoid returning both a selected container and descendants as independent operation roots when the product behavior means the container represents them.
3. Ensure Section's background/bounds do not automatically swallow or mask valid selection of objects inside/overlapping it.
4. Extend region selection coverage to currently delivered objects where reliable geometry exists: Shape (Rectangle/Ellipse/Triangle), VectorPath, VectorStroke, DabStroke, Connector, Image including crop presentation/canonical crop semantics, RichText/Sticky, Group, Section.
5. Tests for ancestry, nested groups, Section background behavior, connector geometry, long strokes, and cropped images.
6. Preserve Runtime's current single source of selection state, existing Scene query/hit-test facilities, selection overlay, and Operation/History boundaries.

## Out of scope

- Re-designing first-batch public APIs unless an actual blocking defect is found; if so, stop and report the earliest owning task rather than quietly widening scope.
- Full multi-object transform/property editing.
- New ObjectKinds or schema changes.
- Reworking Group/Section canonical operations or changing Section authority.
- Snap/Alignment implementation, Group feature expansion, Table, OCR, diagram auto-layout, or new render acceleration structures.
- Broad platform host rewrites.

## Behavior contract

- The selected set is an interaction projection, not canonical document data. Selection-only actions must not create Operations or History entries.
- Hit-testing and region selection use the same object visibility, lock, parent/scope and user-facing identity rules where applicable.
- A child implementation object (for example Sticky's RichText child) must resolve to the user-facing parent identity according to existing behavior.
- Group scope constrains eligible targets; selection outside the active scope must not leak in.
- Selection roots must be deterministic and non-redundant. If a selected ancestor semantically covers a descendant, only the ancestor is an operation root. Preserve multiple independent roots.
- Section is a spatial/container object, not an unconditional occlusion mask. Its visible background must not prevent selecting eligible content inside it. Section selection itself remains possible when the pointer/region targets its boundary/background according to existing hit priority rules.
- Area selection must use precise geometry where the object type supports it; AABB-only intersection is candidate generation, not proof of a hit.
- Image crop selection must respect the currently accepted crop semantics: excluded source pixels are not selectable as visible image content, while the image's intended crop/edit mode behavior remains unchanged.
- Connector selection must consider routed line and arrowhead geometry, not only its enclosing bounds.
- Vector/Dab strokes must be selectable by visible stroke geometry, including erase masks where existing geometry supports it; do not treat an arbitrarily large stroke AABB as a precise hit.
- Any unimplemented object-specific oracle must be recorded as explicit coverage status, not counted as PASS.

## Cancellation and tool changes

- Region-gesture cancellation discards only the in-progress region and preview.
- It must preserve the prior valid selection unless an explicit clear-selection action was requested.
- Do not globally redefine legacy `SelectionSession::cancel()` if it is currently used as clear-selection. Add a narrow cancellation/reset seam if needed and reconcile with batch one's accepted contract.
