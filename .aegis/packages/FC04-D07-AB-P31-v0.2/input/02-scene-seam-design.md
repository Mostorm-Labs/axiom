# Scene Prepare / Commit / Abort - Required Design Contract

## Owner
The seam belongs to the existing IncrementalRuntimeCoordinator / SceneBinding / Scene / RuntimeScene path. It must retain the actual Scene participants, publication gate, checkpoints, observer behavior and recovery owner.

## Prepare
- Consume ordered staged post-intent states and ChangeSets from the real Semantic planning path.
- Bind the prepared result to exact Scene revision and semantic-generation basis.
- Compile and prepare the actual Scene record-store, render, spatial-index, damage, published-snapshot, invalidation and RuntimeScene projection resources.
- Reserve all storage and metadata required by commit/publication.
- Do not mutate or publish canonical Document state, visible Scene/RuntimeScene state, revision/generation, damage/invalidation, observers, History or redo.
- Any participant failure discards staged resources and leaves all visible state unchanged.

## Commit
- The grouped-intent owner must ensure all canonical Operations succeeded in the admitted local commit segment.
- Publish the prepared Scene result without fallible allocation or recoverable rejection.
- Preserve per-Operation IDs, generations, stamps, ChangeSets and ordered DataBridge notifications. Do not synthesize one canonical Operation.
- Scene visibility changes coherently only after the whole intent succeeds; no failed prefix is visible.
- If existing per-operation observer contracts require intermediate Scene states incompatible with this guarantee, stop with AUTHORITY_CONFLICT and exact evidence.

## Abort
- Valid before the first canonical mutation.
- Discard prepared Scene/RuntimeScene participant state without changing visible state, revisions, generations, observers, damage/invalidation, History cursor or redo.
- After canonical mutation begins, do not describe compensation as an abort that left state unchanged.

## Compatibility and faults
Existing single-operation apply/synchronize/recover behavior remains compatible. Where appropriate, implement existing methods as prepare+commit wrappers. Inject faults at actual record-store, render, spatial-index, damage/invalidation, RuntimeScene projection, snapshot, result and publication-reservation boundaries. Record actual phase and before/after state; do not move a post-write failure into preflight to make the test pass.

This is a local Runtime/Scene visibility boundary, not a global canonical Transaction, wire change, cross-client transaction or crash-atomic persistence guarantee.
