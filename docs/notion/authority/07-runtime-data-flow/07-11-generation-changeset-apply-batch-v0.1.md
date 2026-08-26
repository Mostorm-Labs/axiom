# 07-11 SemanticGeneration / ChangeSet / ApplyBatchResult Integration Contract v0.1

> Source page: https://app.notion.com/p/3c44c57a590c815d8d3cc74bfce15196
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze

## Four namespaces

OperationId, SemanticGeneration, CanonicalCommitStamp and server revision/cursor are distinct. RecoverySequence is also a separate Shared Data Runtime recovery-order namespace. APIs/tests/persisted formats may not depend on numeric equality between them.

## SemanticGeneration

Runtime-local strictly monotonic canonical post-state token for one loaded DocumentRuntime. Create/restore establishes a local baseline generation. Every true Applied Operation creates exactly one successor generation. AlreadyApplied and Rejected do not advance generation or create ChangeSet.

SemanticGeneration may fence Scene work/stale derived work, but must not be used as a global Tile/geometry/bounds cache key.

## CanonicalCommitStamp

`CanonicalCommitStamp(runtimeEpoch, ordinal)` is bridge-visible total-order evidence for true Applied commits in one runtime epoch. Each Applied Operation correlates one after-generation and one commit stamp, but the values/semantics do not alias. AlreadyApplied/Rejected receive no new commit ordinal.

## CanonicalCommitRecord logical shape

Internal post-commit publication logically binds OperationId, ApplySource, before/after SemanticGeneration, CanonicalCommitStamp and ChangeSet. Scene consumes afterGeneration+ChangeSet+read view. Local DataBridge consumes OperationPacket+CommitStamp only for local source. External caller receives per-item apply disposition/stamp.

## ChangeSet v1

Immutable post-commit semantic-impact event describing `Gbefore → Gafter`. It is not Operation, persistence diff or Undo record.

Minimum logical content: before/after generation plus deterministic ObjectId-ordered unique object changes with flags for Created, Deleted, Placement, Transform, Properties, Content and EraseMasks; changed FieldIds may refine Properties. Empty property field detail means conservative invalidation, not unknown semantics.

Actual subtree/cascade deletions must list every deleted ObjectId. Old/new visual contribution is **not** mandatory Semantic ChangeSet data; Scene/Bounds before-state supplies old contribution.

## ApplyBatchResult

Bridge batch is scheduling/transport, not Transaction. Results are per item and mixed outcomes are legal. Each item independently returns Applied/AlreadyApplied/Rejected (and corresponding error/collision information). An earlier Applied item is not rolled back because a later batch item rejects.

Only true Applied items have new SemanticGeneration/CommitStamp/ChangeSet. AlreadyApplied/Rejected do not.

## Failure invariant

If canonical commit succeeds but incremental Scene publication cannot be trusted, Scene must detect generation gap/staleness and use full rebuild/recovery rather than pretending it is caught up.

## OPEN

Exact public C ABI packing, containers/allocators, physical recovery metadata and cross-restart dedupe retention implementation remain open.