# ResourceManifest operation v1 candidate (v0.4)

The candidate payload and all canonical field numbers/types are in
`drafts/resource_manifest_v1.proto.txt`. OperationKind and OperationPayload tag16 extend a
successor registry profile named `axiom.operation-registry.resource-manifest-v1` which
imports existing frozen operations 1..15 unchanged. No old registry or descriptor is edited
by this design package. Old clients do not understand operation16 and must fail closed;
manifest-capable callers explicitly opt into `resource-manifest-v1`. Envelope schema/payload
versions remain 1/1. No hidden transaction is added.

## State and validation

A manifest has schema_version=1, manifest_revision=0 initially, sorted active entries and
sorted latest tombstone per ID. An ID is never-used iff absent from BOTH collections.
An active entry and a historic tombstone can share an ID only if entry.resource_revision >
tombstone.retired_revision. Their resource_kind must agree. A retired ID has only a tombstone.
A retirement tombstone is replaced by the next retirement for that ID, never erased by
reactivation. All entries, including tombstone preimages, satisfy the metadata registry.

Payload actions upsert/remove/reactivate are nonempty in aggregate, sorted, unique and
mutually disjoint by complete 128-bit ResourceId. The union of their IDs equals the sorted
unique expectations IDs, with one required state and revision per ID. Never-used expectation
is (state=0,revision=0), active is (1,current resource_revision), retired is
(2,current retired_revision). `expected_manifest_revision` must exactly match current state.
Duplicate/cross-action IDs, empty target sets, omitted presence, mismatched fences, invalid
metadata, overflow or any failed target reject the entire Operation.

1. Existing OperationId idempotency compares the full canonical envelope/payload FIRST:
   AlreadyApplied returns without checking now-stale fences, generation or ChangeSet.
   Equal ID/different canonical payload is collision.
2. New OpId: normalize and validate payload, expectations, reference invariants, counters
   and the complete staged resulting manifest before any mutation.
3. Create upsert is legal only for never-used ID, resource_revision=1.
4. Update upsert requires active ID, same resource_kind, revision=old+1. Unchanged content
   with a new revision is allowed and changes semantic digest. Retired IDs cannot upsert.
5. Remove requires active ID and NO reference in the entire current object graph. Check
   ImageContent.resource_id, every TextStyle.font_resource_id, legacy BrushDescriptor texture,
   and captured BrushResourceBinding resource_id (Vector and Dab snapshots). There is no
   simultaneous object editing in this payload. Apply reference-removing Operations first;
   if that independent step succeeds but remove fails, its result remains committed under 07.
6. Remove atomically stores preimage=old active entry, retired_revision=old+1,
   last_content_hash=old.content_hash, removal_operation_id=this OperationId, lineage_token
   as below, and removes the active entry. It replaces any older same-ID tombstone.
7. Reactivate requires retired state; `expected_removal_operation_id=2`,
   `expected_preimage=3` and `expected_lineage_token=4` must equal the complete CURRENT
   tombstone values. New entry must
   match preimage in ID/kind/hash/length/decode/layout/metadata and use revision=retired+1.
   It adds the active entry and RETAINS the tombstone; it never creates a new identity.
8. Any counter UINT64_MAX requiring successor rejects. Every Applied manifest Operation
   increments manifest_revision once and SemanticGeneration once, emits one ChangeSet and
   retains existing commit/source-dependent no-echo rules. Other Operations do not increment
   manifest_revision. Availability events advance neither semantic counter.

Content references can be missing/unbound without local bytes, under the existing legacy
policy. New references to retired entries reject until reactivation. An existing captured
BrushResourceBinding expected hash/role/decode contract must still match a proposed active
binding: an update must not silently change captured brush semantics. For such a brush
replacement use a fresh ResourceId or remove its references first. Image/Font binding
replacement refreshes derived resources; intrinsic object dimensions and LayoutContext do
not change without their existing semantic Operations/external layout policy.

## Lineage and compensation

Tombstone canonical fields1..5 contain all identity/preimage data. Let `T` be their exact
canonical protobuf bytes with field6 omitted. Domain `axiom.resource.tombstone.v1` is 27 ASCII
bytes. Token = SHA256(u32LE(27) || domain || document_id raw16 || u64LE(len(T)) || T).
Validate this token on restore/prepare. It is an integrity/correlation token, not an
unforgeable authorization proof. `ReactivateResource.expected_lineage_token=4` contains
these 32 bytes; whole-operation validation includes all three expected fields.

EditorHistory stores controlled preimage/postimage, successful removal identity/token and
current revision fences. Undo create = remove if still applicable/no references; Undo update
= new upsert restoring all original content fields with current revision+1, only if current
binding equals the authored postimage; Undo remove = exact reactivation above. Redo is a new
compensation with fresh OperationId/fences, never reusing the original payload/OpId. Stale
manifest_revision, later retirement or unrelated edit yields explicit conflict/rejection.
Multiple resources in one Operation prepare all preimages atomically. Missing history returns
HistoryUnavailable without mutation. No session-history persistence or multi-op atomicity is
claimed. Latest retired preimage is canonical tombstone state and survives Snapshot/replay.
Tombstone deletion, ID reuse and retirement compaction are forbidden in this version.
