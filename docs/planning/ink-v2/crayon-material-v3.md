# Pressure-responsive crayon revision 3

User instruction (2026-11-01): implement crayon first and rebuild the local Web
Demo, using the supplied Vibe crayon screenshot as the visual target. Procreate
chalk screenshot is a future chalk reference; watercolor remains out of scope.
Repository github.com/Mostorm-Labs/axiom; branch codex/sdk-shell-axis-bridge;
anchor 459383332c4abae4942116b19b29a5be5680f755 (ancestor).

Current owners remain PlatformPointerBatch -> CanvasEditingRuntime -> BrushSession
-> BrushCommitAdapter -> OperationEngine, with Scene/FramePlan/Skia derived display.
No new SDK entry point, canonical object, wire tag, dependency or platform renderer.
Existing setBrushConfig controls are reused; their validation/docs will admit the
new crayon preset's existing vector controls. Previous preset revisions remain
byte/render compatible. The user authorizes this additive implementation binding.

Crayon package revision 3 binds config 2 / evaluator 5 / signal 3 / material 3.
Signal 3 reuses bounded PF-style distance-pressure normalization and prediction
forks. Persist only normalized centerline and quantized effective pressure.
Evaluator 5 sweeps the carrier with pressure-dependent radius, soft particulate
edges, world-space fine paper tooth, arc-length-oriented rub, and pressure-driven
pigment flow. Material is evaluated per segment before a max union into scalar
coverage; whole-stroke opacity applies once after union. No per-event accumulation
or expanded dab persistence. This is a dry digital crayon approximation, without
canvas pickup, physical wax height or time simulation.

The shared Skia helper uses captured seed, cumulative arc length and existing
viewport transform in direct, cache, preview and navigation range paths. Render
caches remain view/tile bounded and disposable. New immutable shader/resource
identity includes the complete evaluator sources; old material assets unchanged.

EXECUTION_CLOSURE_CONTRACT
Required: new preset/resource binding; pressure changes both width and pigment at
fixed width; fine varied coverage and soft edge rather than opaque scraped ribbon;
seed determinism, collinear sample-density invariance, range/full equality,
preview/canonical/transform equality, cache/lifecycle behavior, compact codec and
SDK config/replay tests; native build and matching Web build; Chrome SDK checks
and crayon visual captures; local served page available; runtime boundary/types/
diff checks; full CTest compared with recorded 22-name baseline.
Forbidden: chalk/watercolor redesign, old material reinterpretation, descriptor or
dependency locks, expanded canonical dabs, Web geometry or another renderer,
performance subsystem redesign, SDK publication/commit/push without instruction.
Blocking: new focused failure, new regression failure name, missing required SDK
or browser startup. Existing baseline failures are separately reported. Real touch,
Windows ABI and user visual acceptance remain unperformed/corroborative.
Terminal: LOCAL_REVIEW_READY_WITH_BASELINE_FINDINGS or explicit environment/test
blocker. Continue until terminal state; no Gate/P34/release PASS.


Implementation clarification: the existing smoothing scalar is reserved at its
captured default for crayon; only streamline/width response/taper/caps are exposed
for this material. Do not advertise round/marker contour filtering for crayon.
Direction for rub is centerline tangent with cumulative arc length; world-space
paper modulation is independent of event density. This is a max-pigment dry pass,
not repeated-dab flow accumulation, and self-overlap takes the denser deposit.


Visual references and attribution: Vibe screenshot a5c4628c supplied by user
(crayon row 2); Procreate chalk 0d8d5a35 is future-only. Public engineering
references: Krita opacity/flow and texture documentation, plus libmypaint
mypaint-brush.c at d5a88fbe6649d5ec776bc42ec8c1f4bb29d7fd7f (opacity
linearization, lines 1042-1075). Equations here are a new dry-coverage approximation,
not imported brush data or a claim to Vibe's private algorithm.

- https://docs.krita.org/en/reference_manual/brushes/brush_settings/opacity_and_flow.html
- https://docs.krita.org/en/reference_manual/brushes/brush_settings/texture.html
- https://github.com/mypaint/libmypaint/blob/d5a88fbe6649d5ec776bc42ec8c1f4bb29d7fd7f/mypaint-brush.c#L1042


Coverage feather is fixed in world units (0.65 minimum) rather than captured from
current device scale, so recorded coverage and direct replay are identical after
camera changes. Fine-grain noise uses fixed world-space scales .65/.40; no
texture density is derived from DPR or the current canvas matrix.


Observed limitations for visual review: default simulated pressure controls width
and dry flow from event-spacing; repeated passes within one stroke use max pigment
rather than wet build-up. Very thin strokes near the minimum pressure remain light.
The demo exposes size/deposition/opacity/textureScale and width response. Visual
similarity to the supplied reference is user-reviewed, not inferred from unit tests.
