# Module Detailed Design Closure v0.1

> Primary source: `06 Module Detailed Design Closure / Cross-Module Integration Review v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81418cf6cba4511e97f1
> Snapshot date: 2026-08-24
> Repository status: proposed-freeze

## Closure verdict

The seven subsystem designs form a coherent ownership/dependency/lifecycle baseline without reopening 00–05 authority. Cross-module gaps are glue contracts rather than evidence for another top-level subsystem.

## Final ownership topology

### Axiom Semantic Core
Owns SemanticDocument, canonical ObjectStore, OperationEngine, canonical apply, History and post-commit publication. Normal Ready-state writes go only through OperationEngine; snapshot bootstrap is a Loading-only privileged path. Semantic Core does not depend on Scene/Render/Arc/Data/Host.

### Axiom Interaction Runtime
Owns EditorSession, Selection/Tool/Editing local state, InteractionSession lifecycle, input/command routing, Ink semantics, HitTest policy/orchestration and transient presentation intent. It has no SemanticWritePort; canonical results go through OperationSubmitPort.

### Axiom Scene Core
Owns RuntimeScene/SceneRecordStore, runtime dependency closure, Bounds, renderer-neutral geometry/chunks, SpatialIndex and SceneQuery. RuntimeScene is disposable derived state. At the same canonical generation, incremental compile must be equivalent to full rebuild. Shared RuntimeScene contains no Viewport/DPI/surface/frame/tile state.

### Axiom Render Core
Owns per-view FrameState/Visibility, Damage, RenderGroup/RasterCache, Tile, FrameGraph, canonical render backend/Skia boundary and PresentationTracker. Presentation state is derived. `Presented != CanonicalVisible(token)`; transient pixels cannot satisfy canonical coverage.

### Arc Runtime
Owns Native input acquisition/normalization/batching and optional retained preview presentation. Native input and preview are separate capabilities. Arc never owns brush/canonical semantics. Pointer-up does not clear preview; matching CanonicalVisible evidence does.

### Shared Data Runtime
Owns DocumentSession, physical Snapshot/Operation journal custody, durability, recovery ordering, Outbox/Inbox, revision/cursor/ACK/dedupe, Blob, AXTP and lifecycle catch-up. Axiom owns semantic meaning of canonical bytes; Data Runtime owns durable custody and transport. Remote input is persist-first then apply-second.

### Platform Host
Owns architecture composition relationships, Canvas host registry, platform adapters/slots, Canvas<->DocumentSession association, lifecycle fan-out, capability negotiation, native IME/ExternalSurface realization and teardown coordination. It does not regain any assembled subsystem's semantic authority.

## Cross-module contract planes

Product plane:
`Product Shell -> Axiom Public Runtime` and `Product Shell -> Shared Data Runtime Public API`.

Data plane:
`Shared Data Runtime <-> AxiomDataBridge <-> Axiom Semantic/Document Runtime` using opaque process-local handle + canonical bytes, with local stream separated from external no-echo apply.

Input/preview plane:
`Native OS -> Arc Input -> PointerSampleBatch -> Axiom Interaction -> PreviewUpdate -> optional Arc Preview`.

Presentation plane:
`FrameSource -> Axiom Render -> canonical frame -> compositor -> PresentedFeedback -> PresentationTracker -> CanonicalVisible(token) -> Arc`.

## Six orthogonal lifecycle axes

App, Canvas, Document, Surface, Device and Sync are separate state axes. In particular: background != Document destroy; surface lost != Canvas destroy; Canvas detach != Document close; GPU loss rebuilds derived resources rather than semantic truth; network failure != local document loss.

## Integration resolutions that implementation must preserve

- Current canonical mutation terminology is Operation-only; legacy Transaction naming is historical bridge vocabulary and must not reintroduce global semantic transactions.
- Platform Host is the architecture-level Composition Root; old Axiom Host Runtime identity is superseded at the layering level.
- AxiomDataBridge local vs external apply semantics are separated; remote/replay no-echo is mandatory.
- Canonical commit publication occurs only after successful atomic semantic apply.
- Canvas/DocumentSession binding and teardown must respect independent lifetimes and durability barriers.
- PresentedFeedback is evidence input; Render Core owns canonical coverage qualification and CanonicalVisible emission.
