# GT-G2-00-A0 P31 Task Package v0.2

Status: READY / P32 NOT AUTHORIZED

This revision supersedes the non-executable v0.1 package. It reconciles the
source allowlist with the repository's actual Scene public boundary while
preserving the frozen P20/P30 contract. It is a documentation-only P31
materialization and does not authorize repository mutation or P32 execution.

## Package identity

```yaml
package_id: GT-G2-00-A0-P31-v0.2
package_ref: notion://3d74c57a590c81269433e00b57ed6e0d/GT-G2-00-A0-P31-v0.2
supersedes: notion://3d74c57a590c81198813e8eb396a7e08/GT-G2-00-A0-P31-v0.1
package_materialization_ref: this documentation-only Git commit (resolved by the control plane after commit)
task_id: GT-G2-00-A0
stage: P31_TASK_PACKAGING
verdict: READY
package_scope: G2-A0_ONLY
repository:
  provider: github
  full_name: Mostorm-Labs/axiom
  canonical_branch: main
  actual_starting_revision: a908c9bc4acb95566f275747884ae26b7a4c9894
  task_anchor:
    revision: a908c9bc4acb95566f275747884ae26b7a4c9894
    relation: ancestor
  worktree_preflight: clean
resume_cursor: null
p32_authorized: false
p34_authorized: false
g2_gate_evaluated: false
g3_authorized: false
repository_mutation_authorized: false
```

## Reconciled source scope

The repository path for the public Scene boundary is
`runtime/scene/include/canvas/scene/`. The v0.1 parent-directory paths were
invalid and therefore could not be used for an executable package.

```yaml
CREATE:
  - runtime/scene/include/canvas/scene/scene_commit_input.hpp
  - runtime/scene/tests/semantic_generation_binding_test.cpp
MODIFY:
  - runtime/scene/include/canvas/scene/scene.hpp
  - runtime/scene/include/canvas/scene/scene_types.hpp
  - runtime/scene/include/canvas/scene/scene_compiler.hpp
  - runtime/scene/include/canvas/scene/scene_binding.hpp
  - runtime/scene/src/scene_binding.cpp
  - runtime/scene/CMakeLists.txt
  - runtime/scene/tests/CMakeLists.txt
```

All other paths are read-only. In particular, `runtime/semantic/`, schema,
wire, persistence, Render Core, Skia/GPU, record-store, spatial-index,
evidence-generator, and authority/Gate files are outside this package. A P32
executor must return `PACKAGE_SCOPE_DIVERGENCE` rather than widen this list.

## Frozen implementation closure

1. Define `SceneCommitInput` using the existing G1 `SemanticGeneration`,
   `ChangeSet`, and `SemanticReadView` types.
2. Bind Scene/SceneBinding/SceneCompiler to post-state
   `SemanticReadView @ after_generation`; full builds consume only
   `SemanticReadView @ G`.
3. Reject stale-before, generation-gap, mismatched-post-view,
   Applied/AlreadyApplied/Rejected inputs without partial publish.
4. Register the focused test and preserve public-header/dependency boundary
   checks. Existing atomic prepare/commit behavior remains required.

The independent test oracle authors baseline G0, valid G0→G1, already-applied,
rejected, stale-before, generation-gap, mismatched-post-view, and expected
published projections. Production binding is not the sole oracle.

## Locked Semantic SDK and CMake inputs

```yaml
dependency_resolution:
  command: >
    python tools/setup_build_environment.py --core --semantic
    --facts-output out/g2-a0-build-environment.json --status
  required_facts:
    - AXIOM_PROTOC
    - AXIOM_SEMANTIC_RUNTIME_ROOT
    - CMAKE_PREFIX_PATH
  locked_release_tag: semantic-sdk-v2-14e3d492c9b7f970
  locked_release_set_id: 14e3d492c9b7f9705dcb89df8dd3f8abbddb7d1bc026bf3084de45bdc317d5ea
  network_allowed: false
  forbidden:
    - .deps/protobuf
    - hard-coded Protobuf_DIR
    - hard-coded absl_DIR
    - hard-coded utf8_range_DIR
    - ad-hoc dependency acquisition
    - semantic-sdk.lock.json modification
cmake:
  source: .
  build_dir: build-g2-a0
  generator: Ninja
  required_options:
    - CANVAS_BUILD_RF01=ON
    - CANVAS_RF01_BUILD_TESTS=ON
    - CANVAS_BUILD_SEMANTIC=ON
    - BUILD_TESTING=ON
  required_binding:
    - resolver AXIOM_PROTOC
    - resolver AXIOM_SEMANTIC_RUNTIME_ROOT
    - resolver CMAKE_PREFIX_PATH
```

The P32 evidence must record the actual resolver paths and release identities;
these are machine facts, not prose defaults.

## Verification and return boundary

Blocking checks are the focused generation/input oracle, public-boundary test,
existing RF-01 atomicity/full-rebuild regression, SDK facts command, and
`git diff --check`. Required evidence inputs are `G2-BOUNDARY.json`,
`G2-CTEST.txt`, and `G2-GATE-MANIFEST.json`. The future executor must return
the exact source revision, changed-path inventory, resolver facts, process
results, and evidence refs. It must not claim G2 or P34 PASS.

```yaml
terminal_blockers:
  - AUTHORITY_CONFLICT
  - MISSING_REQUIRED_INPUT
  - ENVIRONMENT_BLOCKER
  - FROZEN_VERIFICATION_FAILURE
  - NEW_HIGH_IMPACT_FAILURE_MODE
  - PACKAGE_SCOPE_DIVERGENCE
return_policy:
  continue_until_terminal_state: true
next_stage: P32_G2_A0_CODE_EXECUTION_ONLY_AFTER_EXPLICIT_HANDOFF
```

## Authority references

- [G2 RuntimeScene Foundation Implementation Plan v0.2](https://app.notion.com/p/3c84c57a590c8194a0eed8d1e6f79774)
- [GT-G2-00 P20 Verification Design v0.1](https://app.notion.com/p/3d74c57a590c81759f10d3dfff15fd7c)
- [GT-G2-00 P30 Implementation Planning v0.1](https://app.notion.com/p/3d74c57a590c811390b3cc5c0216f8ea)
