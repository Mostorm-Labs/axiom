# G5 Object Interaction Continuity r1

This package is independent of the frozen G5-UR2c moving-erase package. It addresses the supplied manual Web regression in the mixed scene: Shape/Sticky/Table/Image objects are difficult to select immediately after placement, and dragging one of them is not responsive.

## Confirmed render finding

In deferred Web presentation, pointer Move updates the transform and marks the transient display dirty; the display opportunity then calls `presentTransientOverlayFrame`. That path rebuilds visibility/reference/frame-plan and `SkiaRenderCache::drawTransientScene` resets preparation and redraws the complete reference list. Existing retained runs cover only a subset of compact ink. Non-ink Shape/Sticky/Table/Image content and the long-line background are therefore repeated work on every actual display frame.

## Confirmed interaction risk

Creation commits formal selection through `selectCreatedObject`. Canonical construction calls `renderSelectionOverlay`, but the text-state branch later creates a fresh `RenderViewRuntime` for Sticky/Table text presentation and may replace the selection chrome in the first presented frame. Shape/Image must be verified through the same first-present chain before any code change; do not assume a missing refresh call.

Implementation remains inside existing Runtime interaction and Render owners. OperationEngine, semantic state, ordering, opacity, resource leases and surface generations remain authoritative.
