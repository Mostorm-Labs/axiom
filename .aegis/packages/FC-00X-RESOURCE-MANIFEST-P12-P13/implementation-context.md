# FC-00X ResourceManifest — P12/P13 semantic binding design

## Status and boundary

This is a P12/P13 modeling package. It is not an accepted Authority record and does not authorize product implementation. It resolves the semantic decisions that blocked `FC-00X-SCENE-RESOURCE-COMPOSITION` R2, while preserving the existing Operation-only canonical write path.

## Authority inputs

- Accepted ADR-0013: `ResourceId`, `ResourceManifest`, `ResourceRevision`, versioned `ContentHash`, immutable blobs, manifest/digest separation, missing/corrupt behavior.
- Accepted ADR-0019: manifest binding is a `SemanticChange`; layout/cache/spatial effects are invalidation hints.
- Accepted ADR-0020: Snapshot includes manifest binding and digest at one committed frontier; blob availability is separate and must not invalidate a semantically valid snapshot.
- Accepted numeric/canonical rules: use versioned canonical field encoding and `sha256:<lowercase hex>` content hash.

## Canonical semantic model

`ResourceManifest` is a Document-owned, versioned semantic collection. It is not a RuntimeScene, ResourceManager, provider cache, decoded image/font, GPU object, URL, path or Host-owned registry.

```text
ResourceManifest {
  entries: ordered set<ResourceManifestEntry> keyed by ResourceId
  revision: Document semantic revision of the manifest collection
}

ResourceManifestEntry {
  resource_id: ResourceId                 // stable, non-reusable domain identity
  resource_kind: enum                    // Image, Font, Brush, or future accepted kind
  resource_revision: uint64               // monotone per ResourceId; starts at 1
  content_hash: ContentHash               // V1: sha256:<64 lowercase hex>
  byte_length: uint64                     // exact immutable blob length
  decode_version: uint32                  // canonical decoder contract version
  layout_version: uint32                  // canonical layout contract; 0 for non-layout resources
  metadata: canonical normalized bytes    // kind-specific semantic metadata only
}

ContentHash {
  algorithm: enum { sha256 = 1 }
  hex: lowercase ASCII, exactly 64 characters
}
```

The collection is canonically sorted by complete `ResourceId` bytes. Duplicate IDs are invalid. Entries with the same hash may share immutable blob bytes; the semantic entries remain distinct by ResourceId/revision.

## Validation and canonicalization

- ResourceId must be non-zero and complete 128-bit; no truncation or numeric aliasing.
- `resource_revision >= 1`; replacement of a ResourceId must use a strictly greater revision.
- Hash must match `sha256:<lowercase hex>` exactly; uppercase, alternate algorithms and URL/path forms are rejected in V1.
- `byte_length` must equal the supplied immutable blob length before a resource is `bytes_validated`.
- `decode_version`, `layout_version`, and metadata are validated against the accepted resource kind registry; unknown required kinds fail closed.
- Metadata is canonical bytes with explicit length; provider-supplied metadata cannot change the manifest entry.
- Availability state (`missing`, `bytes_validated`, `decode_ready`, `layout_ready`, `failed`, `evicted`) is derived and excluded from Document digest.

## Digest and snapshot

The Document canonical projection includes the sorted manifest entries after the normalized object graph. It includes ResourceId, kind, ResourceRevision, ContentHash, byte_length, decode_version, layout_version and metadata bytes. It excludes blob bytes, URLs, paths, provider epochs, decode/cache/GPU state and availability diagnostics.

`DocumentSnapshot` carries the manifest collection, manifest schema/version, Document identity, schema/capability, revision/frontier and digest in the same atomic checkpoint. Snapshot decode rejects duplicate IDs, malformed hashes, invalid revisions, unsupported required kinds and digest mismatch before publishing a new Document. Legacy snapshots without a manifest are accepted only under an explicit compatibility capability and produce an empty manifest; a resource reference without a manifest entry remains semantically referenced but unverified/missing.

## Operation and ChangeSet

Add one canonical manifest mutation family, `UpdateResourceManifest`, with one atomic payload containing sorted `upserts` and `removes`:

```text
UpdateResourceManifestOp {
  upserts: ResourceManifestEntry[]
  removes: ResourceId[]
}
```

Rules:

- Operation document_id/schema_version/payload_version use the existing envelope.
- Upsert is create when the ID is absent; replacement requires a strictly greater ResourceRevision and a different or explicitly reasserted hash/metadata.
- Remove is accepted only when no canonical object/content still references the ResourceId in the same atomic operation; otherwise reject without partial mutation.
- Duplicate upsert/remove IDs, non-canonical order, invalid hashes/revisions, unknown required kinds and stale revisions reject atomically.
- A successful update advances SemanticGeneration once, creates one ChangeSet object-level manifest impact record, and changes Document digest. Bytes becoming available or unavailable do not create an Operation or change Document generation/digest.
- Undo/Redo use compensating `UpdateResourceManifest` Operations; they never rewind generation or mutate a snapshot directly.
- Replay/idempotency uses the existing OperationId/document_id/fingerprint lane.

