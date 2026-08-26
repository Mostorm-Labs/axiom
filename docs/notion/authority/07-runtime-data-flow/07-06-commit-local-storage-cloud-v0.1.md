# 07-06 Commit → Local Storage → Cloud Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c814ba015f9be9b95d494
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Local Durability / Sync Fork Data Flow

## Orthogonal axes

`CanonicalCommitted != Presented != LocalDurable != CloudSynced`.

Successful local commit forks independently into render/presentation and DataBridge/persistence/sync. Neither branch synchronously waits for the other.

## Ownership

Axiom Semantic Core owns successful canonical commit. AxiomDataBridge transports ordered local Operation events. Shared TS Data Runtime owns DocumentSession, local persistence, Outbox, retry/revision/cursor and Cloud sync. Product Shell only projects coarse Saved/Synced state.

Storage/sync metadata such as LocalDurable, serverRevision, cursor, retryCount and OutboxState never enters ObjectRecord or Operation envelope.

## State vocabulary

- LocalCommitted: canonical apply succeeded, not crash-safe.
- BridgePublished: local event entered async bridge delivery.
- LocalPending: Data Runtime accepted it, durable point not reached.
- LocalDurable: semantic effect is recoverable after crash.
- SyncPending/OutboxEligible.
- SyncInFlight.
- CloudSynced: protocol-qualified Cloud ACK evidence exists and is durably/reconstructably recorded.

## Bridge contract

Per-document ordered, batchable, no silent drop, async/non-reentrant delivery. JS/TS callbacks do not run synchronously from input/render threads. Public payload is opaque Operation bytes plus minimal identity metadata; zero-copy is not a public guarantee.

Bridge delivery does not imply LocalDurable. Backpressure may not silently drop canonical events or block pointer/render hot paths.

## Crash and retry

Outbox eligibility must survive restart once local state is durable. Lost ACK/crash may retry the same Operation identity; retries must not mint a new OperationId.

## Resource refinement

07-13 separates Operation durability from Blob durability/cloud availability. Document-level `Saved` and `Synced` fences additionally require closure of resource obligations introduced by local commits; an Operation ACK alone does not prove its referenced Blob is available.

## Legacy terminology

Older persistence/sync RFCs may say Transaction. Under current authority this means Operation-level canonical mutation; 07-06 does not restore global Transaction semantics.

## OPEN

Physical DB/WAL/fsync schema, Outbox tables, server revision/ACK wire semantics, Blob protocol ordering and retry/backoff remain implementation/protocol decisions.