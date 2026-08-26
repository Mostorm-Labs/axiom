# Axiom Semantic Schema Spec + IDL v0.1

> Source: Notion `Axiom Semantic Schema Spec + IDL v0.1`
> Source page: https://app.notion.com/p/3c34c57a590c813fb942d2b874bd1276
> Snapshot date: 2026-08-24
> Source status: V1 Release Candidate Schema Lock
> Repository status: frozen

## Authority / supersession

The earlier `Schema Freeze Candidate` wording is superseded for V1 by `Axiom Semantic Schema V1 Release Candidate Final Gate v0.1`. Historical OPEN/review text is design history where later release authorities close or defer it.

This authority supersedes the old global `Transaction → operations[]` canonical mutation model. **Operation is the only canonical delta.**

Semantic Schema is encoding-neutral at the semantic-model level. The V1 concrete wire/codec release is separately frozen as **Protobuf + Axiom Canonical Protobuf**; generated protobuf structure must not redefine the Document Model.

## Core semantic invariants

An Operation is one complete, deterministic, atomic, serializable, persistable, syncable, replayable semantic state mutation.

Operations must leave the Document valid after apply; replay headlessly without UI/Selection/Viewport/Skia/Network; describe final semantic mutation rather than gesture/renderer command; exclude runtime/derived/sync lifecycle state; and apply idempotently for the same OperationId. Same OperationId with a different payload is protocol corruption.

Command/Intent may depend on selection/tool/viewport; Operation may not.

## Canonical vs derived

Never enter canonical schema: world/visual bounds; SpatialIndex/Tile data; Skia/mesh/GPU handles; Selection/Hover/Viewport/Camera/Active Tool; Arc preview/prediction; server revision/sync/retry/local DB metadata.

## Frozen schema boundaries

### ObjectRecord

Complete canonical state of one Semantic Document object: `ObjectId + ObjectKindId + KindVersion + Placement + Transform2D + PropertyBag + ObjectContent + EraseMasks[]`. Placement, Transform, Content and EraseMask are not generic properties.

### FieldId + PropertyValue

Only small stable independently patchable semantic properties. Placement, Transform, ObjectKind, Stroke/Path geometry, RichText and EraseMask are not fields. Published FieldIds are permanent; retired IDs are reserved.

### Placement

`parentId + orderKey` is one atomic structural value. Reparent/reorder use `SetPlacements`.

### StrokeRecord

Renderer-neutral canonical stroke content: BrushDescriptor + deterministic seed + VectorStrokeData or DabStrokeData. Preview/prediction/SkPath/mesh/StrokeChunk/Tile/GPU data are derived.

### RichTextDelta

One committed RichText semantic mutation. Internal RichText steps do not recreate a global Transaction layer. Caret/selection/focus/IME intermediate composition are non-canonical.

### EraseMaskGeometry

Object-local renderer-neutral canonical erase region. Runtime compilation is derived.

## Common primitive rules

Stable IDs are opaque 128-bit identities. Canonical numeric rules reject NaN/Infinity, canonicalize `-0` to `+0`, use released precision/ranges, and use radians for explicit rotation. `OrderKey` is opaque bytes compared by unsigned-byte lexicographic order.

## Operation envelope

Conceptually contains OperationId, DocumentId, schemaVersion, payloadVersion and OperationPayload. Server revision/local sequence/sync/retry/transport metadata belongs to Shared TS Data Runtime.

## V1 Operation vocabulary — release reconciliation

The early version of this overview listed 12 operations. The ObjectContent/Field review found three mutation-coverage gaps; downstream Operation Payload, Reference IDL and Generated Proto authorities close them. **The released V1 vocabulary contains 15 Operation kinds, with OperationPayload oneof tags 1..15.**

Therefore implementation MUST consume the exact 15-kind table from `02-operation-model/operation-payload-validation-v0.1.md` plus `04-reference-idl/reference-idl-codec-mapping-v0.1.md` / generated proto. The historical 12-item list is not implementation authority.

## Release reconciliation

Later release authorities override historical draft text inside this overview. Current release set includes ObjectContent/Field Registry, Shape, Image, Connector, Brush/Stroke, BrushFamily/interpreter, Pressure/Tilt, RichText wire/font, Common Wire, OrderKey, Reference IDL, Generated Proto/Canonical Codec, and Semantic Hard Limits.

## Implementation-facing rule

Runtime/Render/Sync structures are derived/lifecycle state and may evolve independently. Any implementation contradiction with this locked schema must be raised as an architecture blocker and resolved through refreeze rather than silently changing authority in code.
