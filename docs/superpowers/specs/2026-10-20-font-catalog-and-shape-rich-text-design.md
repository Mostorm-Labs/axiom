# Font Catalog Facade and Shape Embedded Rich Text

## Status

Draft for review. This document freezes the semantic and runtime boundary before implementation.

## Problem

The text runtime can already consume manifest-backed font resources and edit ordinary
`RichTextContent`, but the public `RuntimeFacade` does not expose a stable read-only
font/resource catalog. A Shape also cannot carry a label, so a label must either become
an unrelated hidden object or be implemented by a host-owned text path. Both choices
would break the existing object, undo, scene, and resource contracts.

## Decisions

### 1. Shape label ownership

`ShapeContent` owns an optional embedded `RichTextContent label`. The label is part of the
Shape canonical content and is never materialized as a child object or a hidden
`RichTextContent` object. Shape remains a leaf for hierarchy, selection, hit testing, and
placement. Existing Shape objects without a label retain their current behavior.

### 2. Shape content versions

Existing Shape content versions remain accepted:

- version 1: rectangle/ellipse fields (`shape_kind`, `width`, `height`);
- version 2: triangle support with the same geometric fields.

Embedded labels use Shape content version 3. Version 3 is required when `label` is
present; a version 3 Shape may also have an empty label. A decoder must not silently
discard a label from version 3. A version 1 or 2 payload never gains an implicit label.
Unknown Shape versions remain rejected by the existing version/capability validator.

The capability requirement is `shape-richtext-label-v1`. It is independent from the
existing `richtext-fixed-width-v2` capability; a document using a label must declare
both the label capability and the RichText capabilities required by the embedded
document. Existing capability IDs and bit assignments are unchanged; this is a named
document capability and does not renumber the existing `0..7` capability domain.

### 3. Label layout

The label uses Shape-local coordinates. Its layout rectangle is exactly
`[0, 0, width, height]` in Shape local space. The existing RichText paragraph alignment,
line height, inline style, font resource, and wrap rules are reused. No new vertical
alignment mode is introduced.

The label is clipped to the Shape geometry:

- rectangle: rectangular clip;
- ellipse: ellipse path clip;
- triangle: triangle path clip.

The label never enlarges Shape scene bounds, selection bounds, or hit bounds. Shape
selection and hit testing continue to use the Shape body and its existing geometry.
Changing Shape width/height relayouts the label and invalidates the same scene, text,
resource, and frame projections as changing an ordinary RichText object.

An empty label is valid in version 3 and is retained in canonical content when explicitly
present. It produces no glyph pixels and does not create a separate object or undo entry.

### 4. Label editing

The existing `TextEditSession` remains the only text editing state owner. Its target is
extended from an ordinary RichText object to a target descriptor that identifies either
an object-level RichText document or a Shape label field. The session still emits the
existing `EditRichTextOp`-style delta through the Runtime owner; it does not emit host
specific mutations and does not create a hidden text object.

Text intent, selection, caret, composition, formatting, undo grouping, and text geometry
remain unchanged. A Shape label edit is committed atomically as a Shape content update
and participates in the same History/Undo/Redo, Snapshot, replay, digest, and recovery
paths as other Shape content changes.

If a Shape is deleted, locked, lost, or closed, the normal Runtime write admission rules
reject label edits. Cancelling a label session has no canonical effect.

### 5. Canonical encoding and persistence

The protobuf Shape message gains an optional `label` field containing the existing
RichText wire message. Field numbers are allocated without renumbering existing fields.
The mapping is explicit in both directions and preserves absence versus an explicitly
empty label.

Shape content canonical encoding, semantic digest, Snapshot bootstrap, Operation replay,
History restore, and ChangeSet dependency calculation include the label whenever it is
present. Label paragraph IDs and referenced font resource IDs remain canonical data.
Old version 1/2 Shape bytes decode exactly as before and have the same digest as the
legacy representation. A version 3 label mutation changes the Shape content revision
and document digest even when the outer geometry is unchanged.

