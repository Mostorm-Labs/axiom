# GT-G4-5-09V implementation context

## Trusted starting point

Repository: `Mostorm-Labs/axiom`

Task anchor: `f3b8787844b5dad80ba2318e96d1a66180bb4278` (relation: ancestor)

Execution ref: `codex/g4-5-vector-freehand-arc-parity`

The anchor already contains the accepted G4.5 programmable-brush foundation, Skia parity work, Windows native input, Arc preview lifecycle, and CanonicalVisible handoff. The superseded `codex/g4-5-reference-brush-fidelity` branch contains DAB/Texture work and is not an execution base or source of required implementation.

## Current missing behavior

1. `BrushCompiler` accepts only definition version 1.
2. Vector `BrushRuntime::evaluate()` emits one `BrushPrimitive` per input sample; this is not a perfect-freehand-class filled vector geometry pipeline.
3. Canonical `SkiaInkBackend::submit(strokes)` renders a fixed blue 3 px centerline.
4. Windows `appendPreviewSamples()` directly converts raw pointer samples to fixed-radius `ARC_PREVIEW_PRIMITIVE_VECTOR_POINT`; it bypasses common BrushRuntime vector geometry.
5. Space toggles the legacy runtime preview mirror instead of the required dual-color diagnostic.

## Frozen target

Use an explicit immutable RB-V01 Vector brush version (preferred Pen/version 2). Existing v1 remains byte/behavior compatible.

G4.5 live semantics: size, pressure/thinning, smoothing/streamline.

G4.5 taper: visible fixed start/end taper owned by the immutable brush-version interpreter. Do not add per-stroke taper fields.

Target flow:

`PointerSampleBatch -> Axiom Vector Brush Session -> common incremental render-ready Vector geometry -> { Arc preview renderer | canonical StrokeRecord/replay -> common vector geometry -> Axiom Skia }`

Arc must receive render-ready geometry and must not evaluate smoothing, pressure, width, taper, or family behavior.

The canonical document continues storing existing `StrokeRecord` / `BrushDescriptor` fields plus source samples; replay regenerates geometry using the explicit brush version.

## Implementation direction

Prefer a renderer-neutral vector mesh/outline representation that supports a stable prefix plus replaceable tail. A triangle-list/mesh representation is acceptable and is often easier for incremental append than a closed contour whose right edge must be reordered. Any Arc protocol extension must be narrow, versioned, renderer-oriented, and carry geometry rather than brush semantics.

Do not import the superseded 09F Shape/Grain/DAB resource path.

## First incomplete action

Run repository/package/hash/anchor preflights. Record ImplementationDesignPreflight. Add the frozen 09V acceptance tests and observe the expected RED conditions before production mutation. Then continue through the full execution closure contract until READY_FOR_G4_5_10_PHYSICAL_VERIFICATION or an explicit terminal blocker.
