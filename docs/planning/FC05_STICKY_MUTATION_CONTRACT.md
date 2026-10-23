# FC05 Sticky layout and mutation successor candidate

Status: **HUMAN_BEHAVIOR_CONFIRMED / P12–P13_SUCCESSOR_REVIEW_PENDING**.
Source baseline: `d6a47d53dfc6fa5381e20a528ecceb1e2878fa55`.
This is a reviewable proposal, not a released operation/tag or P31 authorization.

Role: P10–P13 successor modeling. Authority: the existing Sticky child invariant,
FC02 native-text rules, and the user's confirmed overflow choice in this task.
Objective: make child text, parent growth and History agree at one commit boundary.
Non-goals: embedded Shape labels, generic transactions, new ObjectKind, Registry,
Sync protocol, or changes to FC02 historical behavior. Required analysis/output:
layout precedence, exact domain mutation, guard, errors, replay and compatibility.
Quality gate: no sequential-submit substitute for atomicity. Handoff: review this
successor before promoting the dependent P14–P18/P20 candidates.

## 1. Product and derived layout

Retain Sticky/1 dimensions and zero/one direct RichText child. RichText/1, /2 and
/3 retain their released validity rules. A user's confirmed choice is:
“沿用上述 Shape label 行为（推荐）”: fixed font size, wrap to interior width,
grow upward during input, manual shrink derives ellipsis, selected-only expansion.

Defaults selected by the design author (not a separate human decision): new box
200×200 local units; solid sRGB `#FFF59D`, alpha 1; padding 12; Body16, regular,
black, left aligned, using an explicitly admitted font resource from the existing
catalog. Public creation may supply positive dimensions, admitted fill and text
preset. Missing font binding rejects text-session entry; no system-font fallback.

Minimum tool-created/resized dimensions are 32×32, giving at least 8 interior units.
This is a product constraint, not a new Semantic minimum on legacy positive boxes.
A legacy box at or below 24 units has `ContainerTooSmall` layout, no fake glyphs or
caret, and may be expanded through the same public control.

Container width is `W-24`; a parent-context TextLayout invocation takes precedence
over the child's stored wrap_width while attached. Stored child width is preserved
byte-for-byte on parent resize and becomes effective again if detached. The child
remains its own text identity, not a synthetic embedded label. Container layout
uses actual child local/world transforms and clips to the parent's padded local
box; it does not ignore arbitrary valid transforms on imported children. New
children use identity local transform; padding is a derived layout translation.
An unsupported/singular transform produces `LayoutUnavailable`, not canonical
normalization. Font metrics, caret, selection, pointer hit and rendering consume
one snapshot. No global `setContext()` mutation may contaminate another object.

Text starts at the padded upper left, honoring persisted paragraph alignment.
Preedit changes only transient layout and may transiently grow the box; cancellation
restores presentation, without a parent mutation. Native result/confirmed input
alone can publish text and growth. Deleting text does not automatically shrink.
Enter, Ctrl+Enter, escape, outside finish, format-focus, touch/source identity and
native finish effects retain FC02 rules. Ink activation never enters text editing.
Touch double activation reuses the existing common recognizer and slop rules.

For ordinary unrotated objects growth fixes the bottom edge and moves the top
toward canvas negative Y. More generally it is parent-local negative Y: for height
increase `d`, keep the 2×2 transform axes, set `tx'=tx-c*d`, `ty'=ty-d_axis*d`,
where `c,d_axis` are the second column of the parent transform. Ancestor composition
then gives the world box. Do not promise world-up for rotated objects. New height
is `max(oldHeight, ceil(measuredUnclippedLocalHeight+24))`; reject non-finite,
unrepresentable or existing release-limit violations. Width and fonts stay fixed.
Fit-to-text uses the same measurement; a no-op produces no Operation or History.

Manual shrink clips the derived text and places a derived ellipsis on the last
visible complete line; neither truncation nor ellipsis enters the document. A
non-fitting first line reports overflow, retains the complete text, and can show
only the hint/affordance that fits. Empty hints and overflow expansion controls are
visible/hittable only on the selected Sticky (editing keeps that Sticky selected).
The existing Shape affordance visual/hit dimensions are reused; no host hit path.

## 2. Why an operation successor is needed

Current Semantic has single-operation prepared apply, and current Runtime History
submission rejects batches larger than one. Existing FC02 Shape auto-growth uses
separate text and geometry operations; that is historical implementation reality,
not a model for claiming a Sticky parent/child atomic transaction.

Propose **`EditStickyOp`**, OperationKind/OperationPayload tag **17**, envelope
schema_version=1, payload_version=1. Numeric 17 is unallocated in the inspected
baseline; it remains a proposed allocation until successor review. No generic
Transaction, new ObjectKind, SDK lock change or new Registry subsystem is required.
The current Semantic codec is repository source generated using the locked SDK;
adding this domain field does not rebuild the published dependency SDK.

Proposed wire shape (existing types imported, not production proto changes):

```proto
message EditStickyOp {
  Id128 object_id = 1;
  bytes expected_state_digest = 2;
  double width = 3;
  double height = 4;
  Transform2D transform = 5;
  oneof child_action {
    bool keep_child = 6;
    ObjectRecord create_child = 7;
    EditRichTextOp edit_child = 8;
    Id128 remove_child = 9;
    ObjectRecord restore_child = 10;
  }
}
// OperationPayload: EditStickyOp edit_sticky = 17;
```

