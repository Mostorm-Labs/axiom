# Semantic document digest v1 candidate (v0.4)

Candidate algorithm registry1 is SHA256_SEMANTIC_PROJECTION_V1, output32 raw bytes, lowercase
hex64 only for presentation. It is a new P12 proposal, not promotion of verification-only
G1 FNV/SHA evidence. Projection bytes/semantic equality remain the primary proof oracle.

`u32`/`u64` below are checked unsigned little-endian fixed-width values; bytes16 is the raw
Id128.value (no protobuf wrapper). Domain length is computed from ASCII bytes (28). Each
protobuf blob has canonical numeric tag order, minimum varints, explicit required presence,
sorted keyed collections, existing finite numeric normalization, no map/unknown/duplicate
singular fields. Candidate messages/metadata use the exact draft proto and registry fields.
Do not hash native object memory or the G1 JSON projection; existing ObjectRecord protobuf
bytes are the complete accepted canonical representation, including full Brush snapshots.

```text
u32(28) + ASCII("axiom.semantic.projection.v1")
bytes16(document_id)
u32(document_schema_version)
u32(capability_requirements_length) + canonical CapabilityRequirementsV1 bytes
u8(manifest_presence)                   # 0 legacy absent, 1 explicit
u64(object_count)
for each normalized ObjectRecord sorted by full ObjectId:
  u32(object_length) + complete canonical ObjectRecord protobuf bytes
u32(manifest_schema_version)
u64(manifest_revision)
u64(active_entry_count)
for each active ResourceManifestEntry sorted by full ResourceId:
  u32(entry_length) + complete canonical ResourceManifestEntry protobuf bytes
u64(tombstone_count)
for each latest ResourceManifestTombstone sorted by full ResourceId:
  u32(tombstone_length) + complete canonical ResourceManifestTombstone protobuf bytes
```

For manifest_presence=0 EXACTLY: schema_version=0, manifest_revision=0, both counts=0,
capability_requirements_length=0 (empty bytes); all four scalars/counts are STILL ENCODED,
not omitted. document_schema_version is the old snapshot1 or2. This is a comparison projection
for admitted legacy data, not a claim old files carried a digest. Explicit empty profile has
manifest_presence=1, manifest_schema_version=1, manifest_revision=0, counts0 and schema3
required capability bytes. The two projections intentionally differ. No aliasing migration.

Manifest_revision and all active/tombstone content, preimage, removal ID and token affect
semantic digest. For schema3, canonical capabilities MUST include resource-manifest-v1 and MUST include
shape2-triangle-v1 exactly when the admitted object set requires Shape2Triangle. Legacy
schema1/2 projections keep capability_requirements_length=0 because their SnapshotCapability
admission is external to the old wire bytes; the registry maps that external value to
shape2-triangle-v1 but does not invent a legacy capability field. Required message scalars, including present zero decode/layout
or collection_index, never default-elide. Missing required metadata fails validation, not a
second hash encoding. Empty repeated collections omit protobuf occurrences, but stream counts
above remain present. No optional candidate entry scalars exist.

document_revision, recovery_frontier, checkpoint_digest, URLs/paths, blob bytes, availability,
provider epoch and decode/cache/GPU state are excluded. Their runtime changes cannot mutate
the semantic projection. checkpoint_digest separately binds nonsemantic checkpoint fields
as defined in snapshot_binding.v1.md. Verify semantic digest before publishing any restore.
