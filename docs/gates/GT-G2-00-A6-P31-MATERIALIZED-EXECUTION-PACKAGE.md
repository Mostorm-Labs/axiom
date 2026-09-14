# GT-G2-00-A6 P31 Materialized Execution Package v0.3

## Status

```yaml
stage: P31_TARGETED_RECONCILIATION_AND_REPACKAGE
task_id: GT-G2-00-A6
package_id: GT-G2-00-A6-P31-v0.3
reconciliation_subject: EXECUTABLE_BOUNDARY_ONLY
status: MATERIALIZED_FOR_P33_ENTRY_REVIEW
repository_bound: true
T02_R07_completion_target_changed: false
T02_R08_completion_target_changed: false
P20_reentry_required: false
P30_substantive_replan_required: false
P32_replay_from_scratch_required: false
p34_authorized: false
```

This v0.3 supersedes P31 v0.2 only for the executable package boundary. It preserves the valid implementation already present at `4a4d00e9bede523fc0989af22b096fc4a592329f` and does not reopen Authority or Verification.

## Package Authority

```yaml
package_ref: notion://3db4c57a-590c-813b-a3d9-c5b64984b6be/GT-G2-00-A6-P31-v0.3
repository:
  provider: github
  full_name: Mostorm-Labs/axiom
  canonical_branch: main
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
task_anchor:
  revision: 4faf351fc85360d0b843dc380c530c12cbb3a48b
  relation: ancestor
resume_cursor:
  type: P32_ACCEPTED_PARTIAL_RESULT
  execution_ref: codex/gt-g2-a6-incremental-runtime-coordination
  accepted_revision: 4a4d00e9bede523fc0989af22b096fc4a592329f
  completed_through:
    - A6_coordinator_surface_materialized
    - canonical_ChangeSet_consumption
    - generation_validation_basic
    - explicit_incremental_recovery_disposition_basic
    - kRequiresFullRebuild_basic_recovery_path
    - exercised_render_prepare_failure_preserves_old_state
    - predecessor_regression_matrix_reported_PASS
    - git_diff_check_reported_PASS
  not_yet_accepted_as_closed:
    - P31_source_scope_conformance
    - T02_R07_blocking_equivalence_evidence
    - T02_R08_full_atomic_boundary_matrix
    - T02_R08_full_recovery_fixture_matrix
    - independent_FullSceneCompiler_exact_oracle
  next_action: resume_remaining_A6_proof_work_under_P31_v0_3
```

`Task Anchor != Execution Cursor`. P33 must preserve the accepted cursor and must not replay completed coordinator work.

## Current Authority Bindings

```yaml
P15_current: notion://3db4c57a-590c-814f-8dd4-d40ec9c2a36b/GT-G2-00-A6-P15-v0.2
P16_current: notion://3db4c57a-590c-81a7-acbd-d4ec00508759/GT-G2-00-A6-P16-v0.2
A6_P20_current: notion://3db4c57a-590c-8122-99ac-facf2b77aa3c/GT-G2-00-A6-P20-v0.2
global_G2_P20_current: notion://3da4c57a-590c-81e0-b4c9-dad679a95bac/GT-G2-00-P20-v0.3
P30_preserved: notion://3d74c57a-590c-8113-90b3-cc5c0216f8ea/GT-G2-00-P30-v0.1
P32_control_review: notion://3db4c57a-590c-816c-bbd1-e68b2cda257a/GT-G2-00-A6-P32-CONTROL-REVIEW-v0.1
```

Verification bindings remain unchanged:

