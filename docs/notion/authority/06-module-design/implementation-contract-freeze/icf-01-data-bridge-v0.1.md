# ICF-01 AxiomDataBridge vNext Contract v0.1

> Status: **Current Proposed Freeze Candidate — AxiomDataBridge vNext Implementation Contract**
>
> Notion source: `3c44c57a-590c-81b8-b47b-cf3c12f1ec2b`

## Ownership and scope

AxiomDataBridge is the boundary between Shared Data Runtime and the Axiom semantic/document runtime. It owns document lifecycle, Snapshot restore/create, external Operation apply, local canonical Operation publication, canonical commit-order evidence and the close publication barrier.

It MUST NOT expose RuntimeScene, Tile, Skia, Surface, Pointer, Product UI or server revision/cursor/ACK semantics.

Legacy `Transaction*` naming is superseded.

## Core semantics

- `DocumentHandle` is process-local and opaque.
- Snapshot and Operation payloads cross as opaque bytes.
- `OperationPacket.id` must match the OperationId encoded in the bytes.
- `CanonicalCommitStamp { runtimeEpoch, ordinal }` is a runtime-local canonical ordering identity, not a server revision/cursor or SemanticGeneration.
- first true Applied mutation in an epoch uses ordinal 1; ordinal 0 is legal as an empty fence.

Apply dispositions:

```text
APPLIED = 1
ALREADY_APPLIED = 2
REJECTED = 3
```

Rules:

- APPLIED means one atomic canonical mutation occurred and receives a new commit stamp.
- ALREADY_APPLIED means no mutation, ChangeSet, local event or new ordinal.
- REJECTED means no mutation or ordinal.
- external/replay/remote `applyOperations()` never echoes APPLIED Operations to the local operation stream.
- batches can mix outcomes; each Operation remains independently atomic and batch is not Transaction.

## Surface

Conceptually the bridge provides:

```text
createDocument
restoreSnapshot
createSnapshot
applyOperations
subscribeLocalOperations
beginDocumentClose
awaitLocalPublication
destroyDocument
```

Same-document external apply calls execute in invocation order. The local operation stream is per-document canonical order for Local source only, lossless, delivered away from Input/Render hot paths and non-reentrant to the initiating JS call.

## Memory/lifetime

- async input byte spans remain immutable until completion returns unless implementation copies/retains earlier;
- callback result arrays are callback-duration values and wrappers copy/convert before return;
- owned snapshot buffers use explicit release semantics;
- public contract does not promise zero-copy.

## Error/close behavior

New semantic work after the ordered close barrier enters Closing is rejected with `DOCUMENT_CLOSING`. Illegal destroy returns an explicit state/status error rather than silently discarding work.

## Required conformance

At minimum verify stale handles, 16-byte Id128, packet metadata-vs-bytes mismatch, mixed apply results, remote/replay no-echo, AlreadyApplied no ordinal, local stream ordered/lossless delivery, callback non-reentrancy, output lifetime, snapshot throughCommit and close publication barrier.
