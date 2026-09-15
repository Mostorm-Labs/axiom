# GT-G2-00-A6 P31 Materialized Execution Package v0.6

## Status

```yaml
stage: P31_TARGETED_REPACKAGE
task_id: GT-G2-00-A6
package_id: GT-G2-00-A6-P31-v0.6
reconciliation_subject: A6_R08_ACTUAL_PARTICIPANT_TRANSACTION_SCOPE
status: MATERIALIZED_FOR_P33_ENTRY_REVIEW
repository_bound: true
supersedes_package: GT-G2-00-A6-P31-v0.5
root_cause: TASK_PACKAGE_DEFECT
preserve_valid_result_revision: d9f5475fc4e42db7cbee848bd7e94284c2bb69fc
P15_reopen_required: false
P16_reopen_required: false
P20_reentry_required: false
P30_substantive_replan_required: false
P32_replay_from_scratch_required: false
R08_completion_target_changed: false
p34_authorized: false
```

This v0.6 fixes only the P31 v0.5 executability defect for frozen T02-R08. Preserve the valid result at `d9f5475fc4e42db7cbee848bd7e94284c2bb69fc`; do not replay already completed R07 work. P15/P16/P20 remain frozen. R08 is not weakened or redefined.

## Package Authority

```yaml
package_ref: notion://3dc4c57a-590c-810c-ac32-f128cb1a5758/GT-G2-00-A6-P31-v0.6
repository:
  provider: github
  full_name: Mostorm-Labs/axiom
  canonical_branch: main
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
materialization_branch: aegis/gt-g2-a6-p31-v0-6-materialization
task_anchor:
  revision: 4faf351fc85360d0b843dc380c530c12cbb3a48b
  relation: ancestor
package_materialization_ref: EXACT_THIS_COMMIT
resume_cursor:
  type: P33_ACCEPTED_PARTIAL_RESULT
  execution_ref: codex/gt-g2-a6-incremental-runtime-coordination
  accepted_revision: d9f5475fc4e42db7cbee848bd7e94284c2bb69fc
  completed_through:
    - preserve_all_valid_work_through_e709ce02
    - R07_comparison_bound_to_exact_P20_v0_4_A1_structural_domain
    - R07_fixed_seeds_11_12_13_with_1000_true_Applied_each_reported_PASS
    - R07_all_nine_V1_kinds_and_after_every_Applied_comparison_reported_PASS
    - normal_incremental_RuntimeScene_does_not_use_A3_full_materialization
    - recovery_A3_bridge_and_dropped_stale_gap_corruption_failed_recovery_matrix_reported_PASS
    - predecessor_regressions_reported_14_of_14_PASS
    - git_diff_check_reported_PASS
    - execution_branch_pushed_at_d9f5475
  not_yet_closed:
    - R08_actual_participant_prepare_publication_transaction_binding
    - R08_actual_checkpoint_failure_matrix
    - A6_ATOMIC_PUBLICATION_EVIDENCE
    - final_exact_result_and_reviewer_resolvable_evidence_return
  next_action: expose_or_refactor_the_actual_A6_prepare_publication_transaction_then_bind_R08_checkpoints_to_real_participant_lifecycle_without_replaying_R07
```

`Task Anchor != Execution Cursor`. The materialization branch is documentation-only and MUST NOT replace or be cherry-picked merely to obtain the execution cursor.

## Current Authority / Verification Bindings

```yaml
P15_current: notion://3db4c57a-590c-81bb-a5a6-f2bd00847c55/GT-G2-00-A6-P15-v0.3
P16_current: notion://3db4c57a-590c-81a1-b2c0-fba8cf76ae9b/GT-G2-00-A6-P16-v0.3
A6_P20_current: notion://3dc4c57a-590c-81fb-9fd8-cec20062874c/GT-G2-00-A6-P20-v0.4
global_G2_P20_current: notion://3da4c57a-590c-81e0-b4c9-dad679a95bac/GT-G2-00-P20-v0.3
P30_preserved: notion://3d74c57a-590c-8113-90b3-cc5c0216f8ea/GT-G2-00-P30-v0.1
superseded_P31: notion://3dc4c57a-590c-81ce-8c78-d4fb1c68d007/GT-G2-00-A6-P31-v0.5

verification_spec_id: GT-G2-00-VS-v0.3
obligation_set_id: GT-G2-00-OBL-v0.3
trusted_basis_id: GT-G2-00-TB-v0.3
scope_contract_id: GT-G2-00-SCOPE-v0.3
acceptance_oracle_id: GT-G2-00-ORACLE-v0.3
corpus_id: GT-G2-00-CORPUS-v0.2
evidence_compilation_contract_id: GT-G2-00-ECC-v0.1
A6_obligation_slice:
  - T02-R07
  - T02-R08
```

