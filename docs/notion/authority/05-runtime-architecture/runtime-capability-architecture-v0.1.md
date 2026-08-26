# Runtime Capability Architecture v0.1

> Sources: Runtime Capability Overview v0.1; Ownership Matrix v0.1; Dependency Graph v0.1; Public Boundary Contract v0.1; later 06 cross-module closure where it resolves 05 integration identity.
> Snapshot date: 2026-08-24
> Repository status: proposed-freeze

## Top-level capability domains

- **Axiom** owns canonical Canvas semantics and derived visual runtime: Semantic Document, Operation execution/history, Interaction semantics, RuntimeScene and canonical rendering.
- **Arc** owns native low-latency input acquisition and optional transient preview presentation. It never owns Document/Operation/Scene authority or a second brush algorithm.
- **Shared Data Runtime** owns durable byte custody, DocumentSession, Snapshot/OpLog physical persistence, Outbox/Inbox, sync revision/cursor/ACK/dedupe, Blob, AXTP, suspend/resume/catch-up and recovery.
- **Product Shell** owns product UI/workflow/navigation/account/share/permission and drives Axiom/Data Runtime only through public contracts.
- **Platform Host** is the architecture-level Composition Root: it composes runtime/session leases, platform adapters, host slots, lifecycle and final platform realization without regaining subsystem semantic authority.

## Canonical invariants

`SemanticDocument` is the only canonical truth. RuntimeScene, Bounds, SpatialIndex, GeometryChunk, Damage, RenderGroup, Tile, Skia/GPU resources, Selection, Viewport, preview state and sync metadata are non-canonical.

The pointer/preview hot path does not traverse Product Shell state, Shared Data Runtime, Snapshot, Outbox or Cloud.

Remote/replay canonical mutation enters Axiom through AxiomDataBridge and the Operation Engine; it never writes Scene/Tile/Renderer directly.

Lifecycle axes are orthogonal: App != Canvas != Document != Surface != Device != Sync. Surface loss must not destroy the canonical Document; sync disconnect must not make a locally ready Canvas unusable.

## Ownership negative contract

Forbidden dependency/authority patterns include:

- Product Shell -> RuntimeScene / SpatialIndex / Tile / Skia / raw GPU ownership.
- Shared Data Runtime -> Scene / Render / Arc / Pointer hot path / semantic object graph mirror.
- Arc -> SemanticDocument / ObjectStore / OperationEngine / RuntimeScene.
- Renderer -> SemanticDocument mutation.
- Platform overlay -> bypass Axiom canonical mutation.
- Platform Host -> duplicate brush, operation, persistence or sync semantics.
- Axiom -> Cloud / Outbox / server revision ownership.
- Tile / RasterCache -> persistent semantic truth.

## Build / module dependency direction

Core rules:

1. implementation depends on contract, not the reverse;
2. Composition Root may depend on assembled module public/lifecycle ports; assembled modules do not depend on Host implementation;
3. Semantic Core does not depend on Interaction, Scene, Render, Arc, Data Runtime or Platform Host;
4. Scene derives from Semantic; Render derives from Scene;
5. Interaction may depend on Semantic read/submit ports and Scene query ports, never Semantic -> Interaction;
6. Axiom <-> Arc runtime flow is through a narrow shared contract;
7. Axiom <-> Shared Data Runtime runtime flow is through AxiomDataBridge;
8. renderer/Skia is never upstream of Semantic Core.

Conceptual Axiom direction:

`Public/Platform Runtime -> Interaction / Semantic / Scene / Render`, with `Interaction -> Semantic + SceneQuery`, `Scene -> Semantic`, and `Render -> Scene`.

## Public/cross-module boundaries

Product Shell -> Axiom exposes coarse Canvas lifecycle, document attachment, tool/command, viewport, selection/state summary and health/capability. It does not expose internal scene/render/Arc/GPU objects.

Shared Data Runtime <-> AxiomDataBridge uses process-local opaque document handles and opaque canonical bytes; local commit stream and external apply are separated; remote/replay apply is no-echo.

Arc <-> Axiom logical flow:

`Arc -> PointerSampleBatch -> Axiom Interaction`

`Axiom -> Preview update -> Arc Preview`

`Axiom Render/Host evidence -> CanonicalVisible(token) -> Arc clear`

## Platform Host identity reconciliation

The older `Platform Host Runtime Contract v0.1` described Platform Host as an Axiom-internal single-Canvas host runtime. The later 05/06 authority resolves `Platform Host` as the architecture-level Composition Root. Retained Axiom-only platform integration may be realized as an internal Axiom Platform Runtime/Adapter Runtime; exact name remains implementation/integration follow-up.

This identity reconciliation is mandatory when reading historical Host documents.
