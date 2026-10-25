# FC05 capability successor review inputs

Status: **AUTHOR_FINDINGS / SUCCESSOR_REVIEW_PENDING**, not an independent verdict.
Baseline: `d6a47d53dfc6fa5381e20a528ecceb1e2878fa55`.

Role: P12/P13/P21 compatibility proposal. Authority: released schema3 framing and
the FC05 Shape compatibility successor. Objective: refuse unsupported composite
read/write/replay before publication. Non-goals: Sync negotiation, new Registry,
rewriting historical bytes or redefining existing digest algorithms. Output:
exact vectors/admission and the legacy-order finding. Gate: independent design
review plus successor positive/negative fixtures. Handoff: accepted inputs to P20.

## Source findings

`snapshot.cpp:capabilityProfile` accepts only old SnapshotCapability values 0..7.
It appends `shape2-triangle-v1` before `shape-richtext-label-v1`; ASCII sort puts
the hyphen in `shape-richtext...` before the digit in `shape2...`. A combined
triangle+label document therefore exposes an order conflict with the documented
sorted-ID rule. This is not resolved by blindly sorting every old fixture or
by accepting any order. No historical bytes/Gate record changes in this task.

The successor's required IDs are the exact sorted union in
`fc05/evidence/architecture-p20/capability-vectors.json`. Key vectors:

| Document features | Exact IDs, in order |
| --- | --- |
| schema3 with only released V1 content | resource-manifest-v1 |
| attached Shape/2 triangle | connector-shape-attachment-v1; resource-manifest-v1; shape2-triangle-v1 |
| attached Shape/3 rectangle label | connector-shape-attachment-v1; resource-manifest-v1; richtext-fixed-width-v2; shape-richtext-label-v1 |
| attached Shape/3 triangle label | previous vector + shape2-triangle-v1 at the end |
| attached Shape/3 triangle label + standalone RichText/3 | connector-shape-attachment-v1; resource-manifest-v1; richtext-fixed-width-v2; richtext-formatting-v3; shape-richtext-label-v1; shape2-triangle-v1 |

This intentionally follows existing text/format detection: do not require a new
formatting capability solely because a Shape label uses formatting already
admitted by Shape/3. Sticky's RichText/3 child does require its existing formatting
ID. New operation `sticky-edit-v1` is a **continuation/writer** requirement; it
does not belong in snapshots just because a legacy Sticky is present.

The composite ID is required iff some attached endpoint targets Shape/2 or /3;
include it for Straight as well as Orthogonal. Never infer it solely from seeing
two unrelated object kinds. Missing/extra/duplicate/unsorted/unknown IDs reject;
canonical CapabilityRequirementsV1 bytes and digest framing remain unchanged.

## Explicit local API support, not an enum shortcut

Propose a small typed source value carried by existing Semantic/Composition owners:
`RuntimeFeatureSupport { legacySnapshotCapability, connectorShapeAttachmentV1,
stickyEditV1 }`. Defaults for both new flags are false. The old overloads and old
0..7 enum values cannot opt into new combinations. This is not a stable ABI claim
or generic feature Registry.

Provide source overloads for SnapshotCodec, SnapshotBootstrapper, ReplayCoordinator
and the operation-owner admission using this value. Local implementation support
is distinct from a remote peer's claim. Known-unsupported remote writers are
rejected at the existing inbound owner boundary; absent negotiation support cannot
be called cross-client compatibility. Do not add a new collaboration protocol.

Replay must preflight capabilities over the complete selected continuation,
including staged insert/restore/upgrade effects and target resolution; absence
from the final snapshot does not prove no new operations occur earlier in the
interval. Rejection before replay leaves store/ledger/generation unchanged and
does not publish Ready. Passing preflight does not promise an entire continuation
is a single transaction: ordinary per-operation replay failures retain their
existing recovery state contract.

## Legacy order disposition requiring review

Preserve schema1/2 behavior and all historical vectors/artifacts exactly. For a
new successor schema3 writer emit sorted IDs, including mixed triangle+label with
no composite attachment. Freeze the current unsorted combination as an explicit
historical source-reality fixture and expect strict successor rejection. It must
not silently become a second accepted schema3 order or be re-signed as if old
data had always been sorted. Other already sorted legacy vectors keep their bytes.

If a real saved checkpoint using the unsorted combination is found and recovery
is required, this candidate has no automatic migration permission. Classify that
as the existing FC02 compatibility defect, preserve the bytes and design a narrow
explicit conversion/recovery path before claiming historical compatibility. This
is a concrete review finding; do not claim all legacy snapshots are readable.

Review exits: accept/reject the sorted successor and historical rejection policy,
the typed false-default admission value, complete-interval capability preflight,
and exact fixture vectors. Missing acceptance holds P20/P31; it is not an excuse
for weakening validators in product implementation.
