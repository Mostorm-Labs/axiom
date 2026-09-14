# GT-G2-00-A6 P31 Materialized Execution Package v0.2

## Status

```yaml
stage: P31_TASK_PACKAGING
task_id: GT-G2-00-A6
package_id: GT-G2-00-A6-P31-v0.2
status: MATERIALIZED_FOR_FRESH_P32_ENTRY_REVIEW
repository_bound: true
p32_authorized: false
```

## Package Authority

```yaml
package_ref: notion://3db4c57a-590c-8162-ab03-db1eb3fb0ffc/GT-G2-00-A6-P31-v0.2
repository:
  provider: github
  full_name: Mostorm-Labs/axiom
  canonical_branch: main
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
task_anchor:
  revision: 4faf351fc85360d0b843dc380c530c12cbb3a48b
  relation: ancestor
resume_cursor: null
```

This v0.2 supersedes the repository materialization of P31 v0.1. The prior materialization commit `b4b63be9c9f2ae728a351fc1ed3b57469e2f7b70` is historical only.

## Current Authority Bindings

```yaml
P15_current: notion://3db4c57a-590c-814f-8dd4-d40ec9c2a36b/GT-G2-00-A6-P15-v0.2
P16_current: notion://3db4c57a-590c-81a7-acbd-d4ec00508759/GT-G2-00-A6-P16-v0.2
A6_P20_current: notion://3db4c57a-590c-8122-99ac-facf2b77aa3c/GT-G2-00-A6-P20-v0.2
global_G2_P20_current: notion://3da4c57a-590c-81e0-b4c9-dad679a95bac/GT-G2-00-P20-v0.3
P30_preserved: notion://3d74c57a-590c-8113-90b3-cc5c0216f8ea/GT-G2-00-P30-v0.1
```

Verification bindings:

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

## Corrected A6 Boundary

Canonical A6 input is only:

```text
SemanticReadView @ Gafter
+
immutable canvas::semantic::ChangeSet(Gbefore -> Gafter)
```

A6 owns runtime update planning, generation tracking, participant coordination, atomic derived-state publication, failure propagation, and explicit full-rebuild recovery orchestration.

A6 consumes but does not own A2 impact classification or dependency closure semantics.

`RuntimeUpdatePlan`, if materialized, is runtime-local derived coordination state only. It is not canonical, wire, persistent, or semantic truth.

The following v0.1 concepts are forbidden and are not implementation targets:

```text
second ChangeSet contract
ObjectChange semantic taxonomy
ChangeKind semantic taxonomy
FieldMask semantic taxonomy
A6-owned impact classifier
A6-owned dependency closure engine
```

## Execution Purpose

Implement the smallest A6 coordination layer that:

1. consumes canonical `SemanticReadView + canvas::semantic::ChangeSet`;
2. consumes existing A2 impact/dependency results without duplication;
3. coordinates the existing localized A4/A5 derived update paths;
4. publishes one coherent semantic generation only after required preparation succeeds;
5. leaves the previous published generation/state unchanged after failed preparation;
6. explicitly recovers unsafe incremental state through the independent FullSceneCompiler path;
7. distinguishes recovery from successful incremental application;
8. satisfies unchanged T02-R07 and T02-R08.

## Authorized Source Scope

Existing files allowed for minimal reconciliation:

```text
runtime/scene/include/canvas/scene/scene_binding.hpp
runtime/scene/src/scene_binding.cpp
runtime/scene/include/canvas/scene/scene.hpp
runtime/scene/src/scene.cpp
runtime/scene/tests/full_incremental_equivalence_test.cpp
runtime/scene/tests/scene_atomicity_test.cpp
runtime/scene/tests/scene_binding_test.cpp
runtime/scene/tests/CMakeLists.txt
runtime/scene/testing/include/canvas/scene/testing/fake_render_scene.hpp
runtime/scene/testing/include/canvas/scene/testing/fake_spatial_index.hpp
runtime/scene/testing/src/fake_render_scene.cpp
runtime/scene/testing/src/fake_spatial_index.cpp
```

