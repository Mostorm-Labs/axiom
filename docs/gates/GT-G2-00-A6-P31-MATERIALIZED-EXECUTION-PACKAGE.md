# GT-G2-00-A6 P31 Materialized Execution Package v0.5

## Status

```yaml
stage: P31_TARGETED_REPACKAGE
task_id: GT-G2-00-A6
package_id: GT-G2-00-A6-P31-v0.5
reconciliation_subject: A6_R07_CANONICAL_COMPARISON_DOMAIN_BINDING
status: MATERIALIZED_FOR_P33_ENTRY_REVIEW
repository_bound: true
supersedes_package: GT-G2-00-A6-P31-v0.4
preserve_valid_result_revision: e709ce02e2091757e9c7fbf4632bc3218fda6942
P20_reentry_required: false
P30_substantive_replan_required: false
P32_replay_from_scratch_required: false
p34_authorized: false
```

This v0.5 supersedes P31 v0.4 only because A6 P20 v0.4 froze the exact R07 canonical comparison domain. Preserve the accepted result at `e709ce02e2091757e9c7fbf4632bc3218fda6942`; do not replay A6. The stale whole-DTO A1-vs-A3 comparison is replaced by an explicit comparison-only canonicalization over the frozen A1 structural domain. Bounds/dependency/spatial/invalidation correctness remains covered through participant-specific evidence.

## Package Authority

```yaml
package_ref: notion://3dc4c57a-590c-81ce-8c78-d4fb1c68d007/GT-G2-00-A6-P31-v0.5
repository:
  provider: github
  full_name: Mostorm-Labs/axiom
  canonical_branch: main
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
materialization_branch: aegis/gt-g2-a6-p31-v0-5-materialization
task_anchor:
  revision: 4faf351fc85360d0b843dc380c530c12cbb3a48b
  relation: ancestor
package_materialization_ref: EXACT_THIS_COMMIT
resume_cursor:
  type: P33_ACCEPTED_PARTIAL_RESULT
  execution_ref: codex/gt-g2-a6-incremental-runtime-coordination
  accepted_revision: e709ce02e2091757e9c7fbf4632bc3218fda6942
  completed_through:
    - preserved_v0_4_A6_coordinator_and_generation_work
    - canonical_A1_RuntimeScene_prepare_publish_surface_present
    - production_full_materialization_bridge_invokes_actual_A3_FullSceneCompiler
    - recovery_path_materializes_A3_result_into_RuntimeScene_staging
    - non_public_checkpoint_and_corruption_test_access_present
    - normal_incremental_RED_path_no_longer_materializes_RuntimeScene_through_A3
    - P36_RED_experiment_reproduced_first_R07_divergence_under_stale_whole_DTO_comparison
    - execution_branch_scope_conformed_through_e709ce02
  not_yet_closed:
    - R07_comparison_adapter_bound_to_exact_P20_v0_4_A1_structural_domain
    - participant_specific_bounds_correctness_separated_from_A1_structural_equality
    - A6_actual_Bounds_Spatial_invalidation_participant_staging_and_publication_where_required
    - T02_R07_long_run_all_kind_all_impact_family_closure
    - T02_R08_complete_actual_participant_checkpoint_matrix
    - T02_R08_corrupt_state_and_failed_recovery_matrix_formal_closure
    - exact_final_result_and_blocking_evidence_materialization_for_control_review
  next_action: reconcile_R07_comparison_to_P20_v0_4_then_finish_remaining_A6_participant_evidence_without_replay
```

`Task Anchor != Execution Cursor`. The package materialization branch is documentation-only and MUST NOT replace the execution cursor.

## Current Authority Bindings

```yaml
P15_current: notion://3db4c57a-590c-81bb-a5a6-f2bd00847c55/GT-G2-00-A6-P15-v0.3
P16_current: notion://3db4c57a-590c-81a1-b2c0-fba8cf76ae9b/GT-G2-00-A6-P16-v0.3
A6_P20_current: notion://3dc4c57a-590c-81fb-9fd8-cec20062874c/GT-G2-00-A6-P20-v0.4
global_G2_P20_current: notion://3da4c57a-590c-81e0-b4c9-dad679a95bac/GT-G2-00-P20-v0.3
P30_preserved: notion://3d74c57a-590c-8113-90b3-cc5c0216f8ea/GT-G2-00-P30-v0.1
fresh_routing: notion://3dc4c57a-590c-81a6-ab9d-f2994952590b/GT-G2-00-A6-AEGIS-FRESH-ROUTING-v0.3
P36_task_package_defect: notion://3dc4c57a-590c-81ce-91da-d4a1f4dc728d/GT-G2-00-A6-P36-v0.2
```

