# GT-G4.6-B — View / Constraint Presentation

## Reuse
At the anchor, `RuntimeFacade` already exposes `panBy`, `zoomAt`, and `fitToContent`; `FrameState` already separates CameraGeneration from SemanticGeneration and `RenderViewRuntime` is per-view. Extend these seams; do not create a second camera authority.

## Frozen Fit policy
- padding: 32 logical px
- clamp: 0.05x..32x unless an already-accepted narrower runtime clamp exists; in that case preserve the accepted clamp and record the reconciliation
- empty target: rejected/no camera change
- degenerate target: epsilon extent + max-zoom clamp
- preserve camera rotation
- DPR does not change logical fit semantics

## Frozen Snap policy
- engage 6 logical px; release 10 logical px
- X/Y independently
- min/center/max features
- transformed/selected objects excluded as targets
- distance first, then ObjectId, then stable feature enum tie-break
- guide/highlight transient only
