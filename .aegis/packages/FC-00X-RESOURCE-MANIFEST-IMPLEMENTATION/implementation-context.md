# FC-00X ResourceManifest implementation P31 context

## Exact authority and baseline

- Repository: `github:Mostorm-Labs/axiom`
- Execution branch: `codex/fc00x-resource-manifest-implementation`
- Task anchor: `02078a36818e7722309aefe679089a48212a424d` (`ancestor`)
- Accepted P12/P13 Authority: `fc00x-resource-manifest-p12-p13-v0.4`
- Accepted Authority revision: `057464f9fab2db4b67c032134cce3f5f417a64e0`
- Accepted package SHA-256: `f8b3ff32b604d19be1f93d37e3f275c64569b199e72cdb05aab204a1e60401d5`
- Acceptance record: `repo:.aegis/results/FC-00X-RESOURCE-MANIFEST-P12-P13/p23-authority-acceptance-v0.4.json@f5e0cfbc6b324b9fcabe77280d9363b10b1cf680`
- This package is P31 packaging only. It authorizes no production mutation until an independent Control Review accepts this exact package.

## Root cause being closed

The accepted semantic Authority exists at the control layer, but the repository anchor has no typed ResourceManifest source in SemanticSnapshot/ObjectRecord/protobuf Snapshot, no UpdateResourceManifest operation, and no resource ChangeSet. The downstream Scene composition package therefore correctly returned BLOCKED_IMPLEMENTATION.

## Target owner and flow

SemanticSnapshot owns the canonical manifest value alongside objects. OperationEngine owns prepare/apply and atomic generation/ledger/ChangeSet publication. Resource availability, provider bytes, decode/layout caches and missing/corrupt status remain derived and never mutate Document semantic state. SnapshotCodec/Bootstrap owns schema3 encode/decode, digest verification, explicit legacy admission and atomic restore. SemanticReadView exposes the captured manifest value without creating a second owner.

## Required implementation map

1. Add typed manifest values and validators with exact accepted field order, hash normalization, kind metadata registry, tombstone lineage and full ResourceId sorting.
2. Add OperationKind=16 and UpdateResourceManifest payload; preserve operations 1..15 and old registry behavior.
3. Extend apply plan/state storage to stage a complete manifest, validate references and fences, apply atomically, update generation/ledger and emit exactly one resource ChangeSet.
4. Extend Snapshot protobuf/schema3 and domain codec mapping; compute and verify semantic/checkpoint digests with fixed-width candidate vectors.
5. Extend restore and read view; preserve schema1/2 explicit legacy read-only admission and reject mixed profiles.
6. Add tests before implementation for positive, negative, idempotency/collision, reference removal, tombstone/reactivation, digest and recovery behavior.

## Failure and rollback order

Validate envelope/payload and normalization first; compare full canonical idempotency payload before current fences; validate expected manifest revision, action disjointness, references and complete staged result; publish object and manifest atomically; then advance SemanticGeneration, commit clock and ChangeSet. Any failure keeps previous complete object+manifest state and emits no ChangeSet. Availability events are outside semantic apply.

## Non-goals

No Scene/composition implementation, provider port, Runtime resource cache, platform consumer, public ABI/SDK, dynamic registry, Android, POC-06/Arc, G5, or historical control mutation.
