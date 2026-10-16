# ResourceManifest Snapshot binding v1 candidate (v0.4)

New writer profile is DocumentSnapshot.schema_version=3. Existing fields1..3 and old writers
are not reinterpreted; all new singular fields have explicit Edition-2024 presence:

| field | type | required meaning |
|---:|---|---|
| 1 | Id128 document_id | existing nonzero raw16 ID |
| 2 | uint32 schema_version | 3 for the new writer |
| 3 | repeated ObjectRecord objects | existing canonical objects, sorted full ID |
| 4 | ResourceManifest manifest | explicit schema1, including empty |
| 5 | uint64 document_revision | source Runtime revision, present zero allowed |
| 6 | bytes recovery_frontier | canonical RecoveryFrontierV1, not runtime generation |
| 7 | uint32 digest_algorithm | 1=SHA256_SEMANTIC_PROJECTION_V1 |
| 8 | bytes document_digest | 32 raw bytes of semantic projection hash |
| 9 | bytes capability_requirements | canonical CapabilityRequirementsV1 |
| 10 | bytes checkpoint_digest | 32 raw bytes of checkpoint hash |

Manifest fields1..4 and tombstone fields1..6 are exactly the draft proto. A tombstone stores
resource_id, retired_revision, last_content_hash, removal_operation_id, full preimage and
lineage_token. A newer active entry may coexist with an older same-ID tombstone as defined
by the operation contract. Duplicates within each collection reject; tombstone preimage ID,
kind/hash, counters and derived token must validate. No silent normalization loses presence.

## Checkpoint and frontier ownership

Capture objects, manifest and source runtime revision from one committed immutable read
view. Recovery owner supplies a matching trusted checkpoint binding (DocumentId, source
checkpoint revision, RecoveryFrontierV1) at this boundary. No owner counter is inferred from
another: OperationId, SemanticGeneration, source document_revision, CanonicalCommitStamp and
RecoverySequence are distinct. Restore validates the supplied expected binding exactly,
then establishes a NEW local RuntimeEpoch/generation baseline by the existing restore lane;
it does not install a historical source counter as the live generation or allow regression.

RecoveryFrontierV1 candidate fields: version=1, kind=1 (local ordered recovery sequence),
sequence=uint64 including present zero. Fields are all required, canonical varints; no
unknowns. This profile compares exact frontier bytes and requires contiguous continuation
positions of the same Document; zero means empty continuation prefix. Other frontier kinds
reject, pending a separately accepted extension. Transport/recovery owns allocating and
checking the sequence; this candidate does not implement storage or a server total order.

Semantic digest is defined separately in digest_projection.v1.md. Source revision/frontier
are excluded from it. Let B be canonical DocumentSnapshot protobuf bytes of fields1..9 only
(field10 omitted, all other required presence validated). CheckpointDigest =
SHA256(u32LE(len(domain)) || ASCII("axiom.checkpoint.v1") || u64LE(len(B)) || B), with domain
length 19. Field10 is exactly that 32-byte output. There is no recursive hashing. These
unkeyed hashes prove integrity/identity comparison, not authentication of external metadata.

Restore stages decode/budget validation -> version/capability -> field presence -> manifest
and object validation -> semantic digest -> expected checkpoint binding -> checkpoint digest
-> atomic staging of complete document -> one publication. Any failure preserves the previous
valid document/checkpoint and exposes no partial state. Canonical bytes need not be present
locally: a missing/corrupt blob produces derived unavailability, not a bad Snapshot.

## Compatibility matrix

| input | permission and result |
|---|---|
| schema1/2, fields4..10 all absent | explicit caller legacy-no-manifest-v1 required; preserve existing object admission including Shape2Triangle for schema2; import-only, no verified checkpoint/digest assertion |
| schema1/2, ANY fields4..10 present | reject mixed profile, even if empty; no fallback |
| schema3, any required singular field missing | reject |
| schema3, explicit empty schema1 manifest | valid if counters/caps/digests/binding valid |
| schema3, malformed manifest/token/digest or unknown required capability | reject before publication |
| unknown snapshot schema | reject |

Legacy admission normalizes in-memory manifest to absent/schema=0/revision=0/no entries/no
tombstones; capability bytes for new extension are empty. The product does not claim that
old persisted bytes carried integrity they never supplied. Caller can request an explicit
Loading-only legacy migration into schema3 with an explicit empty manifest and separately
supplied recovery checkpoint binding. Existing objects are unchanged, resources stay missing
until a real manifest Operation binds them. Migration is a schema bootstrap with before/after
projection evidence; it is not an ordinary editing/Undo write or a provider metadata fix.
A migrated document is manifest-capable, new snapshots always schema3. Historical schema1/2
Golden and current production writers stay unchanged until an authorized successor is built.
