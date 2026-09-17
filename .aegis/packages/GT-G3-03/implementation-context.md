# GT-G3-03 implementation context

GT-G3-02 is closed and merged into canonical `main` at `fe1d03903995d9bcfd46b01212617f8043b9f2ab`. The production Render Core already has immutable `FrameState` and Render-owned `VisibilityResolver`; G2 exposes immutable `RuntimeScene::generation()`, `RuntimeScene::find(ObjectId)`, and the complete nine-kind `RuntimeSceneRecord` projection.

G3-03 adds only the permanent backend-neutral Direct / Non-Tiled Reference Source. The normal source loop is driven by `VisibilityResult.backToFront`, performs one `RuntimeScene::find` lookup per visible ID, preserves complete renderer-neutral record values and frame/query identities, and emits an ordered structural plan plus versioned canonical bytes/digest. It does not query visibility, iterate total scene records, read semantic storage, or implement a backend/frame orchestrator.

`ReferenceDrawList` is the permanent direct/reference correctness oracle, not the final optimized FrameGraph ABI. `Group` must retain an ordered traversal entry with explicit zero-pixel contribution. Exact structure is the primary oracle; the digest is replay/cross-platform evidence, not a lossy replacement.

First action: add the frozen G3-03 contract test and CMake test target, build `axiom_render_direct_reference_tests`, and observe the required missing-header RED before creating production headers or implementation. Do not implement Skia, IRenderBackend, frame planning, presentation, platform hosts, Tile/cache/scheduler work, or any G3-04+ behavior.
