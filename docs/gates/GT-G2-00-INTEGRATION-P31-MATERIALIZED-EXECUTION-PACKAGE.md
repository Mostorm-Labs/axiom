# GT-G2-00-INTEGRATION P31 Materialized Execution Package v0.1

## Status

```yaml
stage: P31_TASK_PACKAGING
task_id: GT-G2-00-INTEGRATION
package_id: GT-G2-00-INTEGRATION-P31-v0.1
status: MATERIALIZED_FOR_P32_ENTRY
repository_bound: true
execution_authority_mode: hybrid
P32_authorized_after_identity_preflight: true
G2_PASS: false
G3_ingress_authorized: false
```

This package authorizes only repository integration of the already Gate-accepted A6 result with the already Gate-accepted A5 D6 harness repair. It does not reopen A5/A6, redesign Verification, claim G2 PASS, or authorize G3.

## Package Authority

```yaml
package_ref: notion://3dd4c57a-590c-8116-8fc7-d6d1e60d2933/GT-G2-00-INTEGRATION-P31-v0.1
repository:
  provider: github
  full_name: Mostorm-Labs/axiom
  canonical_branch: main
canonical_main_at_packaging: 4faf351fc85360d0b843dc380c530c12cbb3a48b
materialization_branch: aegis/gt-g2-integration-p31-v0-1-materialization
package_materialization_ref: EXACT_THIS_COMMIT
execution_branch_to_create: codex/gt-g2-unified-integration-candidate
branch_creation_base_must_equal: 543e16e30e53da7568605827bfb51c42a84f6558
task_anchor:
  revision: 543e16e30e53da7568605827bfb51c42a84f6558
  relation: ancestor
resume_cursor: null
return_surface: CONTROL_REVIEW
continue_until_terminal_state: true
```

Repository identity is blocking. P32 must resolve this package from the same repository before mutation and fail closed on missing, mismatched, ambiguous, or unavailable identity. The execution branch must be created from the exact task-anchor revision before any functional mutation; after creation, descendants of the anchor are valid execution states.

## Frozen Inputs

```yaml
A6_gate_accepted_revision: 543e16e30e53da7568605827bfb51c42a84f6558
A6_source_branch: codex/gt-g2-a6-incremental-runtime-coordination
A5_gate_accepted_revision: e2bfa1606e87481a1984f6439afb84545d229d25
A5_delta_path: runtime/scene/tests/CMakeLists.txt
A5_delta_required_effect: "--output <path> -> --output=<path>"
verification_spec_id: GT-G2-00-VS-v0.3
obligation_set_id: GT-G2-00-OBL-v0.3
trusted_basis_id: GT-G2-00-TB-v0.3
scope_contract_id: GT-G2-00-SCOPE-v0.3
acceptance_oracle_id: GT-G2-00-ORACLE-v0.3
corpus_id: GT-G2-00-CORPUS-v0.2
evidence_compilation_contract_id: GT-G2-00-ECC-v0.1
```

Current Authority refs:

- G2 repository integration routing: `notion://3dd4c57a-590c-81aa-a150-c0dc18a3a6f4`
- G2 Completion Gate Review v0.1: `notion://3dd4c57a-590c-8169-98a4-c3821bb57348`
- A5 P34 Gate Review PASS: `notion://3dd4c57a-590c-8150-a5fe-e002a4f4b8ac`
- A6 P34 Gate Review PASS: `notion://3dd4c57a-590c-81bd-a5ce-ebfca89f960c`
- G2 P20 v0.3: `notion://3da4c57a-590c-81e0-b4c9-dad679a95bac`
- A6 P20 v0.4: `notion://3dc4c57a-590c-81fb-9fd8-cec20062874c`

## Authorized Mutation Scope

Only one functional path is authorized:

```text
runtime/scene/tests/CMakeLists.txt
```

Required final invocation:

```cmake
--output=${CMAKE_CURRENT_BINARY_DIR}/GT-G2-00-A5-D6-Performance-Evidence-v0.1.json
```

The functional diff from exact A6 result `543e16e30e53da7568605827bfb51c42a84f6558` must contain no other behavior change. Integration-control artifacts such as the execution branch, commit, PR, and returned evidence metadata are allowed but do not expand functional scope.

## Non-goals / Forbidden Changes

- no production semantic changes;
- no A5 spatial algorithm/API changes;
- no A6 coordinator/runtime/publication changes;
- no P15/P16/P20 or Verification contract change;
- no unrelated refactor or formatting cleanup;
- no new completion criteria/evidence slots;
- no direct merge to `main` during P32;
- no A5/A6 Gate reopening;
- no G2 PASS or G3 authorization from implementation.

