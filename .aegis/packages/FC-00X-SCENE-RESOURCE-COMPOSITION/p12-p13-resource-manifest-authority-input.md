# P12/P13 ResourceManifest authority binding input

This file is a modeling input and evidence boundary. It is not an accepted Authority record and does not authorize product mutation.

## Confirmed semantic rules from accepted ADRs

| Topic | Frozen rule | Source |
|---|---|---|
| Resource identity | `ResourceId` is a stable, non-reusable semantic identity, distinct from path, URL, platform handle and content hash. | ADR-0013 |
| Manifest ownership | `ResourceManifest` is versioned Document semantic state and contributes to the Document digest. | ADR-0013, ADR-0020 |
| Minimum entry | Each entry includes ResourceId, kind, ResourceRevision, versioned ContentHash, byte length and normalized decode/layout metadata. | ADR-0013 |
| Content hash | V1 uses `sha256:<lowercase hex>` for immutable blob identity. | ADR-0013 |
| Replacement | Replacing content updates the same ResourceId's manifest revision/hash through an Operation; bytes are never silently overwritten under an old hash. | ADR-0013 |
| Digest | Missing blob availability does not change the graph+manifest digest; a manifest binding change does. | ADR-0013, Verification Strategy |
| Resolver boundary | Resolver reads the manifest and verifies supplied bytes; download/decode/upload failure produces derived diagnostics/placeholder and does not mutate Document state. | ADR-0013 |
| Change tracking | ResourceManifest binding changes belong to SemanticChanges; cache/layout/spatial hints remain derived invalidation hints. | ADR-0019 |
| Snapshot/recovery | Snapshot binds manifest, identity, revision/frontier and digest atomically; resource availability is separate from semantic validity. | ADR-0020 |

## Current repository observation at task anchor

- `runtime/semantic/include/canvas/semantic/snapshot.hpp`: `SemanticSnapshot` contains only `document_id`, `schema_version`, and `objects`.
- `runtime/semantic/include/canvas/semantic/object_record.hpp`: objects contain ResourceId references through content/style, but no manifest entries.
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/snapshot.proto`: `DocumentSnapshot` has fields 1–3 only (`document_id`, `schema_version`, `objects`).
- `runtime/semantic/include/canvas/semantic/change_set.hpp`: `SemanticChangeFlags` has no resource-manifest flag.
- `runtime/semantic/include/canvas/semantic/operation.hpp` and `operation_payload.hpp`: the current OperationKind/payload set has no manifest-binding operation.
- No accepted branch or repo-local package in this repository exposes a typed `ResourceManifest` representation that the composition package can consume.

## P12/P13 decisions still required

1. Add or bind the canonical ResourceManifest type without creating a second runtime-owned manifest.
2. Define ResourceRevision and ContentHash value types and exact normalization/validation rules.
3. Define the manifest entry's kind, byte-length and decode/layout metadata schema and digest encoding.
4. Define the manifest-binding Operation payload, validation, atomicity, ordering, replay, undo/redo and ChangeSet flag.
5. Extend Snapshot/restore atomically to carry manifest bindings while preserving missing-blob semantics.
6. Define compatibility/version behavior for snapshots and operations that do not contain a manifest.

Until these decisions are accepted and materialized, R2 must stop with `BLOCKED_AUTHORITY`. Provider bytes, Host metadata, or a private `runtime/composition` manifest cannot substitute for the missing canonical semantic binding.