New files allowed only for A6-owned coordination/tests:

```text
runtime/scene/include/canvas/scene/incremental_runtime_coordinator.hpp
runtime/scene/src/incremental_runtime_coordinator.cpp
runtime/scene/tests/incremental_runtime_coordinator_test.cpp
runtime/scene/tests/incremental_runtime_recovery_test.cpp
```

Use fewer files when possible. Any required source outside this inventory is `PACKAGE_SCOPE_DIVERGENCE` and must fail closed.

## Explicitly Forbidden Source Changes

Do not modify:

```text
runtime/semantic/**
runtime/scene/include/canvas/scene/scene_commit_input.hpp
runtime/scene/include/canvas/scene/scene_compiler.hpp
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
runtime/scene/src/full_scene_compiler.cpp
runtime/scene/include/canvas/scene/full_scene_compiler.hpp
runtime/scene/src/direct_render_scene.cpp
runtime/scene/include/canvas/scene/direct_render_scene.hpp
runtime/scene/include/canvas/scene/render_scene.hpp
```

Also forbidden: schema/wire/persistence/sync changes; Render Core/Tile/RasterCache/GPU/Skia/platform work; new Product SLOs; reopening A0-A5; second semantic validator/apply engine; second A2 impact/dependency engine; silent recovery fallback reported as incremental success.

## Frozen Verification Target

### T02-R07

Blocking proof remains unchanged:

- exact incremental observable projection equals fresh independent FullSceneCompiler projection after every true Applied generation;
- all nine V1 kinds and every inherited impact family;
- at least 1000 true Applied revisions per fixed seed;
- at least three documented fixed seeds;
- deterministic replay;
- first mismatch records seed, step, generation, ObjectId when attributable, and divergent component/field.

### T02-R08

Blocking proof remains unchanged:

- deterministic failure injection before/after every required participant preparation boundary represented by the final implementation;
- failed prepare preserves previous published generation and observable projection byte-for-byte;
- stale state, generation gap, dropped/unusable ChangeSet, and deliberate derived-state corruption exercise explicit recovery;
- recovered projection exactly equals independent FullSceneCompiler output from authoritative SemanticReadView;
- recovery is explicitly distinguishable from incremental success;
- failed recovery exposes no partially rebuilt generation.

A2 impact/dependency correctness is inherited upstream and is not an A6 blocking proof. A6 consumption checks are corroborative unless they expose a distinct high-impact failure mode not covered by R07/R08.

## Required Test Identities

P32 must materialize/reconcile these independently runnable CTest identities:

```yaml
canvas_g2_a6_incremental_equivalence:
  obligation: T02-R07
  expected_result: PASS
canvas_g2_a6_atomic_publication:
  obligation: T02-R08
  expected_result: PASS
canvas_g2_a6_recovery:
  obligation: T02-R08
  expected_result: PASS
```

Required predecessor regressions:

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

## Blocking Evidence

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
```

Corroborative only:

```yaml
A6_A2_CONSUMPTION_DIAGNOSTICS:
  blocking: false
A6_FOCUSED_EXISTING_SCENE_REGRESSIONS:
  blocking: false