All fields 1..5 and exactly one action are required. `keep_child` must be present
and true. Unknown fields, duplicate singular fields, invalid oneof or versions
reject through the strict wire path. Typed admission runs the same value/state
rules. Width/height use existing Sticky finite-positive semantics; transform uses
existing finite affine semantics. The operation cannot change parent placement,
fill, opacity, masks, kind/version or arbitrary unrelated objects.

The parent must be Sticky/1. The final child set must be zero or one direct
RichText/1,/2,/3 child, with existing hierarchy/resource/text rules. `create_child`
requires no current child, an eligible fresh identity, parent_id equal to this
Sticky, valid explicit placement/content/transform, and no tombstone reuse.
`edit_child` must name the current direct child and pass existing guarded delta2/3
admission (ordinary FC05 editor emits guard even for old text). It changes only
the child's admitted text state. `remove_child` must name that unique child and
deletes it through existing tombstone/delete rules; it cannot delete the parent
or silently clear a child introduced by another edit. `restore_child` requires an
empty child set and the existing tombstone/identity/restore eligibility checks;
it does not permit replacing a live record. `keep_child` may accompany a geometry
change. A semantic no-op is filtered by Runtime before ID/History allocation.

## 3. Guard and atomic publication

Let `P` be the canonical strict protobuf ObjectRecord bytes of the current parent,
and `C` those of its unique current child. For absence use the absence branch,
never an all-zero child. The aggregate guard is 32 raw bytes:

```text
D = ASCII("axiom.sticky.state.v1")
G = SHA256(u32LE(len(D)) || D || raw16(DocumentId) || raw16(StickyId)
           || u64LE(len(P)) || P || u8(child_present)
           || [raw16(ChildId) || u64LE(len(C)) || C when present])
```

Normalize/validate through existing canonical rules before hashing; do not change
any older digest algorithm. The guard covers parent/child transform, placement,
content, properties and masks, so stale whole-aggregate compensation fails closed.
It excludes route/cache revisions, resource bytes and Semantic generation. Child
font references still need current manifest admission. Operation identity dedup
precedes guard: same accepted ID/content is AlreadyApplied without a second write;
same ID with different canonical content is Collision.

Prepare parent replacement and the selected create/replace/delete child action in
one PreparedApplyPlan against a final StagedObjectView. Validate the aggregate guard,
hierarchy/cardinality, connector references to parent, resource references, text
guards, release limits, and write admission before apply. Prepare History and all
fallible allocations before publication. One accepted EditSticky creates one ledger
entry, semantic successor, commit stamp and ChangeSet covering both records.
Store/index publication cannot return failure after a partial mutation. Scene or
presentation failure after canonical acceptance reports recovery-needed, not a
false canonical rejection. Recovery rebuilds derived state without replaying text.

OperationEngine must not run a second child Operation with a second ledger ID.
Injected preparation/guard/allocation/lifecycle failures leave store, tombstones,
indices, ledger, generation, History cursor and receipts' applied status unchanged.
Commit-phase allocation is forbidden for this prepared path. Process-crash durable
transactions are outside this in-memory domain operation's promise.

## 4. Operation and History mapping

| User action | Canonical operation | Compensation |
| --- | --- | --- |
| Create empty Sticky | existing InsertObjects with full parent | DeleteObjects; Redo RestoreObjects same parent |
| Enter/cancel empty text or preedit | none | none |
| First confirmed nonempty input | EditSticky create_child, with final parent geometry | EditSticky remove_child + exact previous geometry |
| Edit existing text / format and optional auto-grow | EditSticky edit_child + final parent geometry | guarded delta restore of exact child preimage + previous parent geometry |
| Manual resize / fit-to-text | EditSticky keep_child + final parent geometry | same action with previous geometry |
| Move / fill change | existing SetTransforms / PatchProperties, one target edit | existing guarded preimage compensation |
| Delete Sticky | existing cascade DeleteObjects | one RestoreObjects containing parent, child and dependent Connector closure |

Redo of first text uses **restore_child**, not create_child on the deleted ID.
Forward/inverse IDs are freshly allocated by History; guards must bind the expected
pre/postimage. Repeated Undo/Redo preserves parent and child identities. Conflicting
remote child/parent changes reject compensation before mutation. Connector routing
adds no History or canonical mutation. Add EditSticky and SetConnectorContent to
the current History support sets, Sticky size expectations and identity-restoring
Redo rules; do not mistake one History item for several atomic Operations.

Existing low-level SetObjectSize/EditRichText remain legal with their original
single-object semantics. Such writes may derive overflow but never silently grow
the parent in Semantic. Runtime's Sticky editor and ordinary public consumer use
the domain operation when a composed edit is requested. No exposed internal write
port may bypass Lost/Closed admission or guards.

## 5. Compatibility and exact review exits

Object schema, Sticky/1 bytes and RichText/1..3 bytes remain unchanged. Older readers
can read snapshots they already understood; the child container layout is derived
presentation. Older operation readers reject tag17 rather than drop it and continue.
Advertise `sticky-edit-v1` in the existing local feature/read-write support value;
continuation containing EditSticky requires that support before Ready. It is an
operation requirement, not a gratuitous snapshot bit for every legacy Sticky.
Do not build a cross-client Sync negotiation system for this slice.

Independent review must accept/reject the proposed tag17 allocation, guard framing,
create/remove/restore eligibility, final-state validation and one-plan publication.
P20 must freeze exact positive/negative wire and typed vectors, stale aggregate,
child collision/second child, missing font, Lost/Closed, fault injection and
Undo/Redo observations. Historical FC02 two-operation evidence is not rewritten.