No P15/P16/P20/P30 semantic target changes in this package.

## Root Cause Closed by v0.6

P31 v0.5 required deterministic failure injection before/after every actual required participant prepare boundary and immediately before publication, while necessary transaction-boundary implementation files were read-only. At `d9f5475...`, the remaining Bounds / Spatial / Invalidation checkpoints are therefore coordinator-level seams executed before entering the real `SceneBinding -> Scene` participant transaction. Same-named checkpoint labels without the actual participant lifecycle between them are not valid R08 evidence.

v0.6 freezes these rules:

1. A checkpoint counts for R08 only when immediately bound to the actual operation that prepares/finalizes the canonical participant state named by the checkpoint.
2. A6 must own or explicitly coordinate one prepare -> publication transaction. If retained `SceneBinding` / `Scene` code commits compatibility/backing participant state before the A6 publication barrier, implementation may refactor it into staged prepare + explicit commit/publish, or an authority-equivalent non-public transaction shape.
3. `IRenderScene` and Render-Core `DamageTracker` remain excluded from canonical A6 publication truth. They may remain compatibility/backing implementation and regression coverage, but cannot substitute for Bounds / Spatial / Scene-Core invalidation evidence.
4. Bounds may use existing RuntimeScene extra bounds fields as physical backing only if bounds derivation remains separately staged/checkpointable and uses the trusted BoundsSystem computation. Those fields remain outside R07 A1 structural identity.
5. The generation-bound `SceneDelta` / equivalent Scene-Core invalidation output must be staged/finalized as an actual A6 publication participant when selected by the transition. Render-Core damage state is not the substitute.

## Minimum Canonical R08 Participant Set

```yaml
canonical_participants:
  - RuntimeScene_or_conforming_backing
  - Bounds_derived_state
  - production_SpatialIndex_state
  - generation_bound_Scene_Core_invalidation_output
additional_participants:
  rule: include_every_actual_Scene_Core_participant_mutated_by_the_selected_transition
excluded_as_canonical_R08_substitutes:
  - IRenderScene_execution_state
  - Render_Core_DamageTracker_state
  - GPU_Skia_Tile_FrameState_presentation_state
```

## Authorized Mutable Source Scope v0.6

### Existing production files

```text
runtime/scene/CMakeLists.txt
runtime/scene/include/canvas/scene/incremental_runtime_coordinator.hpp
runtime/scene/src/incremental_runtime_coordinator.cpp
runtime/scene/include/canvas/scene/scene_types.hpp
runtime/scene/include/canvas/scene/scene.hpp
runtime/scene/src/scene.cpp
runtime/scene/include/canvas/scene/scene_binding.hpp
runtime/scene/src/scene_binding.cpp
runtime/scene/include/canvas/scene/bounds_system.hpp
runtime/scene/src/bounds_system.cpp
```

Scope limits:

- `scene.hpp/.cpp` and `scene_binding.hpp/.cpp` may change only to expose/refactor the non-public staged prepare/commit/publication transaction needed by A6 and carry actual participant-prepared state across the A6 barrier.
- `bounds_system.hpp/.cpp` may change only to provide local prepared-state/backing mechanics required by the frozen Bounds participant contract; bounds mathematics/semantics must not be redefined.
- `scene_types.hpp` may change only for A6 RuntimeScene/bounds/invalidation staged-publication representation. R07 A1 structural equality remains exactly P20 v0.4.

### Optional private A6 transaction helper files

```text
runtime/scene/src/incremental_runtime_transaction.hpp
runtime/scene/src/incremental_runtime_transaction.cpp
```

### Existing private production files

```text
runtime/scene/src/incremental_runtime_full_materialization_bridge.hpp
runtime/scene/src/incremental_runtime_full_materialization_bridge.cpp
runtime/scene/src/incremental_runtime_checkpoint.hpp
```

The A3 bridge remains recovery/full-reference only.

### Tests and test-only support

```text
runtime/scene/tests/CMakeLists.txt
runtime/scene/tests/incremental_runtime_coordinator_test.cpp
runtime/scene/tests/incremental_runtime_atomic_publication_test.cpp
runtime/scene/tests/incremental_runtime_recovery_test.cpp
runtime/scene/tests/incremental_runtime_full_oracle_adapter.hpp
runtime/scene/tests/incremental_runtime_full_oracle_adapter.cpp
runtime/scene/tests/incremental_runtime_test_access.hpp
runtime/scene/testing/include/canvas/scene/testing/fake_spatial_index.hpp
runtime/scene/testing/src/fake_spatial_index.cpp
```

