# PX0-BUILD-01 — Runtime/Render -> Scene Include Boundary

This is an independent build unblocker discovered while executing PX0-05. It does not extend PX0 product semantics.

Starting repository baseline:

- repository: `github:Mostorm-Labs/axiom`
- task anchor: `5aa2afa4442d0254f2412b7b625173f36b57354f`
- reported failing include: `canvas/scene/scene_types.hpp`
- public header present at: `runtime/scene/include/canvas/scene/scene_types.hpp`
- render public header using it: `runtime/render/include/canvas/render/reference_draw_list.hpp`

Current CMake intent:

```text
canvas_runtime_scene
  PUBLIC include: runtime/scene/include

canvas_runtime_render
  PUBLIC link: canvas_runtime_scene
  PUBLIC include: runtime/render/include
```

Under normal CMake usage-requirement propagation this should make the Scene public include visible while compiling/consuming Render. The reported failure therefore must be reproduced before any repository edit. It may be a stale/misconfigured build tree rather than a source defect.

## Decision rule

1. Clean configure the same locked native/clang-cl + Skia configuration family used by PX0-04/PX0-05.
2. Build `canvas_runtime_render` verbosely.
3. If `scene_types.hpp` resolves: stop with `BLOCKED_ENVIRONMENT`; do not edit CMake.
4. If it fails and the compile command lacks the owning Scene usage requirement: make the smallest target-scoped CMake correction in `runtime/render/CMakeLists.txt` and/or `runtime/scene/CMakeLists.txt`.
5. Rebuild Render, then the real Windows app target.
6. Never touch C++ source/header semantics or PX0-05 implementation files.

A successful result becomes an exact prerequisite for the next executable PX0-05 package revision. The existing local PX0-05 worktree is intentionally left untouched by this task.