## EXECUTION_CLOSURE_CONTRACT

```yaml
EXECUTION_CLOSURE_CONTRACT:
  implementation:
    required_changes:
      - create codex/gt-g2-unified-integration-candidate from exact 543e16e30e53da7568605827bfb51c42a84f6558
      - apply exact A5 accepted D6 CTest invocation effect from e2bfa1606e87481a1984f6439afb84545d229d25
      - ensure functional diff from A6 exact result is restricted to runtime/scene/tests/CMakeLists.txt
      - push one exact unified candidate revision
      - create or update a reviewer-resolvable PR without merging it
    forbidden_changes:
      - production semantic changes
      - A5 spatial algorithm or API changes
      - A6 coordinator/runtime/publication changes
      - authority or Verification changes
      - unrelated refactors
      - direct merge to main
      - new completion criteria
  tests:
    required:
      - id: PACKAGE_IDENTITY
        oracle: compare exact result against 543e16e30e53da7568605827bfb51c42a84f6558
        expected: only accepted A5 harness functional delta plus authorized integration-control artifacts
      - id: BUILD_RF01
        command: cmake --preset rf01-host-debug && cmake --build --preset rf01-host-debug
        expected: PASS
      - id: A5_D6_REGISTERED_BENCHMARK
        command: ctest --preset rf01-host-debug -R '^canvas_g2_a5_d6_spatial_index_benchmark$' --output-on-failure
        expected: PASS
      - id: A6_INCREMENTAL_EQUIVALENCE
        command: ctest --preset rf01-host-debug -R '^canvas_g2_a6_incremental_equivalence$' --output-on-failure
        expected: PASS
      - id: A6_ATOMIC_PUBLICATION
        command: ctest --preset rf01-host-debug -R '^canvas_g2_a6_atomic_publication$' --output-on-failure
        expected: PASS
      - id: A6_RECOVERY
        command: ctest --preset rf01-host-debug -R '^canvas_g2_a6_recovery$' --output-on-failure
        expected: PASS
      - id: RF01_RELEVANT_REGRESSION
        command: ctest --preset rf01-host-debug --output-on-failure
        expected: PASS
      - id: DIFF_HYGIENE
        command: git diff --check 543e16e30e53da7568605827bfb51c42a84f6558..HEAD
        expected: PASS
  hosted_verification:
    required:
      - exact remote result revision is reviewer-resolvable
      - PR head equals returned result_revision
      - report applicable hosted CI; if frozen required hosted checks cannot run for provider/environment reasons, return ENVIRONMENT_BLOCKER
    optional: []
  evidence:
    blocking:
      - exact result_revision
      - exact PR URL/number and head SHA
      - exact compare against A6 accepted revision
      - focused A5/A6 test results
      - full rf01-host-debug regression result
      - git diff --check result
    corroborative:
      - additional hosted checks not required by frozen G2 Verification
  terminal_success:
    all_of:
      - package materialization ref resolves in Mostorm-Labs/axiom
      - execution branch starts from exact A6 accepted revision
      - accepted A5 harness effect is present
      - no unauthorized functional diff exists
      - all required local tests pass
      - exact result revision is pushed and reviewer-resolvable
      - PR head equals exact result revision
      - return contains result_revision, materialized_ref, evidence_input_refs, and hosted provider refs where applicable
  terminal_blockers:
    explicit_classes:
      - AUTHORITY_CONFLICT
      - MISSING_REQUIRED_INPUT
      - BLOCKED_REPOSITORY_IDENTITY
      - PACKAGE_SCOPE_DIVERGENCE
      - ENVIRONMENT_BLOCKER
      - FROZEN_VERIFICATION_FAILURE
      - NEW_HIGH_IMPACT_FAILURE_MODE
  return_policy:
    continue_until_terminal_state: true
```

## P32 Return Contract

Successful code-surface execution returns `READY_FOR_CONTROL_REVIEW`, never Gate PASS.

Required exact return fields:

```yaml
result_revision: <exact pushed SHA>
execution_branch: codex/gt-g2-unified-integration-candidate
materialized_ref: <reviewer-resolvable PR/commit ref>
evidence_input_refs:
  package_compare: <exact ref/result>
  focused_ctest: <exact log/ref>
  full_rf01_ctest: <exact log/ref>
  diff_check: <exact log/ref>
hosted_provider_refs: <exact runs/checks or explicit not-applicable/provider blocker>
continue_execution: false
next_surface: CONTROL_REVIEW
next_stage: G2_COMPLETION_GATE_REVIEW
```

Gate Closure Stability applies. Do not broaden the finish line during P32.