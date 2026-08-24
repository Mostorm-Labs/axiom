# Axiom Architecture Baseline v0.3

> Source: Notion `Axiom 整体架构基线 v0.3`
> Source page: https://app.notion.com/p/3c14c57a590c817bbac7d6b05d264dbf
> Snapshot date: 2026-08-24
> Source status: Current Architecture Baseline — Decision Reconciliation Edition
> Repository status: frozen

## Reconciliation rule

This baseline distinguishes `Accepted`, `Current Direction`, `Superseded`, and `Open`. “Discussed” does not automatically mean “decided”. Later explicit product corrections override older POC/repository baselines while historical decisions remain traceable.

## One-sentence architecture

**Axiom is the C++ Visual Document / Canvas Runtime; Arc is the low-latency input and transient-preview module; Shared TypeScript Data Runtime owns local storage, DocumentSession, Blob and cloud sync; Product Shell owns product UI/business orchestration; Web uses React and native product clients currently converge on React Native while high-frequency Canvas paths remain Native/C++.**

## Top-level ownership

### Product Shell

Owns navigation, toolbar, account, document library, share/permission, window/lifecycle, and product workflows. Does not own canonical Canvas state, SpatialIndex/Tile, or sync state machines.

### Shared TS Data Runtime

Owns DocumentSession, Repository, storage orchestration, Snapshot/OpLog persistence, Outbox, Revision/Cursor, Sync, Blob, AXTP/cloud client, suspend/resume. Does not enter pointer/render hot paths or own Scene/Skia/Arc preview semantics.

### Axiom

Owns InputRouter, InkEngine, Document, canonical Operation semantics, Undo/Redo semantics, SceneCompiler, RuntimeScene, spatial query, HitTest, FrameGraph, and Canonical Renderer. Does not own product login/navigation, cloud provider, persistent background sync, or platform Arc backends.

### Arc

Owns platform low-latency input acquisition, transient preview presentation, capabilities, and latency diagnostics. It does not own Document/Operation/Scene, brush/prediction semantics, Persistence/Sync, or canonical RenderTarget ownership.

### Platform Host

Composition root for Axiom, Arc, native surfaces, frame scheduler, overlays/native views, and final platform composition. It must not duplicate Axiom/Data Runtime semantic rules.

## Product Shell decision

- Web: React + TypeScript; Axiom via WASM. High-frequency PointerEvent uses a thin JS adapter and batching, not React state.
- Android/iOS/iPadOS/normal Windows product client: React Native for product UI; Canvas is a native component/surface; pen/touch/render events do not traverse JS individually.
- Windows screen annotation: Native Overlay Host special case, reusable under the same Axiom/Arc runtime.

Older Windows Tauri and Apple portability-only baselines are historical POC/repository states, not the current product-shell decision.

## Canonical runtime

Semantic Document is the only saveable/migratable/syncable Canvas semantic truth. Canonical writes flow through Operation → Document. RuntimeScene is disposable derived execution state.

`Semantic Document → SceneCompiler → RuntimeScene → ViewQuery/FrameState → FrameGraph/RenderGroups/TileCache → Skia Ganesh`

Full compile and incremental apply must be equivalent at the same canonical revision.

## Performance model

Target behavior:

- `T_write` depends on current input + dirty area.
- `T_frame` depends on visible + dirty content.
- `T_hitTest` depends on spatial query + local candidates.
- Normal frame cost must not scale linearly with total document size.

SpatialIndex responsibility is accepted; final data structure remains benchmark-driven. Long strokes may use derived StrokeChunks. RenderGroup is a runtime isolation/cache/effect structure, not a Document layer. Tile is a derived historical render cache, generated on demand for visible+prefetch regions; global Document revision must not invalidate every tile.

## Ink + Arc dual path

`PointerSampleBatch → Axiom StrokeSession/InkEngine → PreviewStrokeUpdate + Canonical Stroke`.

Preview goes to Arc; canonical stroke goes through Operation → Document → SceneCompiler → Canonical Renderer. Axiom owns smoothing/pressure/brush/prediction/rollback/canonical semantics. Arc never implements a second stroke algorithm.

Canonical Surface/RenderTarget belongs to Axiom PlatformSurfaceAdapter. Preview Surface/Plane belongs to Arc backend. Final composition belongs to Platform Host/OS. Axiom and Arc must not concurrently own the same presentable backbuffer.

Pointer-up does not immediately clear preview: canonical commit must become visible/presented, then acknowledgement/fence permits Arc clear.

## Complex objects / overlay

Skia is a renderer backend, not the Canvas object model.

- Card/Frame/Section semantics belong to Runtime.
- Table uses Skia for browsing and controlled overlay editor for active cell editing.
- RichText/Rich Document semantics belong to Runtime; real IME/DOM/native editor is a platform adapter/overlay.
- WebView/Embed keeps semantic identity/geometry/clip/z-order/lifecycle in Runtime while Platform Host owns the real surface.
- Spatial Scene Graph and Semantic Graph are orthogonal; Semantic Graph/AI Context is not a completed V1 module.

## Shared TS Data Runtime boundary

Storage/Sync is separated from Axiom. React hooks are bindings, not Core dependencies.

Axiom ↔ TS uses the independent AxiomDataBridge. Snapshot and Transaction/Operation transport is opaque bytes at the TS surface. Remote/replay apply must not echo as local-outbox events. Batch snapshot/operation replay is required. Public contract does not promise zero-copy; WASM/JSI/C ABI are bindings over the same semantics.

## Persistence + Sync

Recovery model: **Snapshot + OpLog after Snapshot + BlobRef**.

Axiom owns snapshot semantics/generation/restore. Data Runtime owns location, revision association, save timing, cleanup, compaction orchestration, local durability and cloud sync lifecycle. Large blobs remain external and are referenced.

Sync never enters Axiom. Remote Operations are revisioned/deduped/persisted/reconciled by Data Runtime before applying through Axiom's semantic apply boundary. Mobile default lifecycle is suspend/persist/pause then foreground restore/reconnect/catch-up, not permanent background sync.

## Hot path vs data path

Hot path:

`Physical Input → Arc Input → Axiom → Arc Preview / Axiom Canonical Renderer → GPU → Display`

Data path:

`Axiom semantic commit → Operation → TS DocumentSession → Local Store + Sync Outbox → Cloud`

Their isolation is a core architectural constraint.

## Superseded directions

Superseded include: Axiom Client SDK owning Canvas+Storage+Sync; AxiomDocumentSession owning Runtime/Store/Sync; C++ Data Runtime solely for background sync; persistent mobile background sync as default; Storage/Sync in pointer/render hot path; Skia as object/UI model; per-cell DOM/native editors; Skia reimplementing WebView; Arc as canonical renderer subtype; Axiom/Arc sharing presentable backbuffer ownership; immediate preview clear on pointer-up; Tile owning objects; Cloud syncing Scene/Tile pixels.

## Open decisions

Do not treat as frozen: final SpatialIndex implementation; Tile size/scale ladder; Stroke chunk size/overlap; final thread topology; Arc input/preview/ack ABI; GPU context sharing; Data Runtime product name; Snapshot codec/database schema/compaction cadence; final transaction binary encoding; Document Schema ↔ AXTP relationship; sync gap thresholds/server contract; collaboration algorithm; Semantic Graph/AI contract; final ExternalSurface production implementation; Windows Overlay backend; final Apple/RN host details.

## Migration note

This baseline is the first repository snapshot marked `frozen`. Downstream snapshots must not contradict it without an architecture refreeze.