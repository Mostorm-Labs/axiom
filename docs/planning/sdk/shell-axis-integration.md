# External Product Shell and Shared Data / Axis integration

The consumer SDK separates product control from data custody. One Runtime owns
one current Document/Canvas; the Shell owns Page navigation, windows and UI.
Axis owns files/database, blob custody, network, durable outbox/inbox, protocol
ordering, recovery frontiers and cloud ACK qualification. No database or network
engine is embedded in `axiom.dll` or WASM.

| Responsibility | Windows C++ | JavaScript |
|---|---|---|
| Product controls | Existing named methods / `invoke` | Existing typed methods |
| State / selection / text | `state`, `selection`, `textSnapshot` | `readState`, `readSelection`, `text.read` |
| Dynamic events | `subscribe(mask, callback)`, `unsubscribe(token)` | `subscribeOperations`, `subscribeState`; returned disposer |
| Owner-lane delivery | `dispatchEvents`; also `frame` | `dispatchEvents`; also rAF |
| Attach volatile data custody | `attachData` | `data.attach` |
| Read retained Operation | `nextOperation(after)` | `data.readNext(after)` |
| Release copied prefix | `releaseOperations(packet)` | `data.release(packet)` |
| Report external per-operation facts | `reportOperationStatus` | `data.reportStatus` |
| Remote single / independent batch | `applyRemote`, `applyRemoteBatch` | `applyRemoteOperation`, `applyRemoteOperations` |
| Local durable-log replay | `applyReplay`, `applyReplayBatch` | `applyReplayOperation`, `applyReplayOperations` |
| Snapshot / restore | `snapshot`, `Options.open` + `snapshot` | `captureSnapshot`, `createRuntime({restore})` |
| Resource discovery / byte custody | `resourceManifest`, `readResourceBytes` | `resources.list`, `resources.readBytes` |
| Resource supply / receipt | `publishResource`, `resourceReceipt` | `resources.publish`, `resources.receipt` |
| Close / interruption | `close`, explicit `abandon` | `close`; browser teardown cannot guarantee save |

`api.h` also exports `axiom_subscribe`, `axiom_unsubscribe` and
`axiom_dispatch_events`. These are additive C/POD entry points; previous config
layout and exports remain compatible. Require the matching header/DLL or matching
JS wrapper/WASM. `readBuildInfo().features.shellAxis` identifies this bridge.
This preview does not establish cross-version stable ABI certification.

## Local operation delivery

Attach data custody before editing. Optional `capacity` (1..4096 packets) and
`byteCapacity` (1..128 MiB) default to 1024 packets / 32 MiB. Reattaching does not
reset the queue. A full queue rejects a new local commit before canonical mutation
and does not advance History/redo. Axis must service custody regularly.

Each packet includes owned/copied Operation bytes and version-1 metadata:
`binding`, `operationId`, `documentId`, `kind`, `source`, `beforeGeneration`,
`afterGeneration`, and `commit {runtimeEpoch, ordinal}`. Source is LocalInteraction
0, LocalCommand 1, UndoRedo 2 or LocalAIImport 3. Id128 is lowercase hex; 64-bit
numbers remain decimal strings. The commit stamp is runtime-local ordering, not
a server revision, recovery frontier or durable acknowledgement. Remote commits
can create ordinal gaps in the local-only stream.

```js
const incoming = [];
runtime.data.attach();
const unsubscribe = runtime.subscribeOperations(packet => {
  incoming.push(packet); // bytes are already owned JS Uint8Array values
});
// Outside the listener, after copying/transferring the entire prefix to Axis:
await runtime.dispatchEvents();
if (incoming.length) {
  // Axis accepts these exact IDs/bytes into its own custody here.
  runtime.data.release(incoming.at(-1));
}
// Later, using the same full packet identity and actual external evidence:
// runtime.data.reportStatus(packet, 3, 1); // LocalDurable; SyncPending
// runtime.data.reportStatus(packet, 3, 3); // qualified CloudSynced
unsubscribe();
```