```

This package does not change `GT-G2-00-ECC-v0.1` or add a new global G2 final evidence-file slot.

## EXECUTION_CLOSURE_CONTRACT

```yaml
EXECUTION_CLOSURE_CONTRACT:
  implementation:
    required_changes:
      - add_or_materialize_A6_runtime_coordination_surface
      - bind_only_existing_canonical_SemanticReadView_and_canvas_semantic_ChangeSet
      - consume_existing_A2_impact_dependency_boundary_without_duplication
      - coordinate_existing_localized_record_spatial_and_invalidation_participants
      - publish_target_generation_only_after_all_required_prepare_succeeds
      - preserve_previous_published_state_on_incremental_prepare_failure
      - add_explicit_full_recovery_through_independent_full_compiler_when_incremental_continuation_is_unsafe
      - preserve_explicit_incremental_vs_recovery_disposition
      - add_deterministic_R07_R08_test_coverage
    forbidden_changes:
      - redefine_semantic_ChangeSet_or_change_kinds
      - duplicate_A2_impact_or_dependency_engine
      - modify_completed_A0_A5_algorithm_ownership
      - silent_recovery_fallback_as_incremental_success
      - semantic_truth_mutation_during_recovery
      - Render_Core_Tile_GPU_Skia_scope
      - persistence_sync_schema_wire_scope
      - Product_latency_or_memory_SLO

  tests:
    required:
      - id: A6_R07_EQUIVALENCE
        command_or_oracle: ctest canvas_g2_a6_incremental_equivalence against independent FullSceneCompiler
        expected_result: PASS
        blocking_reason: T02-R07 integrated incremental drift requires an end-to-end independent oracle
      - id: A6_R08_ATOMIC_PUBLICATION
        command_or_oracle: ctest canvas_g2_a6_atomic_publication with deterministic participant-boundary failure injection
        expected_result: PASS
        blocking_reason: T02-R08 cross-participant half-publish requires coordinator-level proof
      - id: A6_R08_RECOVERY
        command_or_oracle: ctest canvas_g2_a6_recovery against independent FullSceneCompiler
        expected_result: PASS
        blocking_reason: T02-R08 recovery orchestration is A6-owned
      - id: A6_PREDECESSOR_REGRESSION_MATRIX
        command_or_oracle: run frozen predecessor CTest names listed above
        expected_result: PASS
        blocking_reason: A6 must not regress consumed A0-A5 contracts
      - id: SOURCE_HYGIENE
        command_or_oracle: git diff --check
        expected_result: PASS
        blocking_reason: package result must be reviewable and clean

  hosted_verification:
    required: []
    optional:
      - existing repository CI on exact result_revision when available

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
    hosted_CI_not_required_by_this_A6_package: true
    official_Gate_PASS_forbidden_in_P32_return: true

  terminal_success:
    all_of:
      - all_required_implementation_changes_complete
      - all_required_A6_R07_R08_tests_PASS
      - predecessor_regression_matrix_PASS
      - git_diff_check_PASS
      - blocking_evidence_facts_captured_in_structured_execution_return
      - exact_result_revision_pushed_and_reviewer_resolvable
      - no_forbidden_path_or_authority_change

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

## P32 Preflight

Before source mutation, P32 must:

1. resolve repository `Mostorm-Labs/axiom`;
2. resolve this same-repository materialization commit;
3. verify task anchor `4faf351fc85360d0b843dc380c530c12cbb3a48b` is an ancestor of the accepted starting revision;
4. run PackageBindingPreflight against the exact authority/verification refs above;
5. run EvidenceContractPreflight for T02-R07/T02-R08 and the three blocking evidence items;
6. confirm the observed branch has no unauthorized source delta before implementation, or fail closed/reconcile under Aegis rules.

A floating `latest` label, bare SHA without repository identity, or superseded P15/P16/P20/P31 authority is invalid.

## P32 Return Contract

A successful execution return must carry exact identities:

```yaml
task_id: GT-G2-00-A6
status: READY_FOR_CONTROL_REVIEW
package_ref: notion://3db4c57a-590c-8162-ab03-db1eb3fb0ffc/GT-G2-00-A6-P31-v0.2
package_materialization_ref: EXACT_THIS_COMMIT
actual_starting_revision: EXACT_SHA
result_revision: EXACT_SHA
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
blocking_evidence:
  - A6_INCREMENTAL_EQUIVALENCE_EVIDENCE
  - A6_ATOMIC_PUBLICATION_EVIDENCE
  - A6_RECOVERY_EVIDENCE
repository_mutation_scope_conformed: true
next_surface: CONTROL_REVIEW
```

P32 may not emit official P34 PASS.
