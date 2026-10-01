# GT-G4-5-C2 V11/V12 — Verification and Render Observation Annex v0.2

Status: **PROPOSED_PENDING_P20_REVIEW**. This freezes the intended oracles and
observation seams for the accepted amendment; it is not evidence that either
obligation currently passes.

## C2-V11 — BrushResolutionDeterminism

For the same complete `BrushDefinition`, definition version, explicit parameter
set and execution profile, `BrushCompiler` must produce the same normalized
`ResolvedBrushExecutionParameters` on every supported host. No brush name,
catalog lookup, renderer, locale, clock or host RNG participates.

The fixture is the explicit profile in the wire annex plus a matrix that
independently mutates `size`, `thinning`, `smoothing`, `streamline`, easing,
caps, pressure simulation, taper policy, definition identity/version and seed.
The oracle compares the specified typed field projection, not C++ object memory:

```text
resolve(A) == resolve(A)
resolve(-0) == resolve(+0)
resolve(A with field i changed) != A
missing/nonfinite/out-of-range -> reject
unknown profile/algorithm/easing/taper -> reject
descriptor size mismatch -> reject
```

The catalog is removed after resolution to prove the result is self-contained.
The seed is copied exactly; equal options with intentionally different seeds are
not equal canonical strokes.

The pre-change seam is the prospective common resolver/commit-intent boundary
before `BrushSession`. Current C2 v0.1 has no VectorReference resolver or typed
durable payload, so the fixture must fail with unsupported/missing resolution.
If old code reports a resolved profile or silently maps a brush name, classify
that as `VERIFICATION_DESIGN_DEFECT`, not green evidence.

Blocking evidence: `c2-brush-resolution-determinism.json`, containing fixture
identity, normalized projection, rejection matrix and implementation observation.

## C2-V12 — CanonicalReconstruction

The live canonical path and a fresh replay path must produce identical canonical
stroke projection and identical observed render output for the same profile and
confirmed samples.

Fixture sequence:

1. Resolve the profile and feed confirmed samples through `BrushSession`.
2. Seal and build a transient `BrushCommitIntent`.
3. Build and serialize existing `AddStroke` with the reviewed version pair.
4. Start a fresh document/replay instance with no Brush catalog, session, Arc
   preview, prediction tail or GPU state.
5. Decode, validate, apply, rebuild the scene and render through the seam below.

Acceptance is exact equality of typed `StrokeRecord`, operation projection,
document/scene semantic digest and ordered samples. Render comparison uses the
renderer-declared raster digest/tolerance; two empty/fallback frames are not a
valid comparison.

Negative fixtures cover catalog removal, seed mutation, one option mutation,
unknown profile/algorithm, missing pressure presence, malformed version,
old-v1 coexistence, prediction-only samples and descriptor-size mismatch.

### Exact render observation seam

```text
RuntimeScene projection
  -> runtime/render DirectReferenceSource
  -> VectorStrokeReferenceCommand
  -> runtime/render SkiaHeadlessBackend::render
  -> declared RGBA/raster digest
```

For a VectorReference object, the renderer adapter must consume the resolved
options and confirmed samples through the common semantic projection before
creating `VectorStrokeReferenceCommand`. C2 may not redesign the renderer or
silently substitute the old fixed-width line path. If the current renderer
cannot consume the representation, record `RENDERER_CONSUMER_MISSING` and route
to the owning architecture/package layer; it is not a passing fallback.

Against pre-change C2 v0.1, live/replay is expected to fail at unsupported
representation or renderer consumption. The blocking artifact
`c2-canonical-reconstruction.json` must include operation bytes/projection
identity, fresh replay observation, scene digest, raster digest and negatives,
bound to the exact R1 reference and renderer/resource profile.

## Boundary

V11/V12 retain C2-V1 through C2-V10. They do not authorize Skia redesign, new
Arc semantics, WetMedia/DAB work, a new Operation kind, or R1 kernel changes.
P20 owns final fixture/oracle acceptance; P21/P23 owns authority/supersession
review; only after both are accepted may P30/P31 regenerate the executable
package.