`ChangeSet` adds a semantic resource impact collection keyed by complete ResourceId with flags `kResourceManifestCreated`, `kResourceManifestUpdated`, `kResourceManifestRemoved`. Derived layout/resource/spatial hints remain outside canonical changes.

## Resource resolve boundary

The Runtime composition receives an immutable copy of the committed manifest at attach/open. A provider supplies bytes keyed by ResourceId plus expected ContentHash; Runtime verifies hash and length against the manifest, then performs decode/layout at owner safe points. Provider response metadata never mutates semantic state. A missing/corrupt/evicted blob can only produce derived diagnostics/placeholders and frame/Scene invalidation; it cannot create an Operation or change Document digest.

## Compatibility and migration

- Manifest schema version 1 and Operation payload version 1 are the only accepted V1 values.
- Legacy snapshot/schema inputs without a manifest require an explicit `legacy-no-manifest` capability. They remain readable but any referenced resource is unverified/missing until a manifest-binding Operation is applied.
- New snapshots must always carry a manifest section, including an explicit empty collection.
- Unknown future manifest kinds or metadata versions reject only when the entry is required by the document capability; optional entries remain diagnostic/unavailable under the existing capability policy.
- No public C ABI field, ObjectKind, FieldId or unrelated OperationKind is changed by this package; the exact public/canonical schema materialization remains subject to P12/P13 implementation review.

## Required oracle set

1. Full ObjectId/ResourceId identity and equal-prefix collision rejection.
2. Manifest canonical ordering, duplicate rejection and digest sensitivity to binding changes but not blob availability.
3. Hash/length/decoder-version validation and missing/corrupt/evicted transitions.
4. Atomic upsert/remove/revision conflict/replay/idempotency and compensating Undo/Redo Operations.
5. Snapshot round-trip, legacy capability read, digest mismatch rejection and no partial publication.
6. Full/Delta/Recovery Scene equivalence after manifest changes and resource refresh without Document generation change.
7. Shared resource references, object/resource same numeric bytes, provider replacement and late result isolation.

## Stop conditions

If any proposed field, Operation family, compatibility behavior or digest encoding conflicts with current Authority, stop with `AUTHORITY_CONFLICT` and route to the owning P12/P13 review. Do not silently implement a private manifest in Runtime composition.
## Exact candidate bindings for independent review

This section turns the five remaining review questions into explicit candidates. They remain candidates until P12/P13 Control Review accepts them.

### 1. Schema fields and presence

Keep existing `DocumentSnapshot` fields 1–3 unchanged. Add a single optional `ResourceManifest manifest = 4` field to `DocumentSnapshot`; absence is distinguishable from an explicit empty manifest. `ResourceManifest` fields: `uint32 schema_version = 1` (required semantic value 1), repeated `ResourceManifestEntry entries = 2`, and `uint64 manifest_revision = 3`. `ResourceManifestEntry` fields: `Id128 resource_id = 1`, `uint32 resource_kind = 2`, `uint64 resource_revision = 3`, `string content_hash = 4`, `uint64 byte_length = 5`, `uint32 decode_version = 6`, `uint32 layout_version = 7`, `bytes metadata = 8`. All required scalar/message presence is validated explicitly; entries are sorted by complete ResourceId bytes and duplicate IDs reject.

No existing snapshot field is renumbered or reused. The manifest section is optional only for legacy snapshots gated by capability; newly encoded snapshots always emit an explicit manifest, including empty.

### 2. OperationKind allocation

Existing operation payload oneof fields 1–15 and existing OperationKind values 1–15 remain unchanged. Candidate allocation: `UpdateResourceManifest = 16`, with protobuf `OperationPayload.update_resource_manifest = 16`; payload has `repeated ResourceManifestEntry upserts = 1` and `repeated Id128 removes = 2`. Operation schema_version remains 1 and payload_version is 1 for this family.

### 3. Resource kind registry

Candidate V1 numeric values: `Image = 1`, `Font = 2`, `Brush = 3`. Values 4–255 are reserved. An entry with an unknown kind is rejected when required by the document capability; optional/unused entries remain diagnostic/unavailable under the existing capability policy. Kind-specific metadata is canonical bytes, and `layout_version = 0` for Image/Brush unless the accepted kind registry says otherwise; Font layout_version is required and non-zero.

