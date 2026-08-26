# 07-07 Remote → Apply Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c8177a8a4dde14e4623f2
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Remote Durability / Ordered Apply Data Flow

## Core decision: persist first, apply second

For every remote Operation R:

`CanonicalApplied(R) => DurableInbound(R)`.

Before remote bytes may affect SemanticDocument, Shared Data Runtime must durably store the bytes plus enough ordering/recovery metadata to re-drive/reconcile after crash. Network receipt or in-memory queueing is not the durable point.

This avoids an apply-first crash window where a remote dependency becomes visible, a dependent local edit becomes durable, then restart loses the remote prerequisite.

## Durable inbound is not semantic acceptance

Data Runtime owns crash-safe receipt, not semantic validation. A remote item can be durable but later rejected by Axiom. Rejected/quarantined bytes must never be mistaken for accepted canonical continuation during recovery.

## Ownership

Data Runtime: transport, revision/cursor interpretation, gap/order buffering, transport dedupe, durable inbound. Axiom: decode/semantic validation, OperationId idempotency, atomic Document mutation. Remote successful apply triggers ChangeSet/Scene/Render but **NO LOCAL ECHO**.

## Dispatch gate

Only protocol-order-qualified and DurableInbound items may be dispatched to Axiom. Gap/out-of-order handling belongs to Sync Runtime, not OperationEngine.

07-12 refines this to `CanDispatch = DurableInbound && OrderReady && !Quarantined`; DurableInbound and OrderReady are independent facts.

## Double idempotency

Data Runtime dedupe reduces transport/persistence duplicates. Axiom OperationId idempotency remains final canonical correctness authority. Retry uses exact persisted bytes and the same OperationId.

## Apply outcomes

Applied/AlreadyApplied may advance remote-applied evidence according to 07-12. Rejected/collision must enter explicit error/quarantine/rebootstrap policy; never silent skip.

## Small/large gap

Small gaps use persisted incremental Operations. Large-gap Snapshot replacement is allowed only after durable staging/validation and must respect pending local divergence/Outbox reconciliation; 07-08 owns recovery/activation guards.

## Interaction

Active sessions do not veto remote canonical apply. 07-14 freezes remote-first apply followed by ChangeSet×session-footprint transient conflict handling.

## OPEN

Server ordering model, cursor/ACK encoding, gap threshold, CRDT/OT/conflict policy, physical Inbox schema and resource protocol remain outside this contract.