# GT-G2-00-A6 P31 Materialized Execution Package v0.4

## Status

```yaml
stage: P31_TARGETED_RECONCILIATION_AND_REPACKAGE
task_id: GT-G2-00-A6
package_id: GT-G2-00-A6-P31-v0.4
reconciliation_subject: RUNTIME_PROJECTION_PUBLICATION_CONVERGENCE
status: MATERIALIZED_FOR_P33_ENTRY_REVIEW
repository_bound: true
preserve_valid_result_revision: eff48f3b5114a15acdab952eb0be0d2a93c981e0
T02_R07_completion_target_changed: false
T02_R08_completion_target_changed: false
P20_reentry_required: false
P30_substantive_replan_required: false
P32_replay_from_scratch_required: false
p34_authorized: false
```

This v0.4 supersedes P31 v0.3 after the targeted P15/P16/P20 v0.3 reconciliation. It preserves the accepted partial implementation at `eff48f3b5114a15acdab952eb0be0d2a93c981e0`; implementation must continue from that cursor rather than replay from the task anchor.

## Package Authority

```yaml
package_ref: notion://3db4c57a-590c-81c5-8775-cf70fdb62788/GT-G2-00-A6-P31-v0.4
repository:
  provider: github
  full_name: Mostorm-Labs/axiom
  canonical_branch: main
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
task_anchor:
  revision: 4faf351fc85360d0b843dc380c530c12cbb3a48b
  relation: ancestor
package_materialization_ref: EXACT_THIS_COMMIT
resume_cursor:
  type: P33_ACCEPTED_PARTIAL_RESULT
  execution_ref: codex/gt-g2-a6-incremental-runtime-coordination
  accepted_revision: eff48f3b5114a15acdab952eb0be0d2a93c981e0
  completed_through:
    - A6_coordinator_surface_materialized
    - canonical_ChangeSet_consumption_basic
    - generation_validation_basic
    - explicit_incremental_vs_recovery_disposition_basic
    - prior_SceneBinding_kRequiresFullRebuild_recovery_path_basic
    - prior_render_prepare_failure_preserves_old_legacy_state
    - test_only_actual_FullSceneCompiler_adapter_compiles_and_runs
    - focused_A6_and_predecessor_matrix_reported_PASS
    - git_diff_check_reported_PASS
  not_yet_closed:
    - canonical_A1_RuntimeScene_publication_path
    - localized_RuntimeScene_prepare_publish_mechanics
    - independent_FullSceneCompiler_materialization_bridge_for_recovery
    - canonical_Scene_Core_atomic_publication_set
    - T02_R07_long_run_nine_kind_exact_equivalence
    - T02_R08_complete_checkpoint_failure_matrix
    - T02_R08_corrupt_state_and_failed_recovery_matrix
  next_action: converge_preserved_A6_implementation_to_P15_P16_P20_v0_3_without_replay
```

`Task Anchor != Execution Cursor`. The cursor is navigation metadata only and does not expand scope or count as Gate evidence.

## Current Authority Bindings

