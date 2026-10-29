# FC-04 D07 P31 Scope Correction v0.2

Status: TASK_PACKAGE_DEFECT correction proposal. The prior P31 package passed structural preflights but omitted the Scene owner scope required by its own pre-write publication-reservation obligation. This correction supersedes only that exclusion. It is not implementation evidence and does not mark D07 PASS.

## Exact base
- Feature anchor: c7f874395d55952e9d5288e2a336a21747f37f11
- Previous task anchor: 832866a2d001a247c4eb374c1fb1899258a6f6cf
- Latest P32 result/current reported branch head: 15e76458f3872be4d1b51d4aac05eb8402719014
- Branch: codex/fc04-d07-selection-audit
- Worktree: C:/Users/Chado/.codex/worktrees/selection-unified-batch1/canvas

Codex must verify actual HEAD, clean/dirty state, ancestry, report and command hashes before materialization. Do not use main.

## Root cause
IncrementalRuntimeCoordinator::apply currently combines preparation and publication. SceneBinding::synchronize invokes Scene::applyPreparedDelta with the publication callback in the same call. RuntimeScene::PreparedPublication and prepare/publish methods are private to existing Scene/Coordinator ownership. Calling live apply as a reservation changes visible Scene/RuntimeScene state and notifies observers before the grouped canonical intent has succeeded.

The old P31 explicitly excluded Scene source changes and did not bind an existing prepare-only seam. Its prior structural-preflight PASS is historical and did not establish that the required seam existed.

## Newly authorized primary Scene-owner files
- runtime/scene/include/canvas/scene/incremental_runtime_coordinator.hpp
- runtime/scene/src/incremental_runtime_coordinator.cpp
- runtime/scene/include/canvas/scene/scene_binding.hpp
- runtime/scene/src/scene_binding.cpp
- runtime/scene/include/canvas/scene/scene.hpp
- runtime/scene/src/scene.cpp
- runtime/scene/include/canvas/scene/scene_types.hpp

## Focused verification files
- runtime/scene/tests/incremental_runtime_atomic_publication_test.cpp
- runtime/scene/tests/incremental_runtime_coordinator_test.cpp
- runtime/scene/tests/scene_atomicity_test.cpp
- runtime/scene/tests/scene_binding_test.cpp
- runtime/scene/tests/incremental_runtime_recovery_test.cpp
- runtime/scene/tests/incremental_runtime_test_access.hpp
- runtime/scene/tests/CMakeLists.txt
- verification/fc04_d07/audit_test.cpp
- verification/fc04_d07/postcommit_fault.cpp
- verification/fc04_d07/CMakeLists.txt

## Conditional participant-owner files
Only if the implementation-design preflight identifies a concrete missing no-fail prepare/commit contract in an existing participant:
- runtime/scene/include/canvas/scene/scene_record_store.hpp; runtime/scene/src/scene_record_store.cpp
- runtime/scene/include/canvas/scene/render_scene.hpp; runtime/scene/src/direct_render_scene.cpp; runtime/scene/src/shadow_render_scene.cpp
- runtime/scene/include/canvas/scene/spatial_index.hpp; runtime/scene/src/linear_spatial_index.cpp; runtime/scene/src/uniform_grid_spatial_index.cpp
- runtime/scene/include/canvas/scene/damage_tracker.hpp; runtime/scene/src/damage_tracker.cpp
- runtime/scene/include/canvas/scene/scene_delta.hpp; runtime/scene/src/scene_delta.cpp; runtime/scene/include/canvas/scene/spatial_delta.hpp; runtime/scene/src/spatial_delta.cpp

Every conditional path must be justified in the design record and included in the new source lock before mutation. If another owner or wider scope is needed, stop and return to P31.

## Required behavior
1. Prepare real Scene/RuntimeScene participant resources for staged post-intent states before the first canonical write.
2. During prepare, visible Scene/RuntimeScene records, revision/generation, queries, hit tests, draw lists, damage/invalidation, observers and public snapshots remain unchanged.
3. All fallible allocation, validation and participant preparation needed for publication completes before canonical mutation.
4. Commit/publication is no-fail and allocation-free after the whole canonical intent succeeds, or an existing equivalent seam is proved.
5. Abort before canonical mutation discards prepared state and leaves Document, Scene, History cursor and redo branch unchanged.
6. Preserve each typed Operation's ID, generation successor, commit stamp, ChangeSet and ordered eligible DataBridge notifications. Do not create a synthetic Operation or implicitly group independent commands.
7. Scene observers must never see a partial failed intent. If the accepted 07-14 per-operation notification contract cannot coexist with coherent local visibility, stop with AUTHORITY_CONFLICT; do not silently weaken it or call compensation a pre-write rejection.
8. Existing single-operation apply/synchronize/recovery behavior remains compatible.
9. No public C ABI, wire/schema, ObjectKind, FieldId, OperationKind, global registry or global canonical Transaction changes.
10. Preserve the single-command OOM fix, 11 passing D07 checks and all 160 Selection regressions.

## Package changes required
Create a new immutable package, suggested path .aegis/packages/FC04-D07-AB-P31-v0.2/. Do not overwrite earlier packages/results. Bind task_anchor to the exact verified current result; keep the feature anchor. Rebuild source locks and package hashes; update design, closure, test matrix, implementation context, evidence contract and terminal blockers. Run PackageBindingPreflight, EvidenceContractPreflight, DesignPreflight and the D07 package preflight.

This task only materializes and validates the P31 correction. No product source changes in this task.
