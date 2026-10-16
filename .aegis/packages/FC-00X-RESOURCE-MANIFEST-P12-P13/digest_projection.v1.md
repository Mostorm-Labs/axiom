# Semantic document digest v1 candidate

The product digest candidate is `SHA256_SEMANTIC_PROJECTION_V1` (registry value `1`),
with a 32-byte SHA-256 output rendered as lowercase hexadecimal only at textual API
boundaries. It is a candidate binding for independent review; it does not promote the
G1 verification-only FNV-1a value or an evidence SHA-256 into product authority.

The hash input is the following length-delimited little-endian byte stream:

```text
u32(28) + UTF-8("axiom.semantic.projection.v1")
bytes16(document_id)
u32(document_schema_version)
u32(capability_requirements_length) + capability_requirements
u8(manifest_presence)                 # 0 legacy-empty, 1 explicit
u64(active_object_count)
for each canonical ObjectRecord sorted by complete ObjectId bytes:
  u32(object_record_bytes_length) + canonical ObjectRecord protobuf bytes
u32(manifest_schema_version)
u64(manifest_revision)
u64(active_manifest_entry_count)
for each ResourceManifestEntry sorted by complete ResourceId bytes:
  u32(entry_bytes_length) + canonical ResourceManifestEntry bytes
u64(retired_tombstone_count)
for each ResourceManifestTombstone sorted by complete ResourceId bytes:
  u32(tombstone_bytes_length) + canonical ResourceManifestTombstone bytes
```

`document_revision`, `recovery_frontier`, provider epoch, URL/path, blob bytes,
availability, decode/cache/GPU state and transport metadata are excluded. Manifest
presence, manifest_revision, active bindings, tombstones and capability requirements are included. The
canonical protobuf bytes use the existing profile: numeric tag order, explicit presence,
finite f64 normalization, sorted keyed collections and no protobuf map iteration.

The digest is recomputed before Snapshot publication and compared on restore. Projection
bytes and semantic field equality remain the primary oracle; the hash accelerates equality
and detects corruption after the algorithm is accepted.

The `u32` before the domain string is the exact UTF-8 byte length, not a character count.
Every length-delimited field uses unsigned little-endian byte length; zero-length metadata
is represented by an explicit zero length and is distinct from an absent optional field.
`ResourceManifestEntry` and `ResourceManifestTombstone` bytes are canonical protobuf
encodings under the accepted profile, with all semantically required scalar/message
presence validated before encoding. A present default is never elided unless a future
accepted registry explicitly declares it equivalent to absence.
