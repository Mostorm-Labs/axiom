# Axiom Product Object Model v0.1

> Source: Notion `Axiom Product Object Model v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c819cb0e3c5b00e0b54a7
> Snapshot date: 2026-08-24
> Source status: Draft for Review — Product Object Taxonomy / Semantic Boundary Candidate
> Repository status: proposed

## Purpose

This document classifies product concepts before wire-schema freeze. It answers which concepts become Axiom canonical semantic objects and which remain composite structures, product/data entities, external surfaces, local/session state, or derived runtime representations.

It does **not** freeze ObjectKind numeric IDs, complete ObjectContent fields, runtime representations, or a global Transaction layer.

## Classification model

`Product Concept → classify → ObjectKind / Content / Capability / Operation`

Six categories are used:

1. **A — Canonical Semantic Object**
2. **B — Composite / Structured Object**
3. **C — Product Entity / Data Entity**
4. **D — External Surface Object**
5. **E — Local / Session State**
6. **F — Derived Runtime Representation**

## A — Canonical Semantic Object

Prefer canonical representation when the concept has stable identity, must survive save/copy/delete/move/sync/undo, must replay headlessly, does not require a current viewport/GPU/platform widget to exist, and changes through canonical Operations.

Examples: Shape, Image, RichText, VectorStroke, DabStroke, Connector.

## B — Composite / Structured Object

Still canonical, but product semantics include children/content/layout/capability rather than only geometry. Candidates include Sticky, Group, Frame, Section, Table, and Rich Document. Composite does not imply “temporary grouping of primitives”; each concept must be judged on product semantics.

## C — Product Entity / Data Entity

Persisted product concepts whose primary ownership is outside one Axiom Document. The accepted page topology is:

**one Product Page = one Axiom Document**.

Therefore Product Canvas page lists, ordering, title, thumbnail, and `PageId → DocumentId` mapping belong to Product Shell + Shared TS Data Runtime rather than the Axiom object graph.

## D — External Surface Object

The canonical object owns identity, geometry, clip, z-order, and lifecycle contract; Platform Host owns the real interactive surface. WebView/iframe/browser embed is the canonical example. Skia must not reimplement the browser/widget.

## E — Local / Session State

Never persist as ObjectRecord:

- Selection / Hover
- active tool
- Viewport / Camera
- caret / focus / IME composition
- drag-time snap guides
- active stroke preview / prediction
- transient connector routing

Ownership is EditorSession / View / Arc / Overlay.

## F — Derived Runtime Representation

Always rebuildable and never canonical:

- world/visual bounds
- SpatialIndex records
- StrokeChunk / GeometryChunk
- SkPath / mesh / DisplayList
- RenderGroup
- Tile / RasterCache
- decoded texture / GPU handle
- routed connector polyline / screen hit proxy

## Current object-universe decisions

| Product concept | Classification | Current conclusion |
| --- | --- | --- |
| Product Canvas | C Product Entity | Shell/Data Runtime owns page collection |
| Product Page | C Product Entity → 1 Axiom Document | Accepted topology; historical Page ObjectKind requires reconciliation |
| Shape | A Canonical | Covered |
| Image | A Canonical | Covered |
| VectorPath | A Canonical | Covered; must remain distinct from Connector |
| RichText | A Canonical + Hybrid editing | Covered |
| VectorStroke | A Canonical | Covered |
| DabStroke | A Canonical | Covered |
| Sticky Note | B Composite candidate | Gap / review |
| Connector | A Canonical | Strong gap in earlier schema; endpoint/anchor/routing semantics required |
| Group | B Structured candidate | Gap / review |
| Frame | B Structured candidate | Gap / review |
| Section | B Structured/semantic grouping candidate | Gap / review |
| Table | B Composite + Hybrid editing | Post-basic gap |
| Rich Document | B Composite + Hybrid editing | Architecture-ready gap |
| Embed / WebView | D External Surface | Boundary accepted; schema contract required |
| Selection | E Local | Accepted |
| Viewport / Camera | E Local | Accepted |
| Semantic relation / AI graph edge | Not V1-frozen | Keep orthogonal to spatial hierarchy |

## Semantic boundaries

- `ShapeKind` is geometry/visual semantics; Rect/Ellipse/Line do not automatically become top-level ObjectKinds.
- Connector must not be reduced to VectorPath: attachment/routing relation is canonical while routed path is derived.
- Hybrid editing does not move canonical RichText/Table/Document semantics into DOM/native controls.
- ExternalSurface keeps semantic placement in Axiom while real surface ownership stays in Platform Host.
- Runtime optimizations never feed back into Product Object taxonomy.

## Migration note

This file snapshots the Notion authority candidate for implementation consumption. Notion remains the living source until this entry is explicitly frozen in the manifest.