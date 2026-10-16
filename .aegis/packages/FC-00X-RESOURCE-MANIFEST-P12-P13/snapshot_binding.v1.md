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
| 9 | `bytes capability_requirements` | Canonical sorted capability identifiers. Required for capability admission and included in the digest. |

`ResourceManifest` keeps fields 1–3 and appends `retired_entries = 4`, a sorted repeated
`ResourceManifestTombstone` collection. A tombstone contains `resource_id`, the last
accepted `resource_revision`, `content_hash`, and a canonical `lineage_token`. Tombstones
are semantic state and are included in the digest. They prevent an ordinary create or
upsert from assigning a new meaning to a removed ResourceId.

Snapshot validity is atomic:

1. fields 1–9, presence, limits and canonical ordering are validated;
2. manifest entries and tombstones are checked for duplicate IDs and revision/lineage invariants;
3. the digest is recomputed from `digest_algorithm=1` projection bytes;
4. the document revision, frontier and digest are published together, or nothing is published.

Legacy capability `legacy-no-manifest-v1` allows field 4 to be absent and treats it as an
empty manifest with no tombstones. It does not allow fields 5–9 to be omitted from a new
snapshot. A present but malformed manifest or checkpoint never falls back to legacy mode.
