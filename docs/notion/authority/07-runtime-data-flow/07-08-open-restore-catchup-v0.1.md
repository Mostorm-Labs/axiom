# 07-08 Open / Restore / Catch-up Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81799137fdb6e1f97817
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Local Recovery Closure / Catch-up Data Flow

## LOCAL READY

`LOCAL READY` means the device can reconstruct a self-consistent usable canonical Document from local crash-recoverable evidence and has minimum derived RuntimeScene needed for local viewport/hit-test/frame planning.

It does not mean Surface/GPU ready, first frame presented, sync connected, remote catch-up complete or CloudSynced. If a usable local recovery chain exists, Cloud availability must not gate LOCAL READY.

## Local Recovery Closure

Logical closure consists of latest valid local Snapshot/checkpoint + ordered canonical continuation + durable remote-applied evidence + recovery-required DurableInbound reconciliation + required semantic indexes + RuntimeScene projection.

## DurableInbound recovery classes

- Recovery-Confirmed: already proven included in recovery baseline.
- Recovery-Required/Uncertain: durable, order/dependency satisfied, but applied marker uncertain; must re-drive/reconcile before LOCAL READY.
- Deferred Gap-Blocked: durable but never OrderReady because predecessor/dependency missing; does not block LOCAL READY.
- Quarantined/Rejected: never treated as canonical history; may require explicit recovery policy.

## Open flow

Open → load recovery metadata → choose latest valid checkpoint → validate/restore Snapshot → replay ordered canonical continuation with no echo → recover/classify inbound evidence → re-drive recovery-required remote Operations using same IDs → establish SemanticDocument/indexes → Full Compile/equivalent RuntimeScene → LOCAL READY → fork local viewport rendering and reconnect/catch-up in parallel.

## Replay

Replay uses the same canonical apply semantics but source=Replay, emits no local stream and does not recreate Outbox from callbacks. Startup need not render every replayed Operation; derived scene can be built from final recovered state.

## Re-drive

If a remote item may have been applied before crash but completion evidence was not durable, re-drive exact persisted bytes/OperationId. Applied or AlreadyApplied converges metadata. Rejected/collision is explicit recovery error; never guess and continue.

## Catch-up

Small gap: remote Operations through 07-07 persist-first/order/no-echo path. Large gap: candidate Snapshot + tail must be validated and durably staged before activation; pending local divergence/Outbox requires protocol-proven inclusion or safe replay/rebase/reconcile.

## LOCAL READY gate

Requires valid checkpoint, restore, continuation replay, reconciliation of all recovery-required/order-ready inbound, no unresolved closure-invalidating recovery error, semantic indexes and RuntimeScene. It does not require all Blobs materialized or all gap-blocked inbound consumed.

## OPEN

Exact recovery metadata/frontier representation, snapshot manifest/checksum schema, large-gap authority protocol and startup viewport-first optimization remain owner-specific.