Any required path outside this inventory is `PACKAGE_SCOPE_DIVERGENCE` and MUST fail closed rather than be silently added in P33.

## Read-Only Dependencies / Forbidden Mutation

Read/call but do not modify unless explicitly listed mutable above:

```text
runtime/semantic/**
runtime/scene/include/canvas/scene/scene_commit_input.hpp
runtime/scene/include/canvas/scene/scene_compiler.hpp
runtime/scene/include/canvas/scene/runtime_scene_projection.hpp
runtime/scene/src/runtime_scene_projection.cpp
runtime/scene/include/canvas/scene/full_scene_compiler.hpp
runtime/scene/src/full_scene_compiler.cpp
runtime/scene/include/canvas/scene/scene_delta.hpp
runtime/scene/src/scene_delta.cpp
runtime/scene/include/canvas/scene/scene_impact.hpp
runtime/scene/src/scene_impact.cpp
runtime/scene/include/canvas/scene/scene_dependency_graph.hpp
runtime/scene/src/scene_dependency_graph.cpp
runtime/scene/include/canvas/scene/scene_record_store.hpp
runtime/scene/src/scene_record_store.cpp
runtime/scene/include/canvas/scene/spatial_index.hpp
runtime/scene/include/canvas/scene/uniform_grid_spatial_index.hpp
runtime/scene/src/uniform_grid_spatial_index.cpp
runtime/scene/include/canvas/scene/linear_spatial_index.hpp
runtime/scene/src/linear_spatial_index.cpp
runtime/scene/include/canvas/scene/render_scene.hpp
runtime/scene/include/canvas/scene/direct_render_scene.hpp
runtime/scene/src/direct_render_scene.cpp
runtime/scene/include/canvas/scene/damage_tracker.hpp
runtime/scene/src/damage_tracker.cpp
```

Forbidden:

- weakening or renaming away T02-R08;
- counting coordinator-only no-op/same-named seams as actual participant checkpoints;
- treating Render-Core DamageTracker or IRenderScene state as canonical A6 Bounds/Spatial/invalidation truth;
- reintroducing A3 full materialization into normal incremental RuntimeScene construction;
- modifying A2/A3/A4/A5 semantic or algorithm authority;
- changing Spatial backend algorithms merely to expose transaction staging;
- changing bounds mathematics merely to satisfy tests;
- public production debug/corruption APIs;
- replaying completed R07 work from the historical anchor;
- Render Core/Tile/GPU/Skia/persistence/sync/schema/platform work.

## Required Verification

### A6_R07_EQUIVALENCE

```yaml
id: A6_R07_EQUIVALENCE
ctest: canvas_g2_a6_incremental_equivalence
obligation: T02-R07
expected_result: PASS
```

At final result revision preserve P20 v0.4 exact structural domain, at least 3 documented fixed seeds, at least 1000 true Applied transitions per seed, all nine V1 kinds, and comparison after every true Applied generation.

### A6_R08_ATOMIC_PUBLICATION

```yaml
id: A6_R08_ATOMIC_PUBLICATION
ctest: canvas_g2_a6_atomic_publication
obligation: T02-R08
expected_result: PASS
```

The test matrix must prove actual lifecycle binding, not enum coverage. For every canonical participant selected by the implementation path, evidence must identify:

```yaml
checkpoint_entry:
  participant: exact_actual_participant
  actual_prepare_or_finalize_operation: exact_code_boundary
  injected_before: PASS_required
  injected_after: PASS_required_when_meaningful_before_publication
  prior_published_generation_unchanged_on_failure: true
  prior_participant_state_unchanged_on_failure: true
  no_partial_publication: true
```

The matrix MUST cover RuntimeScene/backing, Bounds derived state, production SpatialIndex state, Scene-Core invalidation finalization, every additional selected Scene-Core participant, and the immediate pre-publication barrier. A checkpoint label with no real operation between before/after is invalid evidence.

### A6_R08_RECOVERY

```yaml
id: A6_R08_RECOVERY
ctest: canvas_g2_a6_recovery
obligation: T02-R08
expected_result: PASS
```

Preserve dropped/unusable ChangeSet, stale state, generation gap, corrupt derived state, failed recovery atomicity, and material A3 full-reference recovery use.

### Predecessor regressions

