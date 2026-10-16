# ResourceManifest P12/P13 v0.4 candidate

This is a proposed semantic extension, not an accepted Authority or P31 implementation
permission. It replaces the current candidate prose; historical candidates remain available
at their exact commits and independent-control-review-v0.1/v0.2/v0.3.json remain unchanged.
No product, schema, published SDK, Golden, Gate or PHYS implementation/state is changed.

## Trusted constraints and candidate ownership

ADR-0013 owns stable ResourceId, immutable sha256 content and manifest semantic state.
ADR-0014 owns compensating Operations and controlled preimage. ADR-0019 owns semantic
ChangeSet separation. ADR-0020 owns checkpoint/continuation atomic restore; ADR-0026 owns
semantic numeric representation. Existing canonical protobuf/profile/hard limits remain
applicable to existing ObjectRecord bytes. A new product digest, ResourceKind registry,
Snapshot schema 3 and OperationKind 16 are proposals here, not facts inferred from code.
The G1 verification FNV/SHA hashes remain verification tools only.

Read package.json, this context, execution-contract.json, then these precise candidate
contracts: drafts/resource_manifest_v1.proto.txt, operation_registry.resource_manifest.v1.yaml,
resource_kind_registry.v1.yaml,
resource_manifest_operation.v1.md, snapshot_binding.v1.md, capability_registry.v1.yaml,
digest_projection.v1.md and change_set_contract.v1.md. The proto text defines field
numbers/types; the corresponding named contract defines validation and behavior. They
must agree: a contradiction blocks acceptance rather than being resolved by precedence.

## Model and mutation

Document owns ResourceManifest schema 1: active entries, manifest_revision (initial 0),
and latest retirement tombstone per ResourceId. An entry fields 1..8 is the complete
binding (ID/kind/revision/hash/length/decode/layout/metadata). Tombstone fields 1..6 are
resource_id, retired_revision, last_content_hash, removal_operation_id, complete preimage,
and derived lineage_token. Retirement records survive reactivation: an active entry may
coexist with an older same-ID tombstone only under the revision constraints in the operation
contract. IDs are never treated as new after retirement. Ordinary create cannot bypass this.
Missing bytes and decoded/cache/provider state are derived and never enter this model.

UpdateResourceManifest is candidate OperationKind/oneof tag 16. Payload fields are
upserts=1, removes=2, expectations=3, reactivations=4, expected_manifest_revision=5.
Envelope schema_version=1/payload_version=1; existing operations 1..15 are unchanged.
There is exactly one expectation per targeted ID and at most one action per ID. ReactivateResource carries expected_removal_operation_id=2, expected_preimage=3 and expected_lineage_token=4; all three must equal the current tombstone. Prepare
checks the existing idempotency lane and fences before atomic publication. Live references
prevent removal. Reference-removing Operations must finish first; there is no cross-operation
transaction or implicit cascade. Undo/Redo uses new Operations/current-state fences, exact
controlled preimages and latest removal identity; stale compensation fails without mutation.

## Snapshot, digest and compatibility

Snapshot schema 3 keeps existing fields 1..3 and adds fields 4..10. New writers always emit
all singular fields, explicit empty manifest/capabilities where permitted, semantic digest,
and checkpoint_digest. Both digests have separately specified exact byte streams. Semantic
digest includes manifest presence, revision, active bindings, retirement tombstones and
required capabilities; checkpoint_digest additionally binds runtime revision and the recovery
frontier. Runtime generation, source checkpoint revision and RecoverySequence do not alias.
The candidate capability registry maps existing SnapshotCapability::kShape2Triangle (numeric 1) to shape2-triangle-v1. For schema3 it is encoded in required_ids when needed; for legacy schemas it remains external admission. Legacy Snapshot schemas 1 AND 2 with no extension fields retain their existing admitted
object semantics, including existing Shape/2 capability checks; legacy import requires
explicit permission and has no validated old checkpoint claim. Migration is an explicit
Loading-only bootstrap, never ordinary editing/Undo or provider-supplied metadata.

## ResourceId-level ChangeSet and consumers

Each Applied Operation emits one immutable ChangeSet with before/after SemanticGeneration,
existing ObjectSemanticChange records and a separate resourceChanges() span of full value
records. kCreated=1, kUpdated=2, kRemoved=4 are single flags, not combined flags. Creation
covers exact reactivation with different before state. Values, presence, ordering and Scene
recovery are defined in change_set_contract.v1.md. No ChangeSet serialization is introduced:
Operations/Snapshots are durable; ChangeSet is a rebuildable internal event.

## Upstream and downstream scope

All registries/schema additions in this package are candidate-only. Acceptance must name
an exact revision of these decisions and preserve historical 1..15 Operation/canonical
Golden behavior. Production materialization of the accepted semantic contracts needs a
separate authorized implementation package and RED/behavioral verification. The third
Scene/Resource/Composition package may consume the resulting canonical manifest, never a
private Host/provider substitute; it remains blocked until its required accepted bindings
and implementation prerequisites are independently established.

## Candidate verification

candidate_binding_oracle_matrix.v1.md distinguishes runnable design checks from future
production tests. oracles/check_candidate.py is the package-local read-only design oracle; it executes stdlib reference byte encoders,
state-transition examples, legacy/presence and cross-file checks with no production import
or serializer. A design check passing is not proof of product correctness or acceptance.
Independent review must resolve the exact candidate revision. Do not manufacture accepted
records, alter historical control results, or begin the third package P32.
