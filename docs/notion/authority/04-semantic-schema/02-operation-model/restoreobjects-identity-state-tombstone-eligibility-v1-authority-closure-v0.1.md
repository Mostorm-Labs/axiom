# RestoreObjects Identity-State / Tombstone Eligibility V1 Authority Closure v0.1

> Source: Notion `RestoreObjects Identity-State / Tombstone Eligibility V1 Authority Closure v0.1`
> Source page id: `3ca4c57a-590c-8150-b3bd-cb1d51eb0b83`
> Source status: **Current Authority**
> Repository status: current
> Stage: P13 Operation / Mutation Model
> Scope: GT-G1-04-B RestoreObjects identity-state eligibility

## 1. Canonical rule

RestoreObjects eligibility is determined by the current canonical ObjectStore apply-base state plus complete staged resulting-state validation:

```text
RestoreObjects eligibility
= current canonical ObjectStore apply-base state
+ complete staged resulting-state validation
```

It does not depend on tombstone proof, deletion epoch, deletion generation, Editor History proof,
Shared Data Runtime metadata, server revision, sync cursor, Inbox state, or Outbox state.

## 2. No semantic tombstone requirement

V1 does not require a semantic tombstone or deleted-ID ledger in SemanticDocument, ObjectRecord,
the RestoreObjects payload, PreparedApplyPlan eligibility proof, or Snapshot semantic state.

Storage or Sync may own their own tombstone metadata in the future. Such metadata must not change
the Axiom result for the same canonical apply-base state and the same Operation.

## 3. Producer intent and runtime predicate

InsertObjects introduces a newly allocated identity. RestoreObjects re-introduces a previously
removed identity as producer/history semantic intent. “Previously removed” does not require
OperationEngine to retain ever-seen-ID historical truth.

## 4. New-OperationId identity predicate

For a new OperationId, each RestoreObjects candidate ObjectId must be absent from the current
canonical ObjectStore state and from the staged resulting state. If any candidate ObjectId already
exists, the complete operation is rejected as ObjectAlreadyExists.

This rule applies when the existing ObjectRecord is byte-for-byte or semantically equal to the
candidate as well as when it differs:

```text
NO overwrite
NO upsert
NO semantic no-op
```

## 5. Idempotency ordering

RestoreObjects uses the normal OperationId gate before Restore state validation:

```text
OperationId gate
        ↓
Restore state validation
```

- Same OperationId plus equivalent payload returns AlreadyApplied and stops before existence
  validation.
- Same OperationId plus different payload returns ProtocolCorruption / OperationIdCollision and
  stops.
- A new OperationId whose objects were already restored is rejected as ObjectAlreadyExists.

## 6. Complete staged graph validation

RestoreObjects is whole-operation staged validation, never per-record mutation/validation.

- A parent and child restored together are valid when their resulting hierarchy is valid.
- A target and its cascaded Connector restored together are valid when their resulting staged
  references are valid.
- A Connector restored alone while its target is absent is rejected as InvalidReference.

## 7. ApplySource parity

For the same canonical apply-base state and the same RestoreObjects, LocalUndo, Replay,
RemoteSync, and RemoteRecovery produce the same semantic validation result. ApplySource may
change downstream History, publication, or no-echo behavior; it does not change RestoreObjects
acceptance.

## 8. Snapshot boundary

`restore_snapshot()` is a Loading-only privileged bootstrap through SemanticWritePort. It is not
RestoreObjects and Snapshot semantic state does not need a deleted-ID ledger. Canonical
continuation replay subsequently returns to normal OperationEngine processing.

## 9. Implementation boundary

This authority closes only GT-G1-04-B RestoreObjects identity-state eligibility. It does not
authorize GT-G1-04-A changes, GT-G1-04-C implementation, Atomic Apply, SemanticGeneration,
ChangeSet, History commit, DataBridge, Outbox publication, or GT-G1-05.
