# 07-00 Runtime Data Flow Overview v0.1

> Source: Notion `07-00 Runtime Data Flow 总览 v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c817ca1e6feca469443cd
> Snapshot date: 2026-08-24
> Source status: Current Direction — Runtime Data Flow Authority Aggregation
> Repository status: current-direction

## Scope

07 defines where runtime data comes from, how it is classified, who owns it, which component transforms it, where it crosses thread/language/process boundaries, and when facts such as committed/durable/synced/rendered/presented become true. It does not redefine Object/Interaction/Operation/Schema/module ownership.

## Five invariants

1. `Semantic Document` is the only canonical semantic truth.
2. HOT PATH and DATA PATH are separate; pointer/preview/render do not traverse Shared Data Runtime, Cloud, or React state.
3. External Apply No Echo: Restore/Replay/Remote may use the same apply engine but never republish as local commits/outbox items.
4. `committed != rendered != presented`; Arc handoff waits for canonical content to be actually visible/presented.
5. `Local Ready > Cloud Ready`; local Snapshot + continuation recovery must not wait on reconnect/catch-up.

## Authority lanes

- Platform / Physical: OS + Platform Host; input, surface, frame clock, composition.
- Arc Input / Preview: high-frequency input transport + transient preview presentation; no Document/Operation/brush semantic ownership.
- Axiom Interaction: InteractionSession, selection/view-local state, transient candidates.
- Axiom Semantic: Operation, Semantic Document, history semantics.
- Axiom Derived Runtime: RuntimeScene, runtime records, bounds, SpatialIndex.
- Axiom Render: Damage, RenderGroup, Tile, FrameGraph, GPU resources.
- Shared Data Runtime: Snapshot/OpLog physical persistence, Outbox, revision/cursor/dedupe, Blob, Sync.

## Eight authoritative flows

1. Pointer → Preview.
2. Pointer / Intent → Canonical Operation.
3. Operation → Semantic Document.
4. Document → RuntimeScene.
5. RuntimeScene → Render → Presented.
6. Commit → Local Storage → Cloud.
7. Remote → Apply.
8. Open / Restore / Catch-up.

## Independent state axes

The following state axes must never be collapsed into a single `ready`, `revision`, or `committed` field:

- Canonical: Transient → Validated → Applied.
- Presentation: Dirty → RenderSubmitted → PresentSubmitted → Presented/Visible.
- Durability: LocalPending → LocalDurable.
- Sync: CloudPending → CloudSynced.

Required inequalities include:

`CanonicalCommitted != CanonicalPresented`, `CanonicalCommitted != LocalDurable`, `LocalDurable != CloudSynced`, `LocalReady != CloudReady`.

## Data classes

07 uses five primary correctness classes:

- Canonical
- Transient
- Derived
- Transport
- Control

Classification is by correctness role, not by physical medium. Semantic meaning owner and physical custody owner may differ.

## Identity separation

OperationId, SemanticGeneration, server revision/cursor, UndoIntent/HistoryGroup, ArcCanonicalToken, frame/present identity, SurfaceGeneration/MetricsGeneration and BlobRef/ResourceId are distinct namespaces and must not alias.

## Codex rule

Implementation may optimize physical layout, scheduling and batching, but may not collapse authority lanes or state axes. The detailed constraints live in 07-01 through 07-15 and Final Closure.