# Draw after pinch-in: diagnosis and bounded correction

User report: immediately drawing after pinch-in stalls. Reproduction uses the
4191-equivalent matching SDK, Chrome WebGL2 ANGLE Metal M2, 800×500 DPR2,
64 deterministic revision3 strokes, 30 synthetic ctrl-wheel inputs ending at
zoom≈2.46, then 24 pointer observations. Physical trackpad timing is not measured.

Two related regressions were reproduced with failing native tests:

1. A live BrushSession made serviceNavigationPresentation ineligible. That branch
cleared navigationExactPreparation_. Composition subsequently rebuilt canonical
synchronously before capturing/submitting independent preview. The pointer
handler itself took ~1ms, but the first render blocked ~1.60s; first preview came
~1.65s after down. Waiting150ms after zoom reproduced ~1.50s starvation.
2. A conservative visible Scene candidate whose actual compact support lay fully
outside the target clip caused SkiaRenderCache::prepare to return Fallback.
That abandoned cooperative work for all other sources in the frame. The native
terminal regression exposed this even after the scheduling flag was preserved.

CanvasEditingRuntime now retains cooperative exact policy from the last complete
camera/target identity. Live brush input pauses viewport-only reconstruction when
semantic/scene/target validity is unchanged; Composition still captures/renders
preview. Seal/cancel resumes existing exact preparation without manufacturing a
canonical receipt. An unchanged settled camera is not unnecessarily held. Lost
or foreign targets still reject; semantic/scene mutation and ARC confirmation
remain on their existing owners.

SkiaRenderCache records clip-empty compact admissions as no-pixel work, continues
preparing other sources, and uses that result only while begin() has verified
camera/clip/content/resource/target validity. No geometry, shader, AA, material,
package or SDK contract changed. Direct pixel parity and camera invalidation are
tested. GPU coverage remains the previously committed hybrid implementation.

First preview in the paired64-stroke corpus: 1647.2→49.3ms (immediate),
1495.5→10.6ms (150ms idle). Final pixel SHA256 is identical:
e179c5386cba24e46c893288ceb5e9de0a134839e0ea973ea19002957a796b8e.
The document gains exactly one canonical stroke. No canonical attempt runs during
live input in the corrected trace; after up, exact attempts yield cooperatively
instead of a single1.60s rebuild. Maximum canonical attempt in that run was24.6ms;
this is cooperative admission, not a guaranteed latency ceiling.

Remaining independent bottleneck: sustained preview itself still takes ~190–240ms
for this zoomed stroke as its mutable tail grows. The same effect reproduced on
an empty document (~216ms), while the no-zoom64-stroke control peaked ~38ms. Input
handlers remain near1ms (empty first-use initialization is higher). Chrome CPU
sampling places the hot work in WASM. Source inspection shows compact preview
uses SkSurfaces::Raster for its retained prefix and combined surface, and re-runs
the unchanged CPU coverage helper over the mutable tail each update. V3 retains
an end-taper safety horizon in world units, so magnification increases device
pixel work. A later loaded run peaked381–574ms in preview while first preview remained
18–37ms. These variable maxima are not a performance guarantee. This path is
separate from the hybrid canonical cache job. It is not
fixed by viewport tiles or by removing semantic/generation checks.

Next optimization should profile and bound/reuse exact preview tail coverage and
its device support, with preview/canonical material equality and predicted-tail
replacement oracles. Do not substitute an unqualified GPU gradient or change the
BrushEngine safety horizon in a rendering/scheduling fix. This result is
PASS_WITH_FINDINGS for scheduling correction; overall zoomed drawing performance
remains open and physical platform evidence BLOCKED_EVIDENCE.

Verification: native G5 suite27/27; fail-first navigation interruption and clip-
empty preparation tests; actual WebGL Direct byte parity; matching SDK browser,
context loss/resize, handoff, navigation and facade tests; timing-disabled compile;
boundary/workspace checks. The GPU test's cumulative allocation assertion is now
relative to its starting count because the new preceding pixel test legitimately
allocates targets; it still bounds the tested opportunity by four allocations.
Historical full-root failures remain in the preceding evidence and are not fixed
or reclassified here. No P34 PASS. New demo4192 preserves4191 and its drawings.