Verification bindings remain:

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

No global Verification strength, evidence-file count, R08 wording, P15, P16, or P30 topology is changed by this package.

## Repository Reality Bound Into v0.5

At accepted cursor `e709ce02...`:

- normal `IncrementalRuntimeCoordinator::apply` stages `RuntimeScene` through `RuntimeScene::prepare(post_state)`, not A3 materialization;
- recovery materially uses the private production bridge that calls A3 `FullSceneCompiler`;
- `RuntimeSceneRecord` currently contains A1 semantic fields plus extra derived/inspection-like fields;
- `projectRuntimeScene(...)` fills the A1 semantic fields and leaves the extra derived fields default/empty;
- the current R07 test compares every field, including bounds, `referenceGeometryDigest`, and `directDependencies`;
- the independent A3 adapter supplies those richer fields from FullSceneCompiler;
- public `computeBounds(...)` is available as trusted bounds derivation;
- A3-private digest and direct-dependency derivations remain inspection/oracle internals.

The first incomplete step is therefore to correct the R07 comparison contract to P20 v0.4, not to restore A3 as the normal incremental source.

## Exact R07 A1 Structural Comparison Domain

After every true Applied generation:

```text
CanonicalizeA1(A6 published RuntimeScene @ G)
==
CanonicalizeA1(A3 independent FullSceneCompiler reference @ G)
```

The exact structural domain is:

```yaml
projection:
  generation: SemanticGeneration
  records:
    membership: exact
    allowed_kinds:
      - Shape
      - Image
      - VectorPath
      - RichText
      - VectorStroke
      - DabStroke
      - Connector
      - Sticky
      - Group
    page: absent_or_rejected
per_record:
  - ObjectId
  - ObjectKind
  - KindVersion
  - Placement
  - Transform2D
  - PropertyBag
  - ObjectContent
  - EraseMasks
```

Comparison may normalize deterministic record/container ordering only. It MUST NOT backfill the incremental side from `SemanticReadView`, read expected values from the incremental side, or add fields merely because they exist in a current/future C++ struct.

The following are explicitly outside A1 structural equality:

```yaml
A3_inspection_only_for_R07_structural_comparison:
  - GeometryBounds
  - VisualBounds
  - WorldBounds
  - referenceGeometryDigest
  - directDependencies
  - A3_specific_reference_geometry_representation
```

A direct `RuntimeSceneRecord::operator==` / whole-DTO comparison is insufficient unless both sides are first canonicalized onto the explicit A1 domain above.

## Derived Correctness Binding Retained

Exclusion from A1 structural equality does not remove correctness obligations:

- **Bounds:** actual A6 bounds-derived state is checked against A3 full-scan bounds and/or trusted `computeBounds`. If extra RuntimeScene bounds fields are used as physical backing, they are derived participant state, not A1 structural identity.
- **Dependency:** A2 remains owner of dependency derivation. A6 consumes the established A2 evaluation boundary; A3 `directDependencies` remains oracle/inspection data and creates no A1 storage requirement. A6 MUST NOT invent a second dependency algorithm.
- **Spatial:** production SpatialIndex state remains bound to the existing A5 independent differential/oracle contract. No spatial backend algorithm change is authorized.
- **Reference geometry:** A3 digest/representation remains inspection-only unless an actual A6 geometry-derived participant exists. Do not duplicate A3-private derivation merely for R07 equality.
- **Invalidation:** generation-bound Scene-Core invalidation output remains an A6 participant when the selected transition mutates it and remains under existing participant/A6 verification obligations.

## Required Implementation Reconciliation

1. Preserve the `e709ce02...` RED independence correction. Normal incremental RuntimeScene staging MUST continue to avoid A3 FullSceneCompiler/full materialization.
2. Replace stale whole-DTO R07 equality with comparison-only canonicalization over the exact A1 structural domain.
3. Keep derived participant checks. Do not make R07 green by deleting bounds/spatial/invalidation/A2-consumption verification.
4. Preserve recovery independence: A3 FullSceneCompiler -> private A6 recovery materialization bridge -> canonical RuntimeScene + required derived participants -> atomic recovery publication.
5. Finish remaining v0.4 participant closure without replay: canonical A1 publication, actual required Bounds/Spatial/invalidation staging, generation-coherent publication, R08 actual-participant checkpoint matrix, corrupt-state and failed-recovery fixtures, and predecessor regressions.

