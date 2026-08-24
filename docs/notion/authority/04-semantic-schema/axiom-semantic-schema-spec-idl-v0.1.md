# Axiom Semantic Schema Spec + IDL v0.1

> Source: Notion `Axiom Semantic Schema Spec + IDL v0.1`
> Source page: https://app.notion.com/p/3c34c57a590c813fb942d2b874bd1276
> Snapshot date: 2026-08-24
> Source status: V1 Release Candidate Schema Lock
> Repository status: proposed-freeze

## Authority / supersession

The earlier `Schema Freeze Candidate` wording is superseded for V1 by the Semantic Schema V1 Release Candidate Final Gate. Historical OPEN/review text is design history when later release authorities close or defer it.

This authority supersedes the old global `Transaction → operations[]` canonical mutation model. **Operation is the only canonical delta.**

Semantic Schema is encoding-neutral. C++ types, TS types, and Wire IDL map from the semantic contract; Protobuf/FlatBuffers/custom encoding must not redefine the Document Model.

## Core semantic invariants

An Operation is one complete, deterministic, atomic, serializable, persistable, syncable, replayable semantic state mutation.

Operations must:

- leave the Document in a valid state after apply;
- replay headlessly without UI/Selection/Viewport/Skia/Network;
- describe final semantic mutation rather than gesture or renderer command;
- exclude runtime/derived/sync lifecycle state;
- apply idempotently for the same OperationId; same ID with different payload is protocol corruption.

Command/Intent may depend on selection/tool/viewport; Operation may not.

## Canonical vs derived

Never enter canonical schema:

- world/visual bounds
- SpatialIndex records, Tile IDs/generations
- SkPath/SkPaint/SkImage/mesh/GPU handles
- Selection/Hover/Viewport/Camera/Active Tool
- Arc Preview/predicted tail/PreviewRevision
- server revision/sync state/retry state/local DB row ID

## Six frozen schema boundaries

### ObjectRecord

Complete canonical state of one Semantic Document object:

`ObjectId + ObjectKindId + KindVersion + Placement + Transform2D + PropertyBag + ObjectContent + EraseMasks[]`.

Placement, Transform, Content, and EraseMask are not generic properties.

### FieldId + PropertyValue

FieldId is only for small, stable, independently patchable semantic properties such as visibility, locked, opacity, fill, stroke style, blend mode. Placement, Transform, ObjectKind, Stroke/Path geometry, RichText, and EraseMask are not fields. Published FieldIds are permanent; retired IDs are reserved and never reused.

### Placement

`parentId + orderKey` is one atomic structural value. Reparent and reorder are represented by `SetPlacements`, not independent SetParent/SetZOrder operations.

### StrokeRecord

Renderer-neutral canonical stroke content: `BrushDescriptor + deterministicSeed + VectorStrokeData | DabStrokeData`. ObjectId/Placement/Transform remain in ObjectRecord. Preview, prediction, SkPath, mesh, StrokeChunk, Tile, and GPU buffers are derived.

### RichTextDelta

One committed RichText semantic mutation. It may contain internal steps such as InsertText/DeleteText/SplitParagraph/MergeParagraph/SetInlineStyle/SetParagraphStyle, but these steps do not recreate a global Transaction layer. Caret/selection/focus/IME intermediate composition are non-canonical.

### EraseMaskGeometry

Object-local, renderer-neutral canonical erase region. Runtime may compile it to vector clip, R8 texture, sparse mask tiles, etc.; those representations never change Document schema.

## Common primitive rules

Stable IDs are opaque 128-bit identities: DocumentId, ObjectId, OperationId, ResourceId, EraseMaskId, ParagraphId. They are client-generatable offline, stable forever, and independent of server round-trip. Generation algorithm is not frozen.

Canonical numeric direction:

- geometry/position/transform/size: f64;
- normalized appearance coefficients: f32;
- NaN/Infinity forbidden;
- `-0` canonicalized to `+0`;
- explicit rotation uses radians;
- color channels use `[0,1]`.

`OrderKey` is opaque bytes compared by unsigned-byte lexicographic order. Generation algorithm is outside protocol contract.

## Operation envelope

Conceptually:

- operationId: OperationId
- documentId: DocumentId
- schemaVersion: u32
- payloadVersion: u32
- payload: OperationPayload

Server revision, local sequence, sync state, retry count, and transport request ID belong to Shared TS Data Runtime envelopes, not canonical Operation.

## V1 Operation vocabulary

1. `InsertObjects`
2. `DeleteObjects`
3. `RestoreObjects`
4. `SetPlacements`
5. `SetTransforms`
6. `PatchProperties`
7. `SetVectorPathGeometry`
8. `AddStroke`
9. `SplitStrokes`
10. `AddEraseMasks`
11. `RemoveEraseMasks`
12. `EditRichText`

Resource upload/binary lifecycle is not a V1 canonical Operation; objects reference stable ResourceId/BlobRef.

## Release reconciliation

Later release authorities override historical draft text inside this spec. Current release reconciliation includes dedicated Shape, Image, RichText font, Stroke/Brush, RichText wire, Reference IDL, and conformance authorities. The repository migration must preserve those as separate authority snapshots rather than silently folding them into this overview.

## Remaining implementation-facing rule

Runtime/Render/Sync structures are derived or lifecycle state and may evolve independently. Any implementation contradiction with this schema must be raised as an architecture blocker and resolved through refreeze rather than by silently changing the authority in code.

## Migration note

This snapshot is marked `proposed-freeze`, not `frozen`, until its release-authority dependency set is migrated and reconciled in the repository manifest.