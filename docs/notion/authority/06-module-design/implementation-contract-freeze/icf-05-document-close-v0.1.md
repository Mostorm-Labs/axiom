# ICF-05 Document Close Barrier Contract v0.1

> Status: **Current Proposed Freeze Candidate — Document Close Barrier Implementation Contract**
>
> Notion source: `3c44c57a-590c-81ed-9f17-d127e3c6106d`

## Correctness target

Normal document close is gated by **recovery closure + attachment drain**. Cloud ACK, frame Presented, RuntimeScene readiness and GPU idle are not close barriers.

## Ordered Axiom close

`beginDocumentClose(doc)` enters a barrier on the same-document ordered semantic lane. On success:

1. semantic work ordered before the barrier is terminal (Applied / AlreadyApplied / Rejected);
2. true Applied mutations through the barrier have commit stamps at or before the returned CloseFence;
3. the document enters Closing and rejects new local/external semantic work with `DOCUMENT_CLOSING`;
4. local publication/durability is not implied yet.

`awaitLocalPublication(doc, fence)` completes only after every local-operation sink invocation for Local Applied commits through the fence has been delivered **and returned** on the binding executor. It does not mean storage durability.

## Shared Data Runtime recovery closure

After the local publication barrier, Shared Data Runtime establishes `RecoveryDurableThrough(fence)` so accepted state through the fence can be crash-recovered/re-driven and LocalDurable work retains durable sync eligibility.

Remote Applied work relies on the persist-first inbound rule.

Recovery closure does not imply Cloud ACK, frame Presented or GPU idle.

## Authoritative close order

```text
acquire document-closing guard
-> block new CanvasDocumentBinding
-> quiesce/resolve transient input and commands
-> beginDocumentClose
-> receive CloseFence
-> awaitLocalPublication
-> ensure RecoveryDurableThrough
-> detachAll(documentId)
-> activeBindingCount == 0
-> destroyDocument
-> finalize DocumentSession
```

## API projection

Document-close native operations reuse the DataBridge v1 functions:

```text
begin_document_close_async
await_local_publication_async
destroy_document
```

There MUST NOT be a second `AxiomDocumentCloseApiV1`. `RecoverySequence` and `RecoveryClosure` stay in Shared Data Runtime storage/control semantics and never enter Axiom native ABI.

## Destroy preconditions

Normal destroy requires Closing/CloseReady state, local publication barrier completion and zero active Canvas attachments. Normal orchestration additionally waits for Shared Data Runtime recovery durability before destroy.

Storage failure does not automatically destroy the Axiom DocumentHandle. Cloud offline may still allow normal close after local recovery closure + durable Outbox.

## Required conformance

Verify the final local Operation before close survives, delayed local sink callbacks are drained, storage failure blocks normal destroy, offline close does not wait for ACK, persist-first remote apply remains recoverable, active interaction is resolved before fence, new binding/apply is rejected after close guard, active binding blocks destroy and restart recovery reconstructs through durable recovery ordering evidence.