## Authorized Mutable Source Scope

### Production existing files

```text
runtime/scene/CMakeLists.txt
runtime/scene/include/canvas/scene/incremental_runtime_coordinator.hpp
runtime/scene/src/incremental_runtime_coordinator.cpp
runtime/scene/include/canvas/scene/scene_types.hpp
runtime/scene/src/scene.cpp
```

`scene_types.hpp` / `scene.cpp` may change only for A6 RuntimeScene staging/publication and derived participant backing needed by this package. Removing extra inspection-like fields from `RuntimeSceneRecord` is not required by P20 v0.4; if retained, they MUST NOT define A1 structural equality.

### Private production files

```text
runtime/scene/src/incremental_runtime_full_materialization_bridge.hpp
runtime/scene/src/incremental_runtime_full_materialization_bridge.cpp
runtime/scene/src/incremental_runtime_checkpoint.hpp
```

The full materialization bridge remains recovery-only for A3 materialization. The checkpoint header remains optional.

### A6 tests and test-only support

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

Any required path outside this inventory is `PACKAGE_SCOPE_DIVERGENCE` and MUST fail closed rather than be silently added during P33.

## Read-Only Dependencies / Forbidden Mutation

Read/call but do not modify:

```text
runtime/semantic/**
runtime/scene/include/canvas/scene/scene_commit_input.hpp
runtime/scene/include/canvas/scene/scene_compiler.hpp
runtime/scene/include/canvas/scene/scene_binding.hpp
runtime/scene/src/scene_binding.cpp
runtime/scene/include/canvas/scene/scene.hpp
runtime/scene/include/canvas/scene/runtime_scene_projection.hpp
runtime/scene/src/runtime_scene_projection.cpp
runtime/scene/include/canvas/scene/full_scene_compiler.hpp
runtime/scene/src/full_scene_compiler.cpp
runtime/scene/include/canvas/scene/bounds_system.hpp
runtime/scene/src/bounds_system.cpp
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
```

Forbidden:

- reintroducing A3 FullSceneCompiler/materialization into normal incremental RuntimeScene construction;
- copying/reimplementing A3-private digest or dependency derivation into A6 merely to match the oracle;
- modifying A3 reference projection to fit incremental output;
- treating current struct membership as verification authority;
- introducing a second semantic ChangeSet/ObjectKind/apply engine or A2 dependency semantics;
- making legacy six-kind SceneRecord/IRenderScene canonical A6 truth;
- public production debug/corruption API;
- Render Core/Tile/GPU/Skia/persistence/sync/schema/platform work;
- weakening R07/R08 or reducing corpus/checkpoint/recovery strength;
- adding a new global G2 evidence-file slot.

## Required Tests / Oracles

### A6_R07_EQUIVALENCE

```yaml
id: A6_R07_EQUIVALENCE
ctest: canvas_g2_a6_incremental_equivalence
obligation: T02-R07
expected_result: PASS
```

Requirements:

- at least 3 documented fixed seeds;
- at least 1000 true Applied transitions per seed;
- all nine V1 kinds;
- all inherited R07 impact families;
- comparison after every true Applied generation;
- structural equality uses only the exact A1 domain above;
- incremental side is the actual A6-published RuntimeScene;
- reference side is independent A3 FullSceneCompiler from the same SemanticReadView;
- deterministic replay;
- first divergence records seed, step, generation, ObjectId when attributable, kind, domain, component/field, incremental value, reference value, comparison method.

A direct whole-DTO equality, sequence-end-only comparison, smoke invocation, six-kind comparison, or semantic-post-state backfill is insufficient.

### A6_R07_DERIVED_PARTICIPANTS

Required in the same long-run corpus or deterministically linked blocking fixtures:

```yaml
bounds:
  actual: actual_A6_published_or_staged_bounds_participant
  reference: A3_full_scan_bounds_or_trusted_computeBounds
  classification: blocking_when_applicable_to_selected_participant
spatial:
  actual: production_A6_SpatialIndex_state
  reference: preserved_A5_independent_spatial_oracle_contract
  classification: blocking
A2_dependency_consumption:
  actual: A6_consumes_existing_A2_evaluation_boundary
  reference: frozen_A2_dependency_closure_contract
  classification: blocking_by_upstream_binding
invalidation:
  actual: generation_bound_Scene_Core_invalidation_output_when_mutated
  reference: frozen_participant_or_A6_oracle
  classification: blocking_when_participant_present
```

