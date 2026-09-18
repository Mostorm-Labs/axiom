# GT-G3-04 implementation context

Canonical `main` includes G3-01 per-view FrameState, G3-02 Render-owned VisibilityResolver, and G3-03 DirectReferenceSource at `b995329df9cd299db4990362d1f3c6ac155ee4fb`. There is no existing FramePlan or IRenderBackend implementation in `runtime/render/`.

G3-04 adds only the smallest immutable orchestration boundary: a gate-local frame plan carrying exact FrameState plus ReferenceDrawList, and a private backend interface that consumes that plan and returns explicit success/rejection. It must not own platform surfaces, Skia, presentation, semantic state, RuntimeScene, or final FrameGraph ABI.

First action: add the frozen G3-04 contract test and CMake target, run the required RED compile/test against the current tree, then implement the smallest authorized boundary. Preserve all G2/G3-01/G3-02/G3-03 behavior and run the full predecessor matrix before returning.
