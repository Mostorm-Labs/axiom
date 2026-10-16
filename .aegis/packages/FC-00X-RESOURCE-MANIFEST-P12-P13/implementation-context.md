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
