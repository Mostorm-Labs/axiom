# 07-12 Sync Revision / Cursor / ACK + RemoteApplied Frontier Integration Contract v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81498611e1c87d6e694f
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze

## Revision is not one thing

OperationId, CanonicalCommitStamp, RecoverySequence, SemanticGeneration, ProtocolPosition/server cursor, DurableReceiveFrontier, RemoteAppliedFrontier and OutboundAckFrontier are separate namespaces/facts.

## Protocol-neutral position

07 does not choose total revision vs causal frontier. A ProtocolPosition is opaque; frontier coverage means protocol-complete coverage, not numerically largest seen revision.

A receive cursor may conservatively lag but must never advance past a gap. Sparse durable inbound items do not imply a contiguous/protocol-complete DurableReceiveFrontier.

## Per-item remote facts

DurableInbound and OrderReady are independent. Out-of-order data may be durable but not order-ready. Axiom dispatch requires `DurableInbound && OrderReady && !Quarantined`.

## DurableReceiveFrontier

Represents protocol-complete crash-safe receive coverage. Resume/subscription cursor must not cover beyond it, otherwise missing predecessors could be lost on reconnect.

## RemoteAppliedFrontier

Represents protocol-complete remote items with positive semantic disposition (Applied/AlreadyApplied) **and durable recovery evidence** sufficient to reproduce/prove completion. An in-memory Applied callback is not enough.

RemoteAppliedFrontier cannot advance over Rejected/collision/unresolved protocol errors and is covered by DurableReceiveFrontier.

Marker-first-before-apply is illegal. Apply success followed by crash before marker is legal and converges through durable Inbox + idempotent re-drive.

## ACK semantics

Do not use one generic ACK:

- InboundReceiptAck: client→server, based on DurableReceiveFrontier.
- InboundAppliedAck: client→server if protocol requires semantic consumption, based on RemoteAppliedFrontier.
- OutboundCommitAck: server→client for local outbound acceptance.

Wire protocol may omit some concepts, but may not conflate their meaning.

## CloudSynced

A local Operation is CloudSynced only when protocol-qualified outbound ACK evidence exists and is durably recorded or guaranteed reconstructable. Socket write/request sent/Outbox pop/connection alive are insufficient.

Crash after server acceptance but before durable ACK evidence leads to conservative retry with same OperationId.

## Local sync fence

RecoverySequence may define a local fence for replication-eligible local commits; CloudSyncedThrough means all eligible local commits through that recovery fence have durable qualified ACK evidence. It is not a server revision.

## OPEN

Actual server ordering model, cursor encoding, AXTP messages, DB schema, conflict/rejection policy and large-gap threshold remain protocol/implementation decisions.