# P12/P13 ResourceManifest authority binding input

This file preserves the modeling input and evidence boundary. The accepted Authority is now bound separately by the exact P12/P13 package and acceptance refs below; this file still does not authorize product mutation.

Accepted Authority binding: `repo:.aegis/packages/FC-00X-RESOURCE-MANIFEST-P12-P13/package.json@057464f9fab2db4b67c032134cce3f5f417a64e0`
Acceptance record: `repo:.aegis/results/FC-00X-RESOURCE-MANIFEST-P12-P13/p23-authority-acceptance-v0.4.json@f5e0cfbc6b324b9fcabe77280d9363b10b1cf680`
The accepted Authority resolves the semantic decisions; the typed product representation is bound to implementation revision `4bef9d6e222d156a79a8358f7d88908a48e39f67` and materialized ref `601304425870ceb5715ae34a6da3b6ef1dfdd7e5`.

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

- `runtime/semantic/include/canvas/semantic/resource_manifest.hpp` defines the typed manifest, and `snapshot.hpp` exposes it through `SemanticSnapshot`.
- `object_record.hpp` remains the object reference source; manifest entries are held by the typed `ResourceManifest` bound above.
- `snapshot.proto` now carries the manifest field and typed manifest messages at the bound implementation revision.
- `change_set.hpp` carries resource manifest semantic changes at the bound implementation revision.
- `operation.hpp`/`operation_payload.hpp` carry `OperationKind=16` and the manifest update payload at the bound implementation revision.
- The accepted P12/P13 package defines the canonical contract, but typed `ResourceManifest` product representation is present at the re-materialized anchor for composition to consume.

## P12/P13 decisions still required

1. Consume the bound canonical ResourceManifest type without creating a second runtime-owned manifest.
2. Verify the bound ResourceRevision and ContentHash normalization/validation rules.
3. Verify the bound manifest entry metadata and digest encoding.
4. Verify the bound manifest-binding Operation payload, validation, atomicity, ordering, replay, undo/redo and ChangeSet flag.
5. Verify Snapshot/restore atomically carries manifest bindings while preserving missing-blob semantics.
6. Verify compatibility/version behavior for snapshots and operations that do not contain a manifest.

The decisions are now accepted at the control layer. With the typed product representation materialized at the accepted implementation revision, R2 must verify the bound implementation and continue. Provider bytes, Host metadata, or a private `runtime/composition` manifest cannot substitute for the canonical semantic binding.
