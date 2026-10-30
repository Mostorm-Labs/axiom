# Product Shell and Axis consumer bridge preview

Execution base: `4fc3ebfe1325c9197024f1cd7137e54473a1c76d`. The user's explicit
request authorizes opening these existing owners to external consumers. ADR-0025
owns the separation of Shell, canonical Operation and external Data responsibilities.
ICF-01 and 07-06/12 remain historical Proposed Freeze inputs, not a claim that all
G7/G8 protocol or durability decisions have been accepted or implemented.

Shell uses existing controls, readback, selection, text and resource APIs. New
subscriptions provide operation and coarse state notifications. Notifications run
only from explicit event dispatch (also serviced by native facade frame / Web rAF),
never from pointer collection or canonical publication. Unsubscribe prevents later
delivery. Callbacks must promptly copy/transfer bytes and return; mutation/reentry
inside native callbacks rejects. Legacy construction-time `onOperation` preserves
its documented borrowed synchronous behavior for compatibility.

Axis explicitly attaches the data port before editing. The SDK retains opaque
protobuf Operations plus IDs, kind, source, before/after semantic generation and
runtime-local commit stamp. Attaching twice must not reset retained work. Encoding,
queue nodes and capacity are prepared before local canonical mutation, including
the existing explicit Intent and Undo paths. Overload rejects mutation instead of
dropping packets or waiting for disk/network. Failed preparation/aborted apply
releases its reservation. This is volatile custody, not another canonical store or
durable outbox. Axis reads packets and explicitly releases custody through an exact
operation/stamp fence after it has copied/accepted the complete prefix. Listener
success alone does not release custody. Abandon/crash still cannot preserve RAM.

Dynamic operation subscribers share this data port; subscription attaches it if
needed. All subscribers see independent ordered cursors; releasing a packet before
another subscriber reads it produces an explicit gap, never false continuity.
The data-owner application is responsible for coordinating subscribers and custody.
Queue limits are bounded configuration, not a G5 latency/performance guarantee.

Axis may report local durable/cloud synced states for exact retained operations and
a bounded window of the latest 4096 custody-released operations. These reports are external claims keyed by full
identity; Axiom does not verify database/fsync/cloud protocol or infer a document
Saved/Synced fence or resource-blob closure. The existing status projection reads
these reports; canonical bytes never contain them. Delayed/foreign/expired identities reject; Axis remains the authoritative status ledger.

Remote batches independently decode/validate/apply each packet in order, return
per-item Applied/AlreadyApplied/Rejected, preserve operation identity on retry and
never echo into the outbound local stream. No cross-operation atomicity is added.
Snapshot returns its current canonical through-commit fence; the caller still owns
documentRevision/recoveryFrontier and resource custody. Controlled close finishes
actual native composition, then rejects new canonical work and waits for retained
local packets to be released. Explicit force abandon remains separate.

Implementation ownership: LocalOperationSink gains optional prepare/abort admission
for existing local apply paths; Composition adapts its public callbacks and status;
the shared SDK Session encodes/retains packets and validates data commands; DLL and
JS only adapt subscriptions/lifetimes. No second selection, History, Document or
Scene owner. No canonical Operation format, descriptor or dependency-lock changes.

Verification must use real owners and independent exact-ID/queue/state assertions,
external Windows consumers against the DLL and actual Chrome/WASM. Synthetic input
is not physical IME/touch acceptance. Unresolved semantic decisions: none for this
bounded preview adapter; storage/protocol and cross-client Intent atomicity remain
outside its scope. Authority/verification/scope conflicts: none identified at entry.


Resource custody also exposes the existing manifest snapshot and validated opaque
resource bytes under full binding tokens, so an external data owner can enumerate
resources after restore and persist bytes introduced by import without decoding
Semantic internals. Missing/stale bytes reject explicitly. The SDK still does not
assert blob durability or silently install a network/storage provider.