### 6. Resource dependencies and font catalog

Every font resource referenced by a Shape label is included in the same resource
dependency and manifest validation path used by ordinary RichText. Missing, removed,
stale-generation, corrupt, or identity-mismatched resources make the label unavailable
with the existing diagnostic state; the renderer must not silently substitute a platform
font.

`RuntimeFacade` gains read-only catalog methods:

```cpp
FontCatalogSnapshot readFontCatalog() const;
std::optional<ResourcePortReceipt> readFontResourceStatus(
    foundation::ObjectId resourceId) const;
```

`FontCatalogSnapshot` contains the admitted font resource ID, family name, resource
revision, content hash, byte length, binding/resource state, and diagnostic/failure
reason. The snapshot is a read model only; it is not persisted in a document and cannot
mutate the manifest. Debug UI consumes this facade surface and never reaches into
`RuntimeComposition`, resource containers, or `semantic/src` headers.

Font family names are read from the manifest-backed registered identity. The catalog is
updated only on the Runtime owner sequence after a successful manifest/resource commit;
rejected publish or bind attempts leave the previous catalog unchanged.

### 7. Scene and render integration

Scene projection computes Shape geometry from the outer Shape fields and separately
records the label layout/resource digest. Label changes invalidate the Shape scene
record and frame demand without changing its outer bounds. Direct reference generation
emits one Shape draw item with an optional label payload; it does not add a child scene
node.

The canonical and headless Skia paths share the same RichText layout service and font
resource resolution. Preview uses the same local label rectangle and clip contract as
canonical rendering. Selection overlays draw the Shape bounds only.

### 8. Font catalog and UI controls

The facade catalog is the only source for Debug UI font choices and status. The existing
Text format patch remains the mutation path for font, size, color, alignment, bold,
italic, underline, strikethrough, and bulleted paragraphs. Runtime supports arbitrary
canonical color values; Debug UI may expose its two approved presets. No font file is
installed into the OS as part of this feature.

## Compatibility and failure rules

- Old Shapes and old documents remain readable without the new capability.
- A document that contains a label but omits `shape-richtext-label-v1` is rejected at
  validation; the host cannot strip the label to make it pass.
- Invalid RichText, invalid layout dimensions, or invalid font references reject the
  whole Shape content update atomically.
- Rejected publish/bind/resource operations have no side effects on existing bytes,
  catalog entries, document state, or frame demand.
- A missing or failed font produces the existing diagnostic/placeholder behavior and
  keeps the Shape bounds and object identity stable.

## Verification matrix

1. Legacy Shape v1/v2 decode, digest, bounds, render, hit, and replay remain unchanged.
2. Version 3 Shape with empty label round-trips with presence preserved.
3. Version 3 label with mixed inline styles, paragraph alignment, bullets, and a manifest
   font round-trips through protobuf, Snapshot, Operation replay, Undo, and Redo.
4. Label edits change Shape content revision/digest and request a canonical frame while
   preserving Shape bounds and selection identity.
5. Rectangle, ellipse, and triangle clipping rejects glyph pixels outside the Shape body.
6. Width/height changes relayout the label without changing outer bounds semantics.
7. Missing, stale, corrupt, removed, and re-bound font resources produce the expected
   diagnostics and never fall back to an unregistered platform font.
8. `RuntimeFacade::readFontCatalog()` and `readFontResourceStatus()` expose the exact
   manifest/resource state and remain read-only.
9. Debug UI can enumerate fonts and submit existing format intents only through the
   facade.
10. Canonical and preview rendering agree on label position, style, clipping, and font
    identity.
11. Closed/Lost/locked Shape write paths reject label edits; cancel leaves no history.
12. Strict Clang/MSVC builds and the focused semantic, scene, text, composition, and
    render tests pass without changing SDK locks or historical gates.

## Non-goals

This change does not introduce nested Shape children, a new text model, a second font
registry, vertical alignment, OS font installation, new stable ABI/SDK, or a new render
backend. It also does not change existing ordinary RichText object semantics.