```yaml
P15_current: notion://3db4c57a-590c-81bb-a5a6-f2bd00847c55/GT-G2-00-A6-P15-v0.3
P16_current: notion://3db4c57a-590c-81a1-b2c0-fba8cf76ae9b/GT-G2-00-A6-P16-v0.3
A6_P20_current: notion://3db4c57a-590c-814a-a71c-c97d15f216e0/GT-G2-00-A6-P20-v0.3
global_G2_P20_current: notion://3da4c57a-590c-81e0-b4c9-dad679a95bac/GT-G2-00-P20-v0.3
P30_preserved: notion://3d74c57a-590c-8113-90b3-cc5c0216f8ea/GT-G2-00-P30-v0.1
P23_authority_supersession: notion://3db4c57a-590c-81f4-baa9-f7b53a759701/GT-G2-00-A6-P23-v0.2
P33_control_review: notion://3db4c57a-590c-81d1-86ec-ec5f87789f3b/GT-G2-00-A6-P33-CONTROL-REVIEW-v0.1
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

No Verification strength, corpus size, blocking classification, or global evidence-file slot is changed.

## Repository Reality Bound Into This Package

At the accepted cursor `eff48f3…`:

- `IncrementalRuntimeCoordinator` is still bound to legacy `SceneBinding` for `apply/recover`;
- the production A1 canonical logical scene is `canvas::RuntimeScene` / `canvas::RuntimeSceneProjection` in `scene_types.hpp` and represents all nine V1 semantic kinds;
- current `RuntimeScene::replace/apply` publishes directly and has no separate prepare/publication staging boundary;
- A3 `canvas::scene::FullSceneCompiler` returns the independent inspection/reference projection family;
- the two projection families retain the known `RuntimeSceneProjection` co-include/name conflict;
- A2 impact/dependency functions, BoundsSystem computations, A4 locality work, and A5 SpatialIndex are trusted consumed capabilities and are not redefined here.

Therefore the remaining A6 work is not a test-only patch. The canonical production path must converge to the A1 RuntimeScene publication contract before R07/R08 can close.

## Required Convergence Changes

### Canonical RuntimeScene publication

The blocking A6 path MUST publish the A1 renderer-neutral nine-kind RuntimeScene as the canonical scene observable. Existing SceneBinding/legacy Scene code may remain for compatibility/regression purposes but MUST NOT define R07/R08 canonical truth.

### RuntimeScene prepare -> publication staging

`RuntimeScene` must gain the minimum internal mechanics needed to stage a target generation without publishing it before all required A6 Scene-Core participants have prepared.

Allowed forms include a private/friend prepared projection or equivalent staging object. The resulting implementation MUST:

- preserve the A1 nine-kind logical record contract;
- stage incremental target state from authoritative post-state plus canonical ChangeSet and existing A2-derived affected work;
- stage full target state from the FullSceneCompiler materialization bridge;
- publish only after the A6 publication decision;
- preserve existing A1 replace/apply compatibility, preferably by delegating to the same staging mechanics;
- NOT turn legacy `SceneObjectKind` / `SceneRecord` into a competing nine-kind semantic truth.

### Production FullSceneCompiler materialization bridge

Recovery MUST materially consume the independent A3 FullSceneCompiler result.

Authorized private production topology:

```text
IncrementalRuntimeCoordinator
        |
        | neutral runtime-local materialization DTO
        v
incremental_runtime_full_materialization_bridge.hpp
        ^
        |
incremental_runtime_full_materialization_bridge.cpp
        |
        | actual independent reference computation
        v
