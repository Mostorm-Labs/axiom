# Camera reuse, navigation feedback and bounded rendering execution

Repository: GitHub Mostorm-Labs/axiom. User authorizes the ordered camera reuse
→ cheaper miss → navigation feedback → Tile/Scheduler/ResourceBudget work in this
chat. Trusted ancestor: `1d0b853779bb263929937f8ab87ba745ff990e9e`.
Authority: G5 v0.3 WP-G5-10..19, current source page
https://app.notion.com/p/3f64c57a590c816e8e8ae42f396a3ff2 (edited 2026-10-11).
This is bounded engineering execution, not original Gate prerequisite acceptance.

Owner/data flow: shared Runtime camera and semantic publication → immutable
FramePlan → Render-owned replaceable sources/coverage/scheduling → Skia-private
recorded/raster resources → registered provider → exact platform feedback.
No consumer API/ABI/export, semantic schema, Brush output or dependency-lock change.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- order: (1) camera-independent coverage commands; (2) bounded cropped miss
  targets preserving original device grid/material coordinates; (3) navigation
  reprojection and exact settle with explicit non-canonical coverage; (4) local
  Tile validity/dirty-lazy raster and shared derived-work scheduling/budget.
- required: preserve Direct oracle and disable/clear/fallback; bound recorded and
  pixel resources; resource/content/erase/clip/target generation validity; reject
  stale/invalid frames; camera-only updates never mutate document/Scene; preserve
  ARC and qualified CanonicalVisible; latest camera demand wins under backpressure.
- required tests: fail-first camera command reuse/work count; CPU and actual
  WebGL exact canonical pixels across materials/masks/affine/DPR/clip; scratch
  work bound; eviction/huge fallback; transient cannot issue CanonicalVisible;
  navigation stops at exact current-camera pixels; resize/lost/rebind/present
  failure; Tile negative/world address/local generation; dirty-lazy requests;
  scheduler dependencies/dedupe/cancel/backpressure; soft/hard budget admission.
- required verification: focused native component tests; existing render/material/
  lifecycle/handoff tests; boundary/workspace checks; full regression failure-name
  comparison; matching root SDK browser accumulation/navigation measurements.
- evidence: source/artifact hashes, raw work/timing/pixel observations and failures
  in verification/evidence/performance, with durable human result and limitations.
- forbidden: approximate/stale pixels as canonical coverage, another Brush/Scene
  owner, platform geometry/cache rules, new private thread pool, third-party
  rebuild/lock changes, benchmark choice silently becoming normative tile/scale
  defaults, physical/G5/P34 PASS without independent required evidence.
- terminal_success: ordered implementation and applicable verification completed,
  runnable Web demo, explicit remaining applicability and reviewer-readable evidence.
- terminal_blockers: authority/semantic conflict; oracle mismatch not safely
  recoverable by Direct fallback; locked SDK unavailable; unresolved regression;
  unavailable matching browser/artifact; source/evidence drift.

ImplementationDesignPreflight: acquire/validate before draw, cache candidates are
prepared before insertion and disposable on failure; submission and feedback
stay separate. Camera-independent commands contain existing coverage shader
operations only, not a second stroke algorithm. Current device-space raster
invalidation stays until a representation proves its own transform validity.
No unresolved semantic decisions in the first two slices. Later representation
choices stay candidate/private and require differential evidence before use.

Observed implementation checkpoint: a bounded camera-independent SkPicture source,
pooled absolute-coordinate scratch, navigation feedback/exact settle, candidate
world tile raster, cooperative scheduler and renderer-scoped shared leases are
implemented. Tile remains opt-in; integer device boundaries, independent unmasked
compact groups and opaque plain backgrounds only. Fractional/complex cases use
Direct. This checkpoint is not the full unified Runtime resource/scheduler closure;
see G5_CAMERA_REUSE_RESULTS.md for open obligations and exact evidence.