No `referenceGeometryDigest` or `directDependencies` storage is required in A1 to satisfy this section.

### A6_R08_ATOMIC_PUBLICATION

```yaml
id: A6_R08_ATOMIC_PUBLICATION
ctest: canvas_g2_a6_atomic_publication
obligation: T02-R08
expected_result: PASS
```

Inject deterministic failure at every actual required pre-publication participant boundary and immediately before publication. For every pre-publication failure, previous published RuntimeScene/generation/derived participant state remains exactly visible and partial publication is forbidden.

### A6_R08_RECOVERY

```yaml
id: A6_R08_RECOVERY
ctest: canvas_g2_a6_recovery
obligation: T02-R08
expected_result: PASS
```

Fixtures remain: dropped/unusable ChangeSet, stale derived state, semantic generation gap, deliberately corrupted derived state through non-public test access, and failed recovery. Successful recovery materially invokes A3 FullSceneCompiler through the production bridge and publishes coherent canonical/derived state. Failed recovery preserves previous publication.

### Predecessor regressions

All MUST PASS at exact final result revision:

```text
canvas_g2_a0_semantic_generation_binding
canvas_g2_a1_runtime_scene_projection
canvas_g2_a2_scene_impact
canvas_g2_a2_dependency_closure
canvas_g2_a3_full_incremental_equivalence
canvas_g2_a4_scene_delta
canvas_g2_a5_spatial_index_differential
canvas_rf01_scene_atomicity
canvas_rf01_scene_binding
```

`git diff --check` MUST PASS.

## Evidence Classification

```yaml
blocking:
  - A6_INCREMENTAL_EQUIVALENCE_EVIDENCE
  - A6_ATOMIC_PUBLICATION_EVIDENCE
  - A6_RECOVERY_EVIDENCE
corroborative:
  - A6_A2_CONSUMPTION_DIAGNOSTICS
  - legacy_SceneBinding_render_damage_regressions
```

No new blocking artifact family and no new global evidence-file slot are authorized.

## EXECUTION_CLOSURE_CONTRACT v0.5