### 4. Digest encoding

Candidate digest input is a versioned length-delimited canonical stream:

```text
"axiom.document.digest.v1" (UTF-8 bytes, length u32 LE)
document_id (16 raw bytes)
document_schema_version (u32 LE)
object_projection_count (u64 LE)
canonical object projection bytes (existing G1 projection order/encoding)
manifest_schema_version (u32 LE)
manifest_entry_count (u64 LE)
for each entry sorted by complete ResourceId:
  resource_id (16 bytes)
  resource_kind (u32 LE)
  resource_revision (u64 LE)
  content_hash (u32 LE length + ASCII bytes)
  byte_length (u64 LE)
  decode_version (u32 LE)
  layout_version (u32 LE)
  metadata (u64 LE length + canonical bytes)
```

The digest stream above is a non-authoritative candidate only. The accepted G1 plan explicitly leaves product digest algorithm/width to current authority and permits verification-only SHA-256 when no product digest is frozen; its current verification tool uses FNV-1a 64 for projection evidence. This package therefore cannot claim `axiom.document.digest.v1` or SHA-256 as a product compatibility contract until the owning P12/G1 authority binds exact algorithm, width and bytes. Blob bytes, availability, URL/path, provider epoch, decode/cache/GPU state remain excluded by ADR-0013.

### 5. Legacy capability and compatibility window

Candidate capability name: `legacy-no-manifest-v1`. It permits decoding existing snapshots with no field 4 as an empty manifest and preserves existing object semantics. Such a document is readable but any ResourceId reference is unverified/missing until an accepted manifest Operation supplies an entry. New writes always emit field 4. A snapshot with field 4 present but malformed never falls back to legacy behavior. The capability is read-only migration compatibility; it does not authorize providers or Host metadata to fill the manifest silently.

### Candidate conflict checks

- Snapshot field 4 is unused in the current proto and append-only.
- Operation payload field 16 is unused in the current proto and append-only; OperationKind 16 is unused in the current enum.
- ChangeSet currently has no resource flags; candidate flags use a separate resource impact collection rather than consuming the seven existing object flag bits.
- Resource kind values are a new P12/P13 registry candidate, not inferred from ObjectKind.
- Digest bytes are a candidate and must be accepted together with G1 numeric/canonical projection authority; no implementation may assume these bytes before acceptance.

## Authority conflict audit

The package-local `candidate-binding-audit.json` records the reproducible source audit. Field 4 and payload field 16 are structurally unused, but that is not semantic acceptance. The independent v0.1 review classified the missing digest, checkpoint, registry, tombstone and ChangeSet decisions as `MISSING_CONTRACT` or `UNRESOLVED_DECISION`; v0.2 below supplies explicit candidate material for review without claiming acceptance. No product/runtime source is modified while these remain open.

## v0.2 candidate closure after independent review

The following files make every previously missing choice explicit while preserving the
candidate-only boundary:

- `snapshot_binding.v1.md`: exact candidate Snapshot fields 4–9, atomic restore order,
  manifest tombstones, and legacy presence semantics.
- `resource_kind_registry.v1.yaml`: candidate Image/Font/Brush values, metadata schema
  versions, decode/layout versions, limits and unknown policy.
- `capability_registry.v1.yaml`: candidate `legacy-no-manifest-v1` read-only migration
  capability and `resource-manifest-v1` canonical read/write capability.
- `digest_projection.v1.md`: candidate digest algorithm registry value, exact byte stream,
  inclusion/exclusion rules and projection-first oracle boundary.

### Non-reuse and compensating manifest operations

An absent ResourceId may be created only when it has never appeared in the active or retired
set. Removing an entry atomically appends a tombstone containing the last accepted revision,
hash and lineage token; an ordinary later upsert for that ID is rejected. A compensating
Undo operation may restore the exact preimage only by referencing the tombstone lineage and
using a strictly greater manifest revision. Redo applies a new operation against the new
revision. A remove that still has an object/content reference rejects the whole operation;
removing the reference and manifest entry together requires one explicitly staged atomic
operation. Tombstones remain in snapshots and digest projection until an accepted migration
or compaction authority defines safe retirement.

### ResourceId-level ChangeSet

The candidate adds a separate sorted `ResourceManifestSemanticChange` collection to the
ChangeSet, keyed by complete ResourceId, with `{created, updated, removed}` flags and
before/after entry or tombstone references. It is not represented as an ObjectId change and
never consumes ObjectSemanticChange flags. Full, delta and recovery Scene consumers consume
this collection; equal-revision full compile remains the correctness oracle. If no resource
impact exists, the collection is empty. This shape remains a candidate until P13 accepts the
exact wire-free C++ fields and merge/order rules.