canvas::scene::FullSceneCompiler::compile(SemanticReadView)
```

Rules:

1. bridge `.cpp` may include `full_scene_compiler.hpp` and the A3 inspection projection family;
2. bridge header must not expose either conflicting `RuntimeSceneProjection` type;
3. bridge output must carry all facts needed to materialize canonical A1 RuntimeScene and required recovery-derived state: semantic record identity/kind/version/placement/transform/properties/content/erase masks plus reference bounds needed by the selected Bounds/Spatial materialization;
4. bridge must consume the actual FullSceneCompiler result; an unrelated semantic rebuild may not be relabeled as FullSceneCompiler recovery;
5. A3 FullSceneCompiler and `runtime_scene_projection.*` remain read-only.

### Canonical Scene-Core publication set

One A6 transaction stages/publishes at one `SemanticGeneration`:

- canonical A1 RuntimeScene / conforming backing state;
- derived bounds state required by the selected transition;
- production SpatialIndex update;
- generation-bound Scene-Core invalidation output (runtime-local equivalent allowed if no existing type fits);
- every additional real Scene-Core participant actually mutated by the final implementation.

`IRenderScene`, Render-Core DamageTracker, GPU/Skia, FrameState, Tile/RenderGroup, raster and presentation are excluded from canonical A6 publication truth.

### A2 / A4 / A5 preservation

- consume existing A2 `classifyImpact` and dependency-closure semantics; do not implement a second classifier/closure model;
- preserve A4 locality/stable-handle capability; no A4 algorithm rewrite is authorized;
- use existing A5 production SpatialIndex prepare/commit semantics; no spatial backend algorithm mutation is authorized.

### R08 non-public checkpoint seam

Deterministic coordinator-level checkpoints are frozen at:

```text
before RuntimeScene prepare
after RuntimeScene prepare
before Bounds staging
after Bounds staging
before Spatial prepare
after Spatial prepare
before invalidation finalization
after invalidation finalization
before/after every additional actual Scene-Core participant prepare
immediately before publication barrier
```

Exact hook representation is implementation-open but MUST be non-public verification instrumentation. Friend-only test access, private callbacks, an internal header, or equivalent are allowed. A public production debug API is forbidden.

The same non-public test access may deliberately corrupt derived coordinator state for the frozen R08 corrupt-state fixture, but it must not create a general production mutation/debug surface.

## Authorized Mutable Source Scope

### Production existing files

```text
runtime/scene/CMakeLists.txt
runtime/scene/include/canvas/scene/incremental_runtime_coordinator.hpp
runtime/scene/src/incremental_runtime_coordinator.cpp
runtime/scene/include/canvas/scene/scene_types.hpp
runtime/scene/src/scene.cpp
```

`scene_types.hpp` / `scene.cpp` mutation is restricted to canonical `RuntimeScene` staging/publication mechanics and minimal helpers. Legacy `SceneObjectKind`, `SceneRecord`, RF-01 renderer/query/hit/frame semantics and legacy Scene transaction ownership are not A6 mutation targets.

### New private production files allowed

```text
runtime/scene/src/incremental_runtime_full_materialization_bridge.hpp
runtime/scene/src/incremental_runtime_full_materialization_bridge.cpp
runtime/scene/src/incremental_runtime_checkpoint.hpp
```

Use fewer files where possible. The checkpoint header is optional if the seam fits inside existing A6 files.

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

`incremental_runtime_test_access.hpp` is optional and may exist only for friend/private checkpoint and corruption access. It must not become installed/public production API.

Any required mutable path outside this inventory is `PACKAGE_SCOPE_DIVERGENCE` and must fail closed rather than be silently added during P33.

## Read-Only Dependencies / Forbidden Mutation

The executor may read/call but MUST NOT modify:

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

Also forbidden:

- a second semantic ChangeSet, ObjectKind, validator or apply engine;
- a second A2 impact-classification/dependency-closure semantics;
- changing A3 FullSceneCompiler/reference projection to fit incremental output;
- creating a new semantic alias from `SemanticGeneration` to `SceneRevision`;
- routing canonical A6 proof through legacy six-kind SceneRecord/IRenderScene state;
- public production debug/corruption API;
- Render Core, Tile, GPU/Skia, persistence, sync, schema/wire or platform work;
- new Product latency/memory SLOs;
- weakening T02-R07/T02-R08 or adding an 11th final G2 evidence file.

## R07 Blocking Closure

Required CTest identity:

```yaml
canvas_g2_a6_incremental_equivalence:
  obligation: T02-R07
  expected_result: PASS
```

Blocking evidence requires:

- actual authority-conforming A6 canonical RuntimeScene publication path;
- independent A3 FullSceneCompiler from the same authoritative SemanticReadView;
- independently authored comparison-only canonicalization of both sides;
- exact equality after every true Applied generation;
- all nine V1 kinds and every inherited R07 impact family;
- >=1000 true Applied generations per fixed seed;
- >=3 documented fixed seeds;
- deterministic replay;
- first-divergence diagnostics: seed, step, generation, ObjectId when attributable, kind, component/field, incremental value, reference value.

The current lossy digest/smoke invocation at `eff48f3…` is preserved progress but is not R07 closure.

## R08 Atomic-Publication Blocking Closure

Required CTest identity:

```yaml
canvas_g2_a6_atomic_publication:
  obligation: T02-R08
  expected_result: PASS