```yaml
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

No Verification wording, oracle, corpus size, blocking classification, or global evidence-file slot is changed.

## Preserved Valid Implementation

The accepted result revision `4a4d00e9bede523fc0989af22b096fc4a592329f` is valid partial work and must be preserved unless a new verified defect specifically requires changing it.

Accepted implementation facts:

- runtime-local `RuntimeUpdatePlan` and `IncrementalRuntimeCoordinator` exist;
- canonical `canvas::semantic::ChangeSet` is consumed without a second semantic taxonomy;
- basic generation preconditions exist;
- incremental and recovery paths are explicitly distinguished;
- `kRequiresFullRebuild` explicitly enters the existing rebuild path;
- the exercised render-prepare failure preserves prior published Scene/render/spatial state;
- no Semantic Schema, Operation Model, A2 impact/dependency, SpatialIndex implementation, renderer architecture, Tile/GPU/Skia, persistence, or sync scope was changed.

These facts are implementation progress, not T02-R07/T02-R08 Gate evidence closure.

## Executable-Boundary Repair

P31 v0.2 omitted two source surfaces already required by the accepted implementation:

1. `runtime/scene/CMakeLists.txt` for linking the authorized coordinator into `canvas_runtime_scene`;
2. `runtime/scene/tests/incremental_runtime_atomic_publication_test.cpp` for the dedicated R08 atomic-publication test identity.

P31 v0.3 explicitly covers those paths. This repair is not general permission to expand scope.

## Authorized Source Scope

Existing/accepted paths that P33 may preserve or minimally modify only when required by the remaining frozen closure contract:

```text
runtime/scene/CMakeLists.txt
runtime/scene/include/canvas/scene/scene_binding.hpp
runtime/scene/src/scene_binding.cpp
runtime/scene/include/canvas/scene/scene.hpp
runtime/scene/src/scene.cpp
runtime/scene/include/canvas/scene/incremental_runtime_coordinator.hpp
runtime/scene/src/incremental_runtime_coordinator.cpp
runtime/scene/tests/CMakeLists.txt
runtime/scene/tests/incremental_runtime_coordinator_test.cpp
runtime/scene/tests/incremental_runtime_atomic_publication_test.cpp
runtime/scene/tests/incremental_runtime_recovery_test.cpp
runtime/scene/tests/full_incremental_equivalence_test.cpp
runtime/scene/testing/include/canvas/scene/testing/fake_render_scene.hpp
runtime/scene/testing/src/fake_render_scene.cpp
runtime/scene/testing/include/canvas/scene/testing/fake_spatial_index.hpp
runtime/scene/testing/src/fake_spatial_index.cpp
```

New files authorized only for **test-only independent FullSceneCompiler oracle adaptation**:

```text
runtime/scene/tests/incremental_runtime_full_oracle_adapter.hpp
runtime/scene/tests/incremental_runtime_full_oracle_adapter.cpp
```

Use fewer files when possible. Any required path outside this inventory is `PACKAGE_SCOPE_DIVERGENCE` and must fail closed.

## Explicitly Forbidden Production Mutation

Do not modify:

```text
runtime/semantic/**
runtime/scene/include/canvas/scene/scene_commit_input.hpp
runtime/scene/include/canvas/scene/scene_compiler.hpp
runtime/scene/include/canvas/scene/runtime_scene_projection.hpp
runtime/scene/src/runtime_scene_projection.cpp
runtime/scene/include/canvas/scene/full_scene_compiler.hpp
runtime/scene/src/full_scene_compiler.cpp
runtime/scene/include/canvas/scene/scene_types.hpp
runtime/scene/src/scene_impact.cpp
runtime/scene/include/canvas/scene/scene_impact.hpp
runtime/scene/src/scene_dependency_graph.cpp
runtime/scene/include/canvas/scene/scene_dependency_graph.hpp
runtime/scene/src/bounds_system.cpp
runtime/scene/include/canvas/scene/bounds_system.hpp
runtime/scene/src/scene_record_store.cpp
runtime/scene/include/canvas/scene/scene_record_store.hpp
runtime/scene/src/uniform_grid_spatial_index.cpp
runtime/scene/include/canvas/scene/uniform_grid_spatial_index.hpp
runtime/scene/src/linear_spatial_index.cpp
runtime/scene/include/canvas/scene/linear_spatial_index.hpp
runtime/scene/src/direct_render_scene.cpp
runtime/scene/include/canvas/scene/direct_render_scene.hpp
runtime/scene/include/canvas/scene/render_scene.hpp
```

If a listed path does not exist, the entry creates no authority to create it; it remains a prohibition on adding that production surface for A6.

Also forbidden:

- second ChangeSet/ObjectChange/ChangeKind/FieldMask taxonomy;
- second A2 impact classifier or dependency-closure engine;
- semantic truth mutation during recovery;
- Render Core / Tile / GPU / Skia / persistence / sync / schema / wire changes;
- new Product SLOs;
- weakening or replacing the frozen R07/R08 oracle;
- adding a production debug API only to make tests injectable.

## Test-Only FullSceneCompiler Oracle Adapter

Repository reality contains two incompatible `RuntimeSceneProjection` type families. P31 v0.3 does **not** authorize changing that production type conflict. Instead it freezes a test-only translation-unit boundary:

```text
A6 coordinator / Scene test translation unit
        |
        | neutral test DTO only
        v
incremental_runtime_full_oracle_adapter.hpp
        ^
        |
incremental_runtime_full_oracle_adapter.cpp
        |
        | actual independent oracle
        v
canvas::scene::FullSceneCompiler::compile(SemanticReadView)
```

Rules:

1. Adapter `.cpp` is the only new A6 test translation unit that may include `full_scene_compiler.hpp` / the inspection projection family.
2. Adapter header must not expose either conflicting `RuntimeSceneProjection` type.
3. Adapter returns a neutral test-only canonical comparison DTO/digest containing only renderer-neutral observable fields needed by the frozen comparison.
4. The full side must be produced by the actual independent `canvas::scene::FullSceneCompiler::compile(view)`; a test-local substitute compiler does not satisfy R07.
5. The incremental side must be canonicalized from A6-published derived state / public observation surfaces. It must not backfill incremental-side values from `SemanticReadView` merely to manufacture equality.
6. Canonicalization is comparison-only and does not create semantic Authority or alter the oracle.
7. If the frozen comparison cannot be produced through public/allowed surfaces without forbidden production changes, return `PACKAGE_SCOPE_DIVERGENCE`; do not weaken R07.

## Frozen T02-R07 Target — Unchanged

`A6_INCREMENTAL_EQUIVALENCE_EVIDENCE` remains blocking.

Required CTest identity:

```yaml
canvas_g2_a6_incremental_equivalence:
  obligation: T02-R07
  expected_result: PASS
```

It qualifies as blocking evidence only when all frozen requirements are satisfied:

- actual integrated A6 coordinator path;
- actual independent FullSceneCompiler oracle from the same authoritative SemanticReadView;
- exact canonical observable comparison after every true Applied generation;
- all nine V1 kinds and every inherited impact family;
- at least 1000 true Applied revisions per fixed seed;
- at least three documented fixed seeds;
- deterministic replay;
- first mismatch reports seed, step, generation, ObjectId when attributable, and component/field.

The existing single-step coordinator test remains valid partial progress but is not R07 closure until upgraded to this contract.

## Frozen T02-R08 Atomic Target — Unchanged

`A6_ATOMIC_PUBLICATION_EVIDENCE` remains blocking.

Required CTest identity:

```yaml
canvas_g2_a6_atomic_publication:
  obligation: T02-R08
  expected_result: PASS
```

P33 must enumerate the **actual required prepare boundaries represented by the final A6 path** and deterministically inject failure before/after each applicable boundary. At minimum the matrix must account for the runtime-record/Scene participant, bounds/recomputation boundary when represented, spatial participant, and render-invalidation/render participant before publication.

For every injected pre-publication failure:

```yaml
published_generation_after_failure: MUST_EQUAL_previous_published_generation
published_observable_projection_after_failure: MUST_EQUAL_previous_projection_byte_for_byte
partial_participant_visibility: FORBIDDEN
failure_disposition: MUST_BE_EXPLICIT
```

Existing render-prepare rejection coverage is accepted partial work, not the complete matrix. If a required boundary cannot be deterministically injected using authorized test/fake surfaces, return `PACKAGE_SCOPE_DIVERGENCE`.

## Frozen T02-R08 Recovery Target — Unchanged

`A6_RECOVERY_EVIDENCE` remains blocking.

Required CTest identity:

```yaml
canvas_g2_a6_recovery:
  obligation: T02-R08
  expected_result: PASS
```

The recovery matrix must cover:

- dropped or unusable ChangeSet;
- stale derived state;
- semantic generation gap;
- deliberately corrupted derived state using test-only injection surfaces;
- explicit recovery disposition distinct from incremental success;
- exact recovered observable projection equality to the independent FullSceneCompiler oracle through the neutral adapter;
- failed recovery leaves the previously published generation/state unchanged.

A direct `recover()` smoke test remains valid partial progress but is not blocking closure by itself. If deliberate corruption cannot be exercised without a forbidden production debug API, fail closed rather than expand production scope.

## Required Predecessor Regressions — Unchanged

```text
canvas_g2_a0_semantic_generation_binding
canvas_g2_a2_scene_impact
canvas_g2_a2_dependency_closure
canvas_g2_a3_full_incremental_equivalence
canvas_g2_a4_scene_delta
canvas_g2_a5_spatial_index_differential
canvas_rf01_scene_atomicity
canvas_rf01_scene_binding
```

All must remain PASS at the final result revision. `git diff --check` must remain PASS.

## Blocking Evidence — Unchanged

```yaml
A6_INCREMENTAL_EQUIVALENCE_EVIDENCE:
  obligation: T02-R07
  blocking: true
A6_ATOMIC_PUBLICATION_EVIDENCE:
  obligation: T02-R08
  blocking: true
A6_RECOVERY_EVIDENCE:
  obligation: T02-R08
  blocking: true
A6_A2_CONSUMPTION_DIAGNOSTICS:
  blocking: false
```

No 11th final G2 evidence file is authorized. `GT-G2-00-ECC-v0.1` remains unchanged.

## EXECUTION_CLOSURE_CONTRACT v0.3 — Remaining Work Only

```yaml
EXECUTION_CLOSURE_CONTRACT:
  implementation:
    preserve_completed_work_at: 4a4d00e9bede523fc0989af22b096fc4a592329f
    required_remaining_changes:
      - reconcile_source_scope_with_the_accepted_build_wiring_and_atomic_test_surface
      - add_test_only_independent_FullSceneCompiler_oracle_adapter_if_needed
      - upgrade_A6_R07_test_from_smoke_test_to_frozen_long_run_independent_oracle_contract
      - complete_R08_failure_injection_matrix_for_every_actual_required_prepare_boundary
      - complete_R08_recovery_fixture_matrix_and_exact_full_oracle_comparison
      - preserve_explicit_incremental_vs_recovery_disposition
    forbidden_changes:
      - replay_or_replace_accepted_A6_coordinator_work_without_a_new_observed_defect
      - modify_forbidden_production_projection_or_FullSceneCompiler_paths
      - redefine_semantic_ChangeSet_or_change_kinds
      - duplicate_A2_impact_or_dependency_engine
      - weaken_R07_or_R08_or_replace_the_independent_full_oracle
      - silently_skip_uninjectable_required_participant_boundaries
      - add_production_debug_api_only_for_test_corruption
      - add_new_global_G2_evidence_file_slot

  tests:
    required:
      - id: A6_R07_EQUIVALENCE
        command_or_oracle: ctest canvas_g2_a6_incremental_equivalence using actual A6 coordinator and independent FullSceneCompiler adapter
        expected_result: PASS
        blocking_reason: T02-R07 requires end-to-end drift detection after every Applied generation over the frozen corpus
      - id: A6_R08_ATOMIC_PUBLICATION
        command_or_oracle: ctest canvas_g2_a6_atomic_publication with deterministic failure injection across every final required prepare boundary
        expected_result: PASS
        blocking_reason: T02-R08 requires proof that no half-published generation is observable
      - id: A6_R08_RECOVERY
        command_or_oracle: ctest canvas_g2_a6_recovery with dropped/stale/gap/corrupt/failed-recovery fixtures and independent full oracle
        expected_result: PASS
        blocking_reason: T02-R08 recovery orchestration is A6-owned
      - id: A6_PREDECESSOR_REGRESSION_MATRIX
        command_or_oracle: run the frozen predecessor CTest identities listed above
        expected_result: PASS
        blocking_reason: A6 must not regress consumed A0-A5 contracts
      - id: SOURCE_HYGIENE
        command_or_oracle: git diff --check
        expected_result: PASS
        blocking_reason: final result must remain reviewable and clean

  hosted_verification:
    required: []
    optional:
      - existing repository CI on the exact final result revision when available

  evidence:
    blocking:
      - A6_INCREMENTAL_EQUIVALENCE_EVIDENCE -> T02-R07
      - A6_ATOMIC_PUBLICATION_EVIDENCE -> T02-R08
      - A6_RECOVERY_EVIDENCE -> T02-R08
    corroborative:
      - A6_A2_CONSUMPTION_DIAGNOSTICS
      - A6_FOCUSED_EXISTING_SCENE_REGRESSIONS

  durable_result_boundary:
    repository_branch_must_be_pushed: codex/gt-g2-a6-incremental-runtime-coordination
    exact_result_revision_required: true
    reviewer_resolvable_source_required: true
    package_materialization_must_be_resolvable_before_resume: true
    official_Gate_PASS_forbidden_in_execution_return: true

  terminal_success:
    all_of:
      - accepted_partial_work_preserved_or_superseded_only_by_a_new_verified_defect
      - actual_final_changed_paths_conform_to_P31_v0_3
      - A6_R07_frozen_blocking_evidence_closed
      - A6_R08_atomic_frozen_blocking_evidence_closed
      - A6_R08_recovery_frozen_blocking_evidence_closed
      - predecessor_regression_matrix_PASS
      - git_diff_check_PASS
      - exact_final_result_revision_pushed_and_reviewer_resolvable
      - no_forbidden_authority_or_source_change

  terminal_blockers:
    explicit_classes:
      - AUTHORITY_CONFLICT
      - MISSING_REQUIRED_INPUT
      - ENVIRONMENT_BLOCKER
      - FROZEN_VERIFICATION_FAILURE
      - NEW_HIGH_IMPACT_FAILURE_MODE
      - PACKAGE_SCOPE_DIVERGENCE
      - BLOCKED_REPOSITORY_IDENTITY
      - BLOCKED_EVIDENCE

  return_policy:
    continue_until_terminal_state: true
```

## P33 Resume Policy

After this package is materialized, the branch will contain a package-only descendant of the accepted cursor. P33 must use ancestry and classify the observed position rather than require historical HEAD equality.

```yaml
expected_resume_classification: DESCENDANT_CURSOR
accepted_cursor_revision: 4a4d00e9bede523fc0989af22b096fc4a592329f
allowed_descendant_before_resume: package_materialization_only
```

P33 must first verify that the delta after the accepted cursor is only this v0.3 package materialization. It then resumes at the first incomplete frozen proof item and does not replay completed coordinator work.

## Return Contract After Resumed Execution

A successful resumed execution return must carry exact identities:

```yaml
task_id: GT-G2-00-A6
status: READY_FOR_CONTROL_REVIEW
package_ref: notion://3db4c57a-590c-813b-a3d9-c5b64984b6be/GT-G2-00-A6-P31-v0.3
package_materialization_ref: EXACT_THIS_COMMIT
resume_cursor_used: 4a4d00e9bede523fc0989af22b096fc4a592329f
actual_resumed_starting_revision: EXACT_SHA
result_revision: EXACT_SHA
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
blocking_evidence:
  - A6_INCREMENTAL_EQUIVALENCE_EVIDENCE
  - A6_ATOMIC_PUBLICATION_EVIDENCE
  - A6_RECOVERY_EVIDENCE
repository_mutation_scope_conformed: true
next_surface: CONTROL_REVIEW
```

P33/P32 execution must not emit official P34 PASS.
