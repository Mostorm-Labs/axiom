# P12/P13 ResourceManifest authority binding input

This file preserves the modeling input and evidence boundary. The accepted Authority is now bound separately by the exact P12/P13 package and acceptance refs below; this file still does not authorize product mutation.

Accepted Authority binding: `repo:.aegis/packages/FC-00X-RESOURCE-MANIFEST-P12-P13/package.json@057464f9fab2db4b67c032134cce3f5f417a64e0`
Acceptance record: `repo:.aegis/results/FC-00X-RESOURCE-MANIFEST-P12-P13/p23-authority-acceptance-v0.4.json@f5e0cfbc6b324b9fcabe77280d9363b10b1cf680`
The accepted Authority resolves the semantic decisions; the typed product representation remains absent at the third-package anchor and is therefore an implementation precondition for R2.

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
- The accepted P12/P13 package defines the canonical contract, but no typed `ResourceManifest` product representation is present at the third-package anchor for composition to consume.

## P12/P13 decisions still required

1. Add or bind the canonical ResourceManifest type without creating a second runtime-owned manifest.
2. Define ResourceRevision and ContentHash value types and exact normalization/validation rules.
3. Define the manifest entry's kind, byte-length and decode/layout metadata schema and digest encoding.
4. Define the manifest-binding Operation payload, validation, atomicity, ordering, replay, undo/redo and ChangeSet flag.
5. Extend Snapshot/restore atomically to carry manifest bindings while preserving missing-blob semantics.
6. Define compatibility/version behavior for snapshots and operations that do not contain a manifest.

The decisions are now accepted at the control layer. Until the typed product representation is materialized at the accepted implementation revision, R2 must stop with `BLOCKED_IMPLEMENTATION`. Provider bytes, Host metadata, or a private `runtime/composition` manifest cannot substitute for the missing canonical semantic binding.