```

The final test MUST enumerate the actual authority-conforming participant list and inject deterministic failure at every frozen checkpoint. For every pre-publication failure:

```yaml
published_generation_after_failure: MUST_EQUAL_previous_published_generation
canonical_RuntimeScene_after_failure: MUST_EQUAL_previous_published_RuntimeScene
required_derived_state_after_failure: MUST_EQUAL_previous_published_state
partial_participant_visibility: FORBIDDEN
failure_disposition: MUST_BE_EXPLICIT
```

Legacy render/damage rejection coverage may remain corroborative regression evidence; it does not substitute for the canonical checkpoint matrix.

## R08 Recovery Blocking Closure

Required CTest identity:

```yaml
canvas_g2_a6_recovery:
  obligation: T02-R08
  expected_result: PASS
```

Required fixtures:

- dropped or unusable ChangeSet;
- stale derived state;
- semantic generation gap;
- deliberately corrupted derived state through non-public test access;
- failed recovery path.

For every successful recovery:

- actual A3 FullSceneCompiler is invoked;
- recovery materializes canonical RuntimeScene plus required derived state through the production bridge;
- published recovery generation equals the stable authoritative generation;
- recovered canonical observable exactly equals the independent test oracle;
- disposition is explicitly FULL_REBUILD / RECOVERY, never incremental success.

Failed recovery leaves the previous published canonical state unchanged.

## Required Predecessor Regressions

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

All MUST PASS at the exact final result revision. `git diff --check` MUST PASS.

## Evidence Classification

```yaml
blocking:
  - A6_INCREMENTAL_EQUIVALENCE_EVIDENCE   # T02-R07
  - A6_ATOMIC_PUBLICATION_EVIDENCE        # T02-R08
  - A6_RECOVERY_EVIDENCE                  # T02-R08
corroborative:
  - A6_A2_CONSUMPTION_DIAGNOSTICS
  - legacy_SceneBinding_render_damage_regressions
