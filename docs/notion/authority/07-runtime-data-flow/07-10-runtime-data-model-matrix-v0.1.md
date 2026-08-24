# 07-10 Runtime Data Model Matrix v0.1

> Source page: https://app.notion.com/p/3c44c57a590c814baf98d17393a7473f
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Runtime Data Classification / Ownership Matrix

## Classification rule

Every runtime entity has a primary correctness class: Canonical, Transient, Derived, Transport or Control. Classification is by correctness role; physical durability or carrier form does not change semantic authority.

## Never alias these namespaces

- OperationId — canonical mutation identity/idempotency.
- SemanticGeneration — runtime-local canonical post-state token.
- server revision/cursor — Sync protocol ordering/catch-up metadata.
- UndoIntent/HistoryGroup — local editor-history grouping.
- ArcCanonicalToken — preview→canonical visibility correlation.
- Frame/Present identity — presentation stream identity.
- SurfaceGeneration/MetricsGeneration — platform binding/metrics epochs.
- BlobRef/ResourceId — canonical external resource identity.

Later 07-11 adds CanonicalCommitStamp and RecoverySequence to the same non-alias rule.

## Key entity ownership

Input/interaction: PointerSample transient; PointerSampleBatch transport; InteractionSession transient; selection/tool/viewport/editor state control; UndoIntent control; InteractionDependencyFootprint control; LocalIntentPlan/IntentCommitSegment control; IME composition transient; EditorProjection derived; Preview Packet transport.

Semantic: Operation/ObjectRecord/SemanticDocument canonical; Operation bytes transport representation of canonical data; SemanticGeneration control; ChangeSet post-commit control/derived impact event.

Scene/render: RuntimeScene, bounds, SpatialIndex, GeometryChunk, RenderGroup, Tile, GPU cache are derived; FrameState/HandoffRequirement are control; PresentedFeedback is transport/control evidence.

Data runtime: Snapshot carries canonical state; OpLog contains canonical Operation representations; DurableInbound, Outbox, cursor/frontiers, RecoverySequence, LocalRecoveryClosure are control; Blob bytes are physical custody while BlobRef identity remains canonical.

## Meaning vs custody

Axiom owns semantic meaning of Operation/Object/BlobRef. Shared Data Runtime may physically hold encoded Operations, snapshots and blob bytes without acquiring semantic mutation authority. Platform Host may carry presentation evidence without acquiring canonical coverage authority.

## Persistence rule

Canonical representations may persist/sync. Transient and Derived state do not. Control state may be ephemeral or durable according to its orchestration purpose, but never becomes Object/Operation semantic payload merely because it is durable.

## Implementation rule

C++/TS/WASM/Arc/platform code must use strong types/names that preserve namespace separation. A generic integer `revision`, `generation`, `token` or `ready` that aliases multiple axes violates this contract.

## OPEN

Exact C++ struct layout, JSI/WASM memory layout, database schema, server ordering algorithm, FrameGraph layout and platform presentation primitive remain outside this matrix.