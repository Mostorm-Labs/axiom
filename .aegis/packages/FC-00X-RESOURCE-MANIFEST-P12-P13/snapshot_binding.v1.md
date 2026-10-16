# ResourceManifest Snapshot Binding v1 candidate

This is a candidate P12/P13 binding and is not an accepted wire schema.

`DocumentSnapshot` keeps fields 1–3 unchanged and appends:

| field | type | meaning |
|---:|---|---|
| 4 | `ResourceManifest manifest` | Explicit manifest binding. Absent is legacy-only; new writes always set it, including empty. |
| 5 | `uint64 document_revision` | Monotone Runtime Document revision captured at the checkpoint. It is not the recovery frontier and is excluded from the semantic digest. |
| 6 | `bytes recovery_frontier` | Opaque, non-empty canonical frontier bytes owned by the recovery contract. It is compared byte-for-byte and is excluded from the semantic digest. |
| 7 | `uint32 digest_algorithm` | Candidate registry value `1 = SHA256_SEMANTIC_PROJECTION_V1`. |
| 8 | `bytes document_digest` | Exactly 32 bytes when field 7 is 1. |
| 9 | `bytes capability_requirements` | Canonical `CapabilityRequirementsV1` bytes. Required for capability admission and included in the digest. |`r`n| 10 | `bytes checkpoint_digest` | SHA-256 of fields 1–9 plus the canonical recovery checkpoint envelope; detects checkpoint metadata corruption without entering the semantic Document digest. |

`ResourceManifest` keeps fields 1–3 and appends `retired_entries = 4`, a sorted repeated
`ResourceManifestTombstone` collection. A tombstone contains `resource_id`, the last
accepted `resource_revision`, `content_hash`, and a canonical `lineage_token`. Tombstones
are semantic state and are included in the digest. They prevent an ordinary create or
upsert from assigning a new meaning to a removed ResourceId.

`manifest_revision` is included in the digest projection. `document_revision` and
`recovery_frontier` identify the checkpoint but are not semantic content and are excluded
from the digest. `capability_requirements` is included in the digest because it changes the
admitted semantic interpretation.

Snapshot validity is atomic:

1. fields 1–10, presence, limits and canonical ordering are validated;
2. manifest entries and tombstones are checked for duplicate IDs and revision/lineage invariants;
3. the digest is recomputed from `digest_algorithm=1` projection bytes;
4. the document revision, frontier and digest are published together, or nothing is published.

Legacy capability `legacy-no-manifest-v1` applies only to schema version 1 snapshots that
omit field 4 and therefore also omit fields 5–9. Such a snapshot decodes as an empty
manifest with no tombstones and is read-only until the next write. A new write must emit
fields 4–9, including an explicit empty manifest and capability requirements. A schema
version 2 snapshot must contain fields 4–9. A present but malformed manifest or checkpoint
never falls back to legacy mode. Unknown required capabilities, a missing field 5–9 in a
schema version 2 snapshot, or a digest mismatch reject before publication.