All predecessor regressions frozen by P31 v0.5 plus `git diff --check` MUST PASS at the final result revision.

## Evidence Contract

```yaml
blocking:
  - A6_INCREMENTAL_EQUIVALENCE_EVIDENCE
  - A6_ATOMIC_PUBLICATION_EVIDENCE
  - A6_RECOVERY_EVIDENCE
corroborative:
  - A6_A2_CONSUMPTION_DIAGNOSTICS
  - legacy_SceneBinding_render_damage_regressions
```

`A6_ATOMIC_PUBLICATION_EVIDENCE` is not closed until the actual-participant matrix is closed.

## EXECUTION_CLOSURE_CONTRACT

```yaml
EXECUTION_CLOSURE_CONTRACT:
  implementation:
    required_changes:
      - preserve_d9f5475_and_completed_R07_work
      - expose_or_refactor_actual_prepare_publication_transaction_under_A6_coordination
      - bind_R08_checkpoints_to_actual_RuntimeScene_Bounds_Spatial_invalidation_lifecycle
      - preserve_recovery_A3_independence
      - keep_canonical_and_compatibility_participants_correctly_separated
    forbidden_changes:
      - weaken_R08
      - replay_completed_R07
      - restore_A3_as_normal_incremental_source
      - use_RenderCore_DamageTracker_as_SceneCore_invalidation_substitute
      - modify_upstream_semantic_or_A2_A3_A4_A5_algorithm_authority
  tests:
    required:
      - id: A6_R07_EQUIVALENCE
        command_or_oracle: canvas_g2_a6_incremental_equivalence
        expected_result: PASS
        blocking_reason: preserve_T02_R07_after_transaction_refactor
      - id: A6_R08_ATOMIC_PUBLICATION
        command_or_oracle: canvas_g2_a6_atomic_publication_with_actual_participant_checkpoint_matrix
        expected_result: PASS
        blocking_reason: T02_R08_actual_participant_atomicity
      - id: A6_R08_RECOVERY
        command_or_oracle: canvas_g2_a6_recovery
        expected_result: PASS
        blocking_reason: T02_R08_recovery_atomicity
      - id: PREDECESSOR_REGRESSIONS
        command_or_oracle: frozen_P31_v0_5_predecessor_matrix_plus_git_diff_check
        expected_result: PASS
        blocking_reason: preserve_prior_G2_and_RF01_behavior
  hosted_verification:
    required: []
    optional: []
  evidence:
    blocking:
      - A6_INCREMENTAL_EQUIVALENCE_EVIDENCE
      - A6_ATOMIC_PUBLICATION_EVIDENCE
      - A6_RECOVERY_EVIDENCE
    corroborative:
      - A6_A2_CONSUMPTION_DIAGNOSTICS
      - legacy_SceneBinding_render_damage_regressions
  terminal_success:
    all_of:
      - execution_branch_descends_from_d9f5475
      - repository_mutation_scope_conforms_to_v0_6
      - actual_participant_checkpoint_matrix_complete
      - every_required_prepublication_failure_preserves_previous_canonical_generation_and_state
      - no_coordinator_only_empty_checkpoint_is_counted_as_R08_closure
      - R07_final_regression_PASS
      - R08_atomic_publication_PASS
      - R08_recovery_PASS
      - predecessor_matrix_PASS
      - git_diff_check_PASS
      - exact_final_result_revision_pushed_and_reviewer_resolvable
      - no_forbidden_authority_or_algorithm_change
  terminal_blockers:
    explicit_classes:
      - AUTHORITY_CONFLICT
      - MISSING_REQUIRED_INPUT
      - ENVIRONMENT_BLOCKER
      - FROZEN_VERIFICATION_FAILURE
      - NEW_HIGH_IMPACT_FAILURE_MODE
      - PACKAGE_SCOPE_DIVERGENCE
      - BLOCKED_REPOSITORY_IDENTITY
      - BLOCKED_EXECUTION_DIVERGENCE
  return_policy:
    continue_until_terminal_state: true
```

## Durable Result / Return Contract

```yaml
repository_branch_must_be_pushed: codex/gt-g2-a6-incremental-runtime-coordination
exact_result_revision_required: true
reviewer_resolvable_source_required: true
package_materialization_must_be_resolvable_before_resume: true
official_Gate_PASS_forbidden_in_execution_return: true
return_surface: CONTROL_REVIEW
```

P31 v0.6 does not authorize P34 or Gate PASS. After exact package materialization resolves, route to P33 Entry Review against `d9f5475...` or a verified descendant; do not automatically execute CODE_EXECUTION from P31.