```cpp
std::vector<axiom::Response> incoming;
runtime.attachData();
auto subscription = runtime.subscribe(AXIOM_EVENT_OPERATION,
    [&](uint32_t, const axiom::Json& metadata, std::span<const uint8_t> bytes) {
        incoming.push_back({metadata, {bytes.begin(), bytes.end()}});
    });
runtime.dispatchEvents(); // or the normal runtime.frame() owner-thread loop
if (!incoming.empty()) {
    // Copy/transfer the complete prefix into the application's Axis queue here.
    runtime.releaseOperations(incoming.back().value);
}
runtime.unsubscribe(subscription);
```

Callbacks must finish promptly and synchronously; enqueue asynchronous storage or
network work outside them. Native callbacks cannot reenter Runtime; use the host's
owner-thread message queue. Copy native borrowed metadata/bytes before returning.
Callbacks run during dispatch, never during input or canonical publication. A
throwing/nonzero-returning listener retains its failed cursor; explicitly retry
dispatch after fixing the receiver. JS reports `EVENT_LISTENER_FAILED` and pauses
that listener until explicit retry. Retry preserves OperationId and bytes, so Axis
must deduplicate. Unsubscribe does not release data custody. Subscribers have
independent cursors; do not release ahead of another required subscriber. That
misuse produces `PUBLICATION_GAP`, not false continuity. A dispatch processes at
most 64 operations per subscriber; continue pumping for larger backlogs.

Construction-time `Options.onOperation` / `RuntimeOptions.onOperation` remain as
compatibility callbacks. The former is synchronous; the latter deferred. They are
not the retained/retryable data interface. Prefer the new subscriptions/data port
for persistence or replication.

## External status, replay and recovery

Progress values are Unavailable 0, Pending 1, Failed 2 and Confirmed 3. Report exact
packet identity: foreign binding, unknown/expired packet and invalid transitions
reject. Confirmed cannot be downgraded by a late report; CloudSynced Confirmed
requires LocalDurable Confirmed. Status history is a bounded projection (4096
released records plus retained packets); Axis owns the authoritative ledger.

These reports reach the existing Runtime `LocalOperationSink::status` projection.
`state.lastCommit` distinguishes applied commit, presented generation and the two
external axes. A reported per-operation ACK does **not** prove document Saved/Synced:
Axis must also close resource/blob obligations and its protocol/recovery fences.

Incoming batches carry `{operationId, bytes}`. Results are independent and ordered;
one rejected item does not hide successful predecessors or prevent later items.
Metadata-vs-byte ID mismatch rejects. Applied returns a new commit stamp;
AlreadyApplied returns no new commit. Remote application never creates outbound
local notifications. A batch is not a cross-operation transaction, durable Inbox
or collaboration conflict resolver.

Local storage recovery uses the explicit replay entry points, which invoke the
same canonical engine with `RestoreReplay` source 4. Network deliveries use
`RemoteSync` source 5. Neither path echoes new local Operations or recreates user
History. Axis must restore the matching checkpoint first, then replay only its
post-frontier continuation, in the original durable order, before enabling input.

The existing preview's numeric `disposition` remains compatible: 0 Applied,
1 AlreadyApplied, 2 Rejected, 3 CommitBlocked. Always inspect `ok` and `code` too;
decode/packet failures have no semantic disposition. These values are this SDK
adapter's existing result vocabulary, not ICF-01's proposed 1-based ABI layout.

Snapshots return `throughCommit` from the real canonical clock, including an
ordinal-zero empty fence. Axis still supplies matching generation, documentRevision
and recoveryFrontier. Restore is into a fresh Runtime, without Undo or resource
bytes. Enumerate `resources.list()` after restore, obtain each full binding token,
fetch matching hash/version bytes externally and publish them with that token.
Use `readBytes` to take custody of validated bytes already held by Runtime, such as
an imported image. Missing/stale bytes reject explicitly. Do not infer blob
durability from render/decode readiness.

Controlled close completes actual IME composition, stops new canonical work and
returns BUSY / `PUBLICATION_PENDING` until retained local packets are released.
Service the data queue and retry close. Close does not wait for network or assert
fsync. Explicit abandon, browser destruction, process crash and power loss can
discard volatile custody. The independent D07 injected-OOM risk remains unchanged.

Evidence comes from the external Windows `ExternalAxiomShellAxisBridge` test and
actual Chrome `shell_axis.html`, plus existing consumers/regressions. These are
automated owner integration checks, not physical touch/IME acceptance or Gate PASS.
