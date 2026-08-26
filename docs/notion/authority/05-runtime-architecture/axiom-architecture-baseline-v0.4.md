# Axiom Architecture Baseline v0.4

> Status: **Current Architecture Authority — Authority Reconciliation + Root Governance Edition**
>
> Notion source: `3c84c57a-590c-8148-82f9-f1f809752346`
>
> Supersedes: `axiom-architecture-baseline-v0.3.md`
>
> Snapshot date: 2026-08-26

## Change reason

v0.4 reconciles legacy terminology and root navigation with current 04/06/07 authority. It is not an architecture redesign. In particular, the old `Transaction`-named canonical/data-bridge language is superseded by the Operation-only model, and historical Persistence/Sync RFCs no longer act as current implementation authority.

## Canonical truth

- `SemanticDocument` is the sole persistent/synchronized/replayable Canvas canonical truth.
- **Operation is the only canonical mutation unit.** There is no global `Transaction -> operations[]` canonical layer.
- A whole Operation is atomic. A batch is ordered delivery / bridge amortization only and does not create global rollback or cloud atomicity.
- Undo/Redo emits compensating Operations rather than rewinding the document or operation log.
- RuntimeScene, bounds, spatial index, render groups, tiles, GPU/Skia resources, selection, viewport, Arc preview and sync cursors are derived/non-canonical state.

## Runtime ownership

Axiom owns interaction/ink semantics, SemanticDocument, OperationEngine, history semantics, SceneCompiler, RuntimeScene, bounds/spatial/hit-test, Render Core and canonical rendering.

Axiom does not own product account/navigation, local/cloud storage orchestration, outbox/inbox/server cursors, cloud protocol, or Arc platform backend ownership.

At the same canonical revision, incremental RuntimeScene compilation MUST be equivalent to a full rebuild.

## Arc boundary

Arc owns native input acquisition/normalization, transient preview presentation and platform latency/capability diagnostics. Axiom owns brush/prediction/erase/connector/snap semantic decisions. `pointer-up` is not sufficient to clear preview: canonical commit -> canonical render/presentation proof -> `CanonicalVisible(token)` -> Arc clear.

## Shared TypeScript Data Runtime

The Shared Data Runtime owns DocumentSession, repository, Snapshot/Operation journal orchestration, outbox/inbox, durability, sync frontier/cursor, blob lifecycle, AXTP client and suspend/resume/reconnect/catch-up. It stays out of pointer/render hot paths.

The state axes remain distinct: `CanonicalCommitted != LocalDurable != CloudSynced`.

## DataBridge current direction

Current implementation-facing DataBridge authority is ICF-01 v0.1:

- process-local opaque `DocumentHandle`;
- opaque Snapshot/Operation bytes;
- `applyOperations(...)` and `subscribeLocalOperations(...)` are separate;
- external apply/replay/remote produces **no local echo**;
- batch is not Transaction;
- callbacks are async/non-reentrant;
- public contract does not promise zero-copy;
- `CanonicalCommitStamp`, server cursor/revision and SemanticGeneration are separate namespaces.

## Remote apply path

```text
Cloud bytes
-> Shared Data Runtime
-> dedupe / ordering / gap
-> DurableInbound
-> OrderReady
-> AxiomDataBridge.applyOperations
-> OperationEngine atomic apply
-> SemanticDocument / ChangeSet
-> NO LOCAL ECHO
```

DurableReceiveFrontier, RemoteAppliedFrontier, outbound ACK/CloudSynced, CatchUpComplete and UpToDate are distinct semantic axes.

## Platform boundary

Web / Windows / Android / Apple share common semantic contracts while physical realization may differ. Platform-specific code must not redefine canonical semantics or alias physical callbacks/GPU fences/server cursors into canonical revision identities.

## Authority routing

- 00–04: product/semantic/wire authority
- 05–06: ownership/module/implementation contract authority
- 07: canonical runtime data-flow/recovery/sync-frontier authority
- 08: platform ABI/thread/lifecycle/surface authority
- 09: performance engineering/benchmark-driven choices
- 10: executable verification/proof
- G0–G9: execution authority and gate evidence

When historical ADR/RFC/Baseline content conflicts with current authority, **current authority wins**.

## Non-changes

Axiom / Arc / Shared TS Data Runtime / Product Shell / Platform Host ownership remains unchanged; SemanticDocument remains the sole canonical truth; RuntimeScene/Spatial/Tile/GPU remain derived; React Web + React Native shell direction remains; Arc preview stays separated from canonical rendering; storage/sync remains off pointer/render hot paths.
