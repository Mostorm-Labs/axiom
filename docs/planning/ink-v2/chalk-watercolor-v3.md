# Pressure-responsive chalk and watercolor revision 3

User instruction 2026-11-01: improve watercolor and chalk after accepted crayon.
Repository github.com/Mostorm-Labs/axiom; branch codex/chalk-watercolor-material-v3;
anchor 1229d0a12092107516b5c2cc1ea8a5750dfc76ae
(origin/codex/g5-settle-timing, ancestor).
The user requested this base after merging f2e391cd. Preserve G5 hybrid coverage,
cooperative raster jobs, pooled targets, navigation scheduling and frame timing.
Earlier native/Chrome observations at f2e391cd plus dirty material changes are
pre-migration diagnostics; validation must be rerun on this G5 descendant.
This local implementation binding extends INK-V2-02/03 and their current owner
contracts; it does not replace authority, change historical Gate evidence or ship
an SDK release. References: user Vibe row 3 (watercolor) and Procreate chalk image
0d8d5a35. No claim to either application's private implementation.

Owners remain Input -> CanvasEditingRuntime -> BrushSession -> CommitAdapter ->
OperationEngine; derived Scene/FramePlan/Skia. No new public API or semantic schema.
New chalk/watercolor package revision 3 binds config 2 / evaluator 6 / signal 3 /
material 4. Reuse bounded PF normalization and retained centerline/pressure chunks.
Old revisions and crayon 3 remain unchanged. New shader resource binds both sources.

Chalk: pressure-dependent contact/deposition, correlated multi-scale pores in world
space, continuous dense pigment and soft particulate edge. Watercolor Lite: smooth
wash, pressure-driven concentration, gently irregular boundary and low-frequency
paper/cloud modulation. Geometry and pressure coverage union precedes watercolor
material evaluation; wet pigment is computed only at the union's perimeter, never
at internal segment joins. Same-stroke coverage max-combines; different strokes
use SrcOver. This is a procedural wash, not fluid diffusion, pickup or drying.

Only centerline/effective pressure/config/seed/resource identity persist. Preview,
canonical, navigation and erase share Skia. Target-compatible GPU surfaces and
existing 32MiB aggregate cache bound apply to these new materials too.

EXECUTION_CLOSURE_CONTRACT
Required: RED/GREEN pressure-at-fixed-width and visual material tests; correlation
and contrast for chalk, translucent continuous textured wash without uniform dark
rim for water; range/direct/recorded/preview equality, opacity once, seed and event
density stability, cache/rebind/transform/mask; compact codec/replay and public SDK
selection/readback/pressure/lifecycle; matching native+Web build; actual Chrome
DPR1/2 observations and visual captures; full CTest compared with known 22 names;
runtime boundary, TypeScript and diff checks; served local page with new presets.
Forbidden: changing prior resource bytes/revisions, new semantic/wire/public API,
dependency/descriptor locks, expanded canonical dabs, JS geometry, Canvas2D,
physical fluid simulation, SDK publication or commit/push without instruction.
Blocking: new scoped regression, required build/browser environment unavailable.
Known baseline failures must be named in evidence; visual user acceptance, physical
touch and Windows qualification remain unperformed. Local-review-ready requires
all executable items; continue until that state or explicit blocker, never Gate PASS.

Implementation result: evaluator6 simplifies each confirmed chunk and replaceable
preview tail with the existing .25 world / .019 effective-pressure error limits.
The same simplified knots drive live rendering and commit, while predicted state
remains disposable. Water pressure coverage is contact-weighted; concentration is
recovered after union, eliminating per-segment constant-pressure endpoint patches.
The sparse/subdivided ramp test improved from4.29219 to.00078125 alpha error.

Final evidence: verification/evidence/ink-v2/chalk-watercolor-v3. Matching Chrome
DPR1/2 each105 checks and actual WebGL cached/resumed/Direct probe pass. Native
full suite219/246 with27 named baseline findings; four extra direct UI failures
reproduce on clean1229d0a1, and FC04.MSE-18 invokes two of those failures. Final
focused10 checks pass. No commit/push, physical touch or user visual acceptance.
