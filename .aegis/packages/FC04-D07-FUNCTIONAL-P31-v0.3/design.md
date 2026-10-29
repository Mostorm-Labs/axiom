# Functional D07 Design Boundary — P31 v0.3

## Core model

A user intent is a transient local grouping of ordinary typed Operations. It is not a canonical/wire Transaction. The intent has one admitted basis, ordered forward Operations, prepared inverse/preimage data, staged Scene/runtime projections and a reserved History result.

## Lifecycle

```text
NEW -> PREPARED -> COMMITTED
  \       |
   \      +-> ABORTED
    +--------> REJECTED
```

- NEW: capture the document/scene generation and resolve intent ownership.
- PREPARED: validate the complete ordered operation set, stage all affected semantic/scene state, prepare inverses and reserve resources before any canonical mutation.
- COMMITTED: perform the authorized no-fail local commit/publication; create exactly one user-visible Undo unit while retaining required per-operation records.
- ABORTED: explicit caller cancellation destroys staged resources; no canonical or History/Redo mutation.
- REJECTED: invalid, stale, unsupported or unreservable intent fails before canonical mutation.

No recoverable validation or allocation failure may be deferred into a partially mutated prefix. A failure that occurs after canonical mutation begins must not be relabeled as a precommit rejection or hidden by compensation if observers could have seen partial state.

## Scene publication

Prepare the actual Scene/RuntimeScene participants before semantic commit. Commit must publish a coherent final state and preserve operation-specific metadata/notifications required by the accepted 07-14 contract. Readers/observers must not see an intermediate failed intent. If a real 07-14 consumer requires intermediate visibility incompatible with this bounded guarantee, stop with AUTHORITY_CONFLICT.

## Undo/Redo

- Success produces one user-visible Undo unit per explicit intent.
- Undo uses the exact committed target/preimage captured by the intent, independent of the current selection.
- Redo preserves the original forward meaning with fresh operation IDs.
- Independent commands remain separate History units.
- Rejected or aborted intents do not advance History cursor or destroy Redo.

## OOM scope split

The known real OOM failure in Protobuf lazy descriptor initialization is explicitly outside this package's source-change scope. This does not declare it harmless or pass it. Do not edit SDK/upstream code or remove the fault test. Functional tests may proceed separately, but overall D07/release remains blocked. An Axiom-local containment can be added only if already within the allowlist and a source audit proves every re-entry path is intercepted; otherwise report the risk and do not improvise a catch-only workaround.