```

No new blocking artifact and no new global evidence-file slot is authorized.

## EXECUTION_CLOSURE_CONTRACT v0.4

```yaml
EXECUTION_CLOSURE_CONTRACT:
  implementation:
    preserve_completed_work_at: eff48f3b5114a15acdab952eb0be0d2a93c981e0
    required_changes:
      - preserve_existing_A6_planning_generation_and_disposition_work_unless_a_verified_defect_requires_change
      - make_canonical_A1_nine_kind_RuntimeScene_the_blocking_A6_published_scene_observable
      - add_minimal_RuntimeScene_staging_and_publication_mechanics_for_cross_participant_atomicity
      - consume_existing_A2_impact_dependency_boundary_for_incremental_affected_work
      - stage_bounds_and_production_spatial_updates_before_publication
      - produce_generation_bound_Scene_Core_invalidation_output_without_Render_Core_ownership
      - add_private_production_FullSceneCompiler_materialization_bridge_and_use_it_for_recovery
      - add_non_public_coordinator_checkpoint_and_corruption_testability_seam
      - upgrade_R07_test_to_exact_long_run_nine_kind_independent_oracle_contract
      - complete_R08_atomic_checkpoint_matrix
      - complete_R08_recovery_matrix_including_corrupt_state_and_failed_recovery
    forbidden_changes:
      - replay_from_task_anchor_or_discard_accepted_eff48f3_work_without_verified_defect
      - modify_A2_A3_A4_A5_algorithm_authority_or_semantic_truth
      - modify_FullSceneCompiler_or_reference_projection_to_match_incremental_output
      - make_legacy_SceneBinding_or_six_kind_SceneRecord_the_canonical_A6_truth
      - public_production_debug_or_corruption_API
      - weaken_R07_or_R08_or_reduce_corpus_strength
      - silently_skip_required_checkpoint_or_recovery_fixture
      - add_new_global_G2_evidence_file_slot

  tests:
    required:
      - id: A6_R07_EQUIVALENCE
        command_or_oracle: ctest canvas_g2_a6_incremental_equivalence against independent A3 FullSceneCompiler after every Applied generation
        expected_result: PASS
        blocking_reason: T02-R07 integrated drift/kind-loss has no other end-to-end independent detector
      - id: A6_R08_ATOMIC_PUBLICATION
        command_or_oracle: ctest canvas_g2_a6_atomic_publication with every frozen coordinator checkpoint injected
        expected_result: PASS
        blocking_reason: T02-R08 cross-participant half-publish is only detected at the A6 transaction boundary
      - id: A6_R08_RECOVERY
        command_or_oracle: ctest canvas_g2_a6_recovery with dropped/stale/gap/corrupt/failed-recovery fixtures and actual production recovery bridge
        expected_result: PASS
        blocking_reason: T02-R08 recovery orchestration/materialization is A6-owned
      - id: A6_PREDECESSOR_REGRESSION_MATRIX
        command_or_oracle: run all predecessor CTest identities frozen above
        expected_result: PASS
        blocking_reason: A6 must preserve consumed A0-A5 and RF-01 contracts
      - id: SOURCE_HYGIENE
        command_or_oracle: git diff --check
        expected_result: PASS
        blocking_reason: final source must remain reviewer-clean

  hosted_verification:
    required: []
    optional:
      - existing repository CI on exact final result revision when available

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
    hosted_CI_not_required_by_this_A6_package: true
    official_Gate_PASS_forbidden_in_execution_return: true

  terminal_success:
    all_of:
      - accepted_eff48f3_partial_work_preserved_except_where_directly_reconciled_to_v0_3_Authority
      - final_changed_paths_conform_to_P31_v0_4_allowlist
      - canonical_A1_RuntimeScene_is_actual_A6_published_observable
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
2. resolve this exact same-repository package materialization commit;
3. verify task anchor `4faf351f…` and accepted cursor `eff48f3…` ancestry;
4. verify the descendant delta after `eff48f3…` is package-materialization-only;
5. run PackageBindingPreflight against P15/P16/P20 v0.3 and the exact Verification bindings above;
6. run EvidenceContractPreflight for the three blocking A6 evidence obligations;
7. verify there is no unauthorized source delta before mutation.

Expected resume classification after package-only materialization: `DESCENDANT_CURSOR`.

A floating `latest`, superseded Authority/P20/P31 ref, unresolved package ref, or non-package source delta before P33 is fail-closed.

## Return Contract After Resumed Execution

```yaml
task_id: GT-G2-00-A6
status: READY_FOR_CONTROL_REVIEW | BLOCKED
package_ref: notion://3db4c57a-590c-81c5-8775-cf70fdb62788/GT-G2-00-A6-P31-v0.4
package_materialization_ref: EXACT_V0_4_MATERIALIZATION_SHA
resume_cursor_used: eff48f3b5114a15acdab952eb0be0d2a93c981e0
actual_resumed_starting_revision: EXACT_SHA
result_revision: EXACT_SHA
execution_branch: codex/gt-g2-a6-incremental-runtime-coordination
repository_mutation_scope_conformed: true | false
blocking_evidence:
  A6_INCREMENTAL_EQUIVALENCE_EVIDENCE: PASS | FAIL | NOT_RUN
  A6_ATOMIC_PUBLICATION_EVIDENCE: PASS | FAIL | NOT_RUN
  A6_RECOVERY_EVIDENCE: PASS | FAIL | NOT_RUN
predecessor_regressions: PASS | FAIL | NOT_RUN
git_diff_check: PASS | FAIL | NOT_RUN
terminal_blocker: null | EXPLICIT_CLASS
next_surface: CONTROL_REVIEW
```

P33/P32 execution MUST NOT emit or imply official P34 PASS.
