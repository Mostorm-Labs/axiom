# GT-G4-5-C2 V11/V12 Verification Annex v0.3

Status: **P20 CONTROL-ACCEPTED FOR P31 REGENERATION; NOT EXECUTED EVIDENCE**.

## C2-V11 BrushResolutionDeterminism

Resolve the complete Vector profile twice on every supported host and compare
the typed canonical projection of `ResolvedVectorStrokeParameters`. Assert
`resolve(A)==resolve(A)`, `resolve(-0)==resolve(+0)`, and changing each field
changes the projection. Missing/non-finite/out-of-range values, unknown IDs,
and descriptor-size mismatch reject before session begin. Remove the catalog
after resolution to prove the result is self-contained. The pre-change oracle
is expected RED because no typed VectorReference resolver exists.

Blocking artifact: `c2-brush-resolution-determinism.json`.

## C2-V12 CanonicalReconstruction

1. Resolve the profile and process confirmed samples through the common session.
2. Seal a `BrushCommitIntent` and build existing AddStroke (envelope v1).
3. Serialize, create a fresh document with no catalog/session/Arc/GPU state,
   validate and replay the operation.
4. Compare exact StrokeRecord and named semantic projection bytes, then compare
   scene semantic projection.
5. For raster observation, build a non-empty `FramePlan`, call the existing
   `SkiaHeadlessBackend::submit(FramePlan)`, and read
   `SkiaHeadlessBackend::observation()`; a missing observation or empty plan is
   failure, not a fallback pass. Compare the declared RGBA digest/tolerance
   under the same renderer/resource configuration.

Negative fixtures: seed mutation, one option mutation, unknown profile, missing
pressure presence, malformed version, old V1 coexistence, prediction-only
sample, catalog removal, and descriptor-size mismatch.

Blocking artifact: `c2-canonical-reconstruction.json`.

The V11/V12 obligations retain all accepted C0/C1/R01 semantics. They do not
authorize renderer redesign, new ObjectKinds/Operations, or WetMix/Dab work.

