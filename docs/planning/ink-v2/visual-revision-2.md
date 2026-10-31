# Compact brush visual revision 2

User scope, 2026-11-01: improve stroke smoothness, distinguish brush materials,
and show only relevant brush controls. Performance work remains separate.
Base: `6c46f540` on `codex/sdk-shell-axis-bridge`.

## Verified reference

Excalidraw revision `8bcdca4526867433f99fdfcb55a063668db0c9dc` uses
`perfect-freehand` for variable-width freedraw: thinning 0.6, smoothing 0.5,
configurable streamline (default 0.5), with the outline subsequently emitted as
midpoint quadratic SVG path commands. Constant width uses its laser-pointer
geometry. It is a combination of trajectory/width processing and curved outline
presentation, not merely antialiasing or increasing event count.

Source: [shape.ts](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/element/src/shape.ts#L1266),
[quadratic conversion](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/element/src/shape.ts#L1402),
[actual canvas fill](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/element/src/renderElement.ts#L524).
These are reference observations, not an adoption of a new dependency or its
empirical width multipliers.

## Ownership and compatibility

Input -> CanvasEditingRuntime -> BrushSession/CompactBrushSession owns signal
normalization and new centerline fitting. BrushCommitAdapter -> OperationEngine
persists one fitted centerline/effective-pressure sequence. Skia owns continuous
coverage and versioned material composition, shared by preview/canonical/export.
The Web UI calls existing public SDK commands and holds no stroke geometry.
No new public method, field, wire tag, dependency lock or document owner is added.

The six persistent compact presets gain package revision 2. Revision 1 remains
loadable with its original algorithm/material/resource identity. Revision 2 uses
algorithm version 2 and material version 2; the existing config/protobuf fields
persist these bindings. Unknown versions/resources still reject. Configuration
controls preserve the selected preset's private evaluator/resource binding.

Algorithm 2 rounds centerline controls with midpoint quadratic segments inside
their convex hull, adaptively flattened at a 0.15-unit quadratic midpoint
deviation threshold and bounded depth 8, followed by the existing compact
simplification. The first/last endpoints and pressure provenance are preserved.
Only newly completed segments become immutable chunks; the last straight reach
to the current pointer remains replaceable. Prediction evaluates a disposable
fork. Seal and confirmed-only preview use the same fitted/simplified knots.
Replay draws persisted effective knots without re-fitting or simulating pressure.
This is an explicit geometry revision: it may round an intentional sharp corner.

Material 2 keeps the analytic coverage evaluator and introduces an immutable
`ink-compact-material-v2` resource. Its SHA-256 concatenates the listed sources
in manifest order. Chalk uses fine dry pores throughout the body; crayon uses
dense directional wax rub; watercolor uses continuous translucent wash, broad
concentration variation and an inward pigment rim. Whole-stroke opacity is
applied once. This remains Watercolor Lite; it performs no canvas color pickup,
time diffusion, drying simulation or per-stroke bitmap persistence.

The Demo chooses the latest revision per profile, exposes brush color, and
filters parameters by family. Amber preview and selected-color canonical handoff
remain unchanged. Existing revision 1 objects are not restyled.

## Verification boundaries

Tests cover quadratic rounding/convex bounds/endpoints, input batching, predicted
tail exclusion, chunk/preview/seal equality, versioned codec round-trip, material
pores and pigment-rim metrics, matching preview/canonical pixels, public config
edits and browser snapshot/remote replay. Same-paint/size comparison swatches
are generated from real BrushSessions. Visual acceptance still requires user
review in the local Web Demo. No performance, physical platform, Gate or P34
qualification is claimed by these visual changes.
