# GT-G3-02 implementation context

GT-G3-01 is closed and merged into canonical `main` at `47ffe5b69d7dda457d16203e41803086c880c174`. This task adds only Render-owned visibility resolution. The production resolver derives `SceneQuery.worldRect` from immutable `FrameState.worldViewport`, consumes G2 `Scene::query`, and preserves its deterministic `backToFront` final order and diagnostics.

Use a real `Scene` with the production spatial index for the 100K locality oracle. Fixture construction may create all records, but the measured normal resolve path must call only the G2 query boundary and must not access SemanticDocument/ObjectStore. Camera-only tests reuse unchanged canonical Scene/RuntimeScene identities.

First action: add the G3-02 contract test and test target, run the frozen `G3Visibility` command, and observe RED because the production resolver boundary is absent. Do not implement DirectReferenceSource, frame planning, backend, Tile, Demo, or any G3-03+ behavior.
