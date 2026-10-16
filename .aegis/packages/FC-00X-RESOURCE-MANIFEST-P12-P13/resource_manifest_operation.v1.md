# ResourceManifest operation v1 candidate

`UpdateResourceManifestOp` is candidate OperationPayload tag 16 with envelope
`schema_version=1`, `payload_version=1`. It is one whole Operation and never a global
transaction. Candidate fields are materialized in `drafts/resource_manifest_v1.proto.txt`.

Validation and staging order:

1. `expected_manifest_revision` must equal the current manifest revision before staging.
2. `expectations` are sorted unique ResourceIds. Each state/revision must match the current
   active entry or retired tombstone; a mismatch rejects the whole operation.
3. `upserts`, `removes`, and `reactivations` have disjoint IDs and canonical ResourceId order.
4. Every upsert is validated against the ResourceKind registry and metadata schema. An ID in
   the retired set cannot be upserted; it can only be reactivated with the exact tombstone
   removal OperationId and full preimage.
5. A remove requires that the post-state object graph contains no ImageContent,
   TextStyle.font_resource_id, BrushDescriptor.texture_resource_id or BrushResourceBinding
   reference to that ResourceId. This is a read-only reference invariant, not a hidden
   cross-operation transaction. Callers must first apply any reference-removing Operation;
   a same-intent batch cannot claim atomicity across two Operations.
6. The staged manifest and tombstone set are validated, canonicalized, and applied as one
   Operation. SemanticGeneration advances once; a rejection advances neither generation nor
   ChangeSet.

Tombstone and compensation rules:

- A remove stores the complete last active entry, its content hash, the removing OperationId,
  and `retired_revision = old_revision + 1`.
- The lineage token is SHA-256 over the domain `axiom.resource.tombstone.v1` and the
  canonical fields `(resource_id, old_revision, content_hash, metadata)`; the token is not
  an externally supplied arbitrary value.
- Normal upsert after removal is rejected forever for that ID. Reactivation is a distinct
  candidate payload carrying the exact tombstone OperationId and exact preimage; it creates
  a new revision and a new OperationId. It does not erase the tombstone.
- Undo/Redo are EditorHistory compensating Operations under ADR-0014. Their history
  preimage is the complete entry/tombstone and expectation revision. Stale/concurrent
  compensation returns a conflict/rejected disposition without partial mutation.
- Tombstone retirement/compaction is outside this candidate and forbidden until a separate
  accepted migration authority supplies a recovery frontier and proof.

The operation digest/fingerprint includes all payload fields, including expectations and
preimages. Same OperationId with different payload is a collision; same ID/equal canonical
payload is AlreadyApplied.
