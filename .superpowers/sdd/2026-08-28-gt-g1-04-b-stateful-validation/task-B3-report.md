# B3 hierarchy validation report

- Task: B3 staged hierarchy, parent references, and cycle validation
- Authorization: `3ca4c57a-590c-8101-a11d-f1d4c24ebb65`
- Plan/base commit: `91314a05008e2e7357414958779221cdbb7aeaf1`
- Branch: `codex/gt-g1-04-operation-apply`
- Source commit: `fa7f9a0ba6b47ae1263c3b1b0138846f40a914b6`

## RED

Command (before production header/source):
`cmake -S . -B /tmp/axiom-b3-build.L5q7t2 -DCANVAS_BUILD_SEMANTIC=ON -DBUILD_TESTING=ON -DCANVAS_SEMANTIC_ENABLE_PROTOBUF=OFF -DCANVAS_BUILD_POC01=OFF -DCANVAS_POC01_BUILD_TESTS=OFF && cmake --build /tmp/axiom-b3-build.L5q7t2 --target canvas_semantic_hierarchy_stateful_validation_test -j2`

Observed failure (expected): `fatal error: 'canvas/semantic/hierarchy_validation.hpp' file not found`; build exited 2.

## GREEN and regression

Focused build command: `cmake -S . -B <fresh /tmp/axiom-b3-build> ... && cmake --build <build> --target canvas_semantic_hierarchy_stateful_validation_test -j2` — PASS.

Focused runtime: `canvas_semantic_hierarchy_stateful_validation_test --gtest_color=no` — PASS, 5/5 tests.

Regression/full-suite: semantic CTest and requested B0/B1/B2/store/A filters were not run in this worker (SKIPPED); no failures observed in executed checks.

Boundary: `python3 tools/check_runtime_boundaries.py --root .` — PASS (`48 contract files`).

Docs/scope: `git diff --check` — PASS. Existing untracked `build-b2/`, `build-semantic-b2/`, and Android visual-smoke data were untouched.

## Observations

The implementation projects all edits before parent-reference and complete-topology checks, rejects missing/deleted edited objects and parents, detects self/two-node/general cycles independent of edit order, and traverses descendants through `StagedObjectView::children` with deterministic child order and visited-ID cycle defense. Null parent is treated as document root. No `allObjects()` call is made by validation or descendant traversal. Reference and Indexed stores produced the same decision; base projections and Indexed rebuild equivalence remained unchanged.

## Files changed

`runtime/semantic/include/canvas/semantic/hierarchy_validation.hpp`
`runtime/semantic/src/hierarchy_validation.cpp`
`runtime/semantic/tests/hierarchy_stateful_validation_test.cpp`
`runtime/semantic/CMakeLists.txt`
`runtime/semantic/tests/CMakeLists.txt`

## Self-review

Changes are limited to the authorized B3 header/source/test and minimal CMake registration. No B0/B1/B2 headers or implementations, ObjectStore ABI, ObjectIndex, A-lane validator/normalizer, schema/proto/registries, authority documents, or later packages were modified.