```yaml
EXECUTION_CLOSURE_CONTRACT:
  implementation:
    preserve_completed_work_at: e709ce02e2091757e9c7fbf4632bc3218fda6942
    required_changes:
      - preserve_e709ce_RED_independence_correction_and_all_compatible_prior_A6_work
      - bind_R07_structural_comparison_to_explicit_P20_v0_4_A1_domain_only
      - keep_A3_bounds_digest_dependencies_as_inspection_or_participant_oracle_data_not_A1_structural_fields
      - preserve_or_complete_actual_A6_bounds_spatial_invalidation_participant_staging_and_atomic_publication
      - retain_existing_A2_dependency_ownership_and_consume_its_evaluation_boundary
      - keep_A3_full_materialization_bridge_recovery_only
      - complete_R07_long_run_nine_kind_all_impact_family_evidence
      - complete_R08_actual_participant_checkpoint_matrix
      - complete_R08_recovery_matrix_including_corrupt_state_and_failed_recovery
    forbidden_changes:
      - replay_from_main_task_anchor_eff48f3_a69e91_or_bd41068
      - restore_A3_materialization_as_normal_incremental_RuntimeScene_source
      - require_referenceGeometryDigest_or_directDependencies_as_A1_structural_equality_fields
      - duplicate_A3_private_digest_or_dependency_derivation_in_A6
      - remove_derived_correctness_checks_merely_to_make_R07_green
      - modify_A2_A3_A4_A5_algorithm_authority_or_semantic_truth
      - weaken_R07_or_R08_or_reduce_corpus_strength
      - add_new_global_G2_evidence_file_slot

  tests:
    required:
      - id: A6_R07_EQUIVALENCE
        command_or_oracle: canvas_g2_a6_incremental_equivalence using explicit P20 v0.4 A1 canonicalization after every true Applied generation
        expected_result: PASS
        blocking_reason: closes integrated long-run structural drift and kind-loss while preserving independent A3 reference
      - id: A6_R07_DERIVED_PARTICIPANTS
        command_or_oracle: participant-specific bounds/spatial/invalidation checks plus frozen A2 dependency-consumption binding
        expected_result: PASS
        blocking_reason: prevents structural-domain clarification from deleting derived correctness coverage
      - id: A6_R08_ATOMIC_PUBLICATION
        command_or_oracle: canvas_g2_a6_atomic_publication against every actual required participant checkpoint
        expected_result: PASS
        blocking_reason: closes cross-participant half-publish failure mode
      - id: A6_R08_RECOVERY
        command_or_oracle: canvas_g2_a6_recovery with dropped/stale/gap/corrupt/failed-recovery fixtures and actual A3 bridge
        expected_result: PASS
        blocking_reason: closes A6-owned recovery and materialization failure modes
      - id: A6_PREDECESSOR_REGRESSION_MATRIX
        command_or_oracle: run all frozen predecessor CTest identities
        expected_result: PASS
        blocking_reason: preserves A0-A5 and RF-01 consumed contracts
      - id: SOURCE_HYGIENE
        command_or_oracle: git diff --check
        expected_result: PASS
        blocking_reason: reviewer-clean final source

  hosted_verification:
    required: []
    optional:
      - repository_CI_on_exact_final_result_revision_when_available

  evidence:
    blocking:
      - A6_INCREMENTAL_EQUIVALENCE_EVIDENCE
      - A6_ATOMIC_PUBLICATION_EVIDENCE
      - A6_RECOVERY_EVIDENCE
    corroborative:
      - A6_A2_CONSUMPTION_DIAGNOSTICS
      - legacy_SceneBinding_render_damage_regressions

  durable_result_boundary:
    repository_branch_must_be_pushed: codex/gt-g2-a6-incremental-runtime-coordination
    exact_result_revision_required: true
    reviewer_resolvable_source_required: true
    package_materialization_must_be_resolvable_before_resume: true
    official_Gate_PASS_forbidden_in_execution_return: true

  terminal_success:
    all_of:
      - e709ce_valid_work_preserved_except_verified_reconciliation_changes
      - final_changed_paths_conform_to_P31_v0_5_allowlist
      - normal_incremental_A6_does_not_generate_RuntimeScene_via_A3
      - exact_A1_structural_domain_equal_to_independent_A3_reference_after_every_required_Applied_generation
      - required_derived_participant_correctness_checks_closed_without_A1_digest_dependency_storage_requirement
      - canonical_A6_participants_publish_one_generation_atomically
      - recovery_materially_uses_independent_A3_FullSceneCompiler_result
      - A6_R07_blocking_evidence_closed
      - A6_R08_atomic_blocking_evidence_closed
      - A6_R08_recovery_blocking_evidence_closed
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
      - BLOCKED_EXECUTION_DIVERGENCE

  return_policy:
    continue_until_terminal_state: true
```

## P33 Entry Preflight

Before source mutation P33 MUST:

1. resolve repository identity `github / Mostorm-Labs/axiom`;
2. resolve this exact same-repository v0.5 package materialization commit;
3. verify task anchor `4faf351f...` is an ancestor of `e709ce02...` and the execution branch still resolves at or as a valid descendant of the accepted cursor;
4. classify `EXACT_CURSOR` or `DESCENDANT_CURSOR` from the execution branch, not from the package-materialization branch;
5. run PackageBindingPreflight against P15 v0.3, P16 v0.3, A6 P20 v0.4, and global P20 v0.3;
6. run EvidenceContractPreflight for the three blocking A6 evidence families;
7. inspect only the delta after `e709ce02...`; do not replay earlier A6 work;
8. fail closed on any non-package source divergence outside the v0.5 allowlist.

## Return Contract After Resumed Execution

Execution return carries exact identities/navigation only. Machine proof facts remain owned by EvidenceArtifact/provider observations.

```yaml
task_id: GT-G2-00-A6
status: READY_FOR_CONTROL_REVIEW | BLOCKED
package_ref: notion://3dc4c57a-590c-81ce-8c78-d4fb1c68d007/GT-G2-00-A6-P31-v0.5
package_materialization_ref: EXACT_V0_5_MATERIALIZATION_SHA
resume_cursor_used: e709ce02e2091757e9c7fbf4632bc3218fda6942
actual_resumed_starting_revision: EXACT_SHA
result_revision: EXACT_SHA
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
materialized_ref: reviewer_resolvable_exact_result_ref
evidence_input_refs:
  - exact_evidence_input_refs
provider_run_refs:
  - exact_provider_refs_when_applicable
terminal_blocker: null | EXPLICIT_CLASS
next_surface: CONTROL_REVIEW
```

P33/P32 execution MUST NOT emit or imply official P34 PASS.
