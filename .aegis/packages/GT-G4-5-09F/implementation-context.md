# GT-G4-5-09F implementation context

## Starting point

- Repository: `Mostorm-Labs/axiom`
- Trusted task anchor: `3b5c60b99f2c257b0d6bd87853c39abe86788fc6`
- Execution ref: `codex/g4-5-reference-brush-fidelity`
- Integration target after review: `codex/g4-5-programmable-brush`
- G4.5-00 through G4.5-09 are preserved accepted work; this is an additive 09F delta.

## Current incomplete boundary

Current G4.5 proves programmable-brush architecture but not production-relevant brush fidelity:

- V1 runtime emits one primitive per raw input sample; `spacing` is not an emitter control.
- `ResourceCatalog` records resource identity/kind only; no sampled alpha content is materialized.
- canonical `SkiaInkBackend` draws a fixed blue path and does not consume programmable dabs/resources.
- Windows ARC stores preview primitives but currently renders a fixed cyan path; its brush descriptor resource is not sampled.
- Space currently toggles a legacy runtime-preview mirror, not the required ARC/runtime dual-color diagnostic.

## Target ownership flow

```text
Reference BrushDefinition v2
  -> BrushCompiler / ProgramIdentity
  -> materialized Shape + Grain
  -> common derived BrushRenderResource
  -> distance-based BrushRuntime DAB emitter
  -> ordered stable BrushPrimitive stream
       -> canonical Skia DAB adapter/renderer
       -> ARC descriptor + UploadResource + DAB primitive mapping
```

ARC is presentation-only. It must never calculate spacing, pressure curves, random channels, Shape/Grain composition, or brush-family semantics.

## Key implementation constraints

1. Preserve BrushDefinition v1 output and existing golden corpus.
2. Use V2 for 09F semantic additions.
3. Common runtime composes one render-ready alpha resource from Shape/Grain; this lets existing `arc_brush_descriptor_v0` single resource identity remain useful.
4. Add a generic Arc preview-resource upload message/API; do not put GPU/Win32 handles in semantic data.
5. Windows Arc DAB renderer samples the uploaded common alpha resource and applies only primitive geometry/opacity plus descriptor color.
6. Space is presentation-only dual-color mode. If the old mirror remains, move it to `M`.
7. Reference brush assets are original/procedural Axiom qualification resources, not copied third-party brush files.
8. P32 stops at automated + bundle readiness. Human Windows video/screenshots remain G4.5-10 successor evidence.

## First incomplete action

Run repository/package preflight, perform the required ImplementationDesignPreflight, then add the seven frozen 09F acceptance tests before changing production behavior. Observe the expected failures caused by the known current gaps and continue through the full closure contract.
