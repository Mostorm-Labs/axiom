# G5 restore navigation continuity

Repository: GitHub Mostorm-Labs/axiom. Controlled worktree
axiom-geometry-navigation, branch codex/geometry-navigation-integration.
Trusted ancestor:2790f27ba95afd718f5c828dfd789a577b07ff9c.
User authorizes the sequence:restore navigation continuity, GPU resident/staging
lifecycle separation, then complex editing/scale verification. This package owns
the first independently verifiable slice; later slices remain dependent work,
not inferred complete from this slice or silently mixed into its mutation scope.

Current authority is architecture v0.4 via docs/notion/manifest.yaml, existing
shared Render coverage/quality/resource/receipt contracts, and the current SDK
JS/Windows/Shell contracts. Canonical document/Brush/Operation ownership stays
unchanged. No unresolved semantic or public API decision is needed.

Owner:Runtime navigation demand -> SkiaRenderer -> SkiaNavigationTiles ->
prepared geometry + tile coverage -> registered provider/present. Input, Scene
and canonical mutation retain their existing owners. No public SDK API/ABI/version
change; source developer diagnostics are private to normal consumer packages.

Evidence basis:g5-canonical-coverage records current canonical gains and an open
restore guard. Default selected reverse-order recheck:388.6ms candidate versus
236.8ms older baseline gap; exact wait777.5ms versus3711.9ms. During the current ON
trace, calls35..54 have incomplete coverage/no present while actively drawing
CPU coarse masks; GPU throttles0, shared budget58..60MiB below96MiB hard cap.
The coarse path still uses N32/Lighten despite the equivalent v4 A8 encoding
already qualified in canonical. This is the first hypothesis to test, not proof
of a GPU lifetime cause.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required_changes: reuse the existing single v4 white-coverage geometry and
  qualified A8/SrcOver encoding in coarse CPU masks/prepared drawing; retain N32
  for unsupported material/target cases. Keep actual valid-region publication,
  sharp-overlap priority, generation validation, final canonical and receipts.
  Add diagnostic counts proving which mask path executes and coverage progress.
- allowed:private Render helpers/navigation/cache work, focused native/GPU
  behavior tests, private runners and independently bound evidence.
- forbidden:Brush/schema/descriptor/Skia/Semantic locks, document mutations,
  SDK API/ABI/version changes, JS geometry or renderer, incomplete coverage
  presentation, weaker pixels/tolerances, larger frame/query/step/GPU/byte limits,
  premature GPU resource release. Preserve4219/4220/4221 installations and pages.
- required_tests:RED/GREEN actual coarse scalar-mask execution; prepared cached
  and checkpoint geometry A8-to-N32 material byte parity against unchanged N32
  Direct; caps/tapers/marker/reversal/clip/transforms; partial frontier/sharp
  overlap/final canonical equality; N32 fallback, budget, lifecycle/cancellation.
  Relevant112 native tests with the five baseline failures explicitly compared;
  seven real Chrome GPU modes, G5 Node and Runtime boundary contracts; matching
  normal ON/OFF packages with no private exports/headers/archives.
- required_browser:paired same dense_navigation_matrix corpus versus frozen
  out/g5-canonical-alpha-final-{on,off}/js (implementation1a39d1a0, source anchor
 2790f27b has evidence only). Selected/unselected pan/zoom,1/60s idle and5s frozen/
  active; setup quiescence; reverse-order selected60s recheck. Exact artifact and
  source hashes, full pixels/document/selection/Operation/camera/receipt equality.
  Owned workers180s/3GiB; no builds/regressions during timing. Distinguish source
  availability/CPU draw/coverage gating from GPU time and physical input latency.
- performance_oracle:default OFF selected/unselected restore median maximum
  input-to-color-submission gap <=150ms and <=60% of matched current baseline;
  inspect each phase and reverse order, no repeatable warm navigation regression.
  Preserve canonical A8 gains:default warm exact wait median <=300ms and
  diagnostic warm coverage median <=80ms on the same four observations. These
  are task engineering checks, not product SLOs or physical input-to-photon.
- evidence:source/tests/evidence locally committed, standalone manifest and SHA
  in verification/evidence/performance/g5-restore-navigation/; rejected hypotheses
  preserved. New isolated Demo only after clean-source install/UI smoke.
- terminal_success:all required correctness/budget/performance/package/evidence
  oracles complete; READY_FOR_CONTROL_REVIEW for this slice with remaining
  findings. Then preflight the next authorized dependency slice, rather than
  declaring all G5 done. No official Gate/P34/platform PASS.
- terminal_blockers:repository divergence, missing locked artifact, authority or
  oracle conflict, new correctness defect, or no measured improvement after
  systematic investigation. Return exact preserved work/evidence and owning
  boundary, never relabel a failed threshold or fabricate device evidence.

Reconciled hypothesis after the A8 trial, before the next behavior change:
A8/Prepared parity passes and restore gap improves~389->203..219ms, but misses
the frozen150ms target. Opt-in per-component timing attributes most unpresented
opportunities to contour drawing (~1.5..2.1ms); coverage partition is0..0.1ms and
material0..0.5ms. GPU throttles remain0. The fixed64-texel speculative margin may
make a tiny exposed strip depend on densely populated offscreen geometry.

Add a RED native/GPU frontier case with five2048-knot coils strictly outside the
new visible strip but inside the fixed lead. First feedback must occur within
eight one-step opportunities, with original pixel/gutter/overlap/canonical
oracles. If confirmed, bound speculative lead by observed per-edge world motion
(four observed deltas, at least one texel, at most existing64), using previous
valid navigation demand or retained canonical bounds. Actual missing demand is
never reduced, unfinished pixels never enter coverage, and no work/byte/time
limit is increased. Cold demand without a previous viewport keeps the existing
cap. This changes only optional spatial prefetch, not coverage authority.

The new offscreen-density case is RED:normal frontier feedback occurs in7
one-step opportunities, but adding only offscreen coils makes first feedback
miss the8-opportunity bound. Keep the existing long-axis lead to preserve exact
rectangular region merging/diagonal-pan amortization; apply observed-motion lead
to the shorter strip axis. Thus the missing strip is complete, long-axis growth
remains bounded by the same cell, and irrelevant transverse prefetch no longer
blocks first coverage. Preserve the original inside-density and expanded-region
pixel oracles as well as the new outside-density case.

The four-delta transverse trial remains RED against the same offscreen fixture:
its required strip ends260, optional lead reaches276, and the brush support plus
coarse sampler/culling guard still intersects the offscreen coils near282.
Reduce only speculative transverse lead to two observed edge deltas; retain the
same1..64-texel cap, long-axis lead,8-opportunity/pixel oracles and browser targets.
This is a rejected spatial-prefetch trial, not an accepted fix or relaxed oracle.

Two-delta lead passes both first-feedback checks in7 opportunities. The new
fixture then exposed a test-fixture-bound error:its later40/12 pan brings the
previously offscreen dense coils into the actual viewport, so the old light
fixture's32 one-step extension limit cannot cover the newly required geometry.
Keep32 for the original fixture and retain the same4096 convergence ceiling as
its independent cold full-region oracle for the new heavy extension. Every
per-opportunity budget, the new first-feedback<=8 requirement, all four expanded
positions, exact pixel/overlap/canonical checks, and browser thresholds stay fixed.
This is an explicit test-input bound correction, not a production budget change.

Architecture/representation review after the motion trial:strict native/GPU
pixels pass but warm zoom gaps grow101->202ms. Console evidence records four
AXIOM_NAVIGATION_REGION_LIMIT events, absent in the fixed-lead runs. The8-region
representation exhausts and promotes a small union to an entire1024-world cell.
The problem is the dependency domain created by validity compaction, not a need
for another lead constant or a bigger work budget.

Before the next behavior change:add a diagonal multi-strip RED oracle with
high-density ink outside all requested views but inside the cell. Keep the8-region
cap. On exhaustion, repaint the tight bounding rectangle of old valid regions
plus new demand, including previously unknown inside corners; publish that one
rectangle only after every source actually completes. Never merely mark the
bounding rectangle valid. Grid/gutters/order/alpha/parity and all original tests
remain blocking. Native feedback≤32 one-step opportunities for each small view,
explicitly exercise compaction, retain visible ink and final Direct equality.
The original first-frontier≤8 remains unchanged. No new semantic or SDK decision.

The tight-compaction prototype passes the native/GPU validity and unknown-corner
mutation oracles, but real Chrome still shows~201ms warm/~254ms restored gaps.
Trace confirms no GPU throttle; CPU contour raster dominates. Root inspection
finds the contour culling uses the material-demand scissor, but the actual CPU
coverage canvas retains the full260x260 clip. A ribbon touching a narrow strip
therefore rasterizes its entire cell footprint before the material discards it.
Before any further lead/scheduler redesign, apply the same device scissor to the
coverage target before its world transform. This removes unconsumed coverage
pixels only; the existing material demand, sampling gutter, contour/order, valid
regions and alpha semantics stay unchanged. Existing real-browser RED thresholds
remain blocking, and all partial-strip/final Direct pixel oracles must pass.

The first coverage-scissor trial correctly fails the strict native frontier
oracle at dx4/dy0,x252/y43 (252 versus253). The material uses linear sampling,
so its exact scissor is not the complete coverage-read footprint. Preserve one
adjacent texel beyond that scissor for the linear kernel. This does not enlarge
material demand/validity or reservations; the same strict pixel test stays fixed.

Coverage scissoring is rejected, not accepted:one-texel expansion still fails
strict native pixels (251 versus253 at the same location). The existing
compactContourCullBounds contract explicitly warns that Skia analytic AA changes
with raster clip even away from the edge; this restriction was missed in the
first inspection. Restore the full coverage raster clip. Do not tune extra
texels or weaken the pixel oracle. Future work can change dependency scheduling
or reuse already-qualified masks, not assume clipped raster equivalence.

Scheduling refinement before the next change:the motion-only trial produces
many successive tiny jobs (coarse starts5..15 per opportunity), losing the fixed
lead's amortization even when every stroke already intersects the actual strip.
Keep the existing64-texel lead unless the shared prepared contour cull identifies
a new stroke dependency introduced solely by that lead. In that case only,
restrict the transverse speculative extension by observed motion. Inspect the
already-admitted source/geometry list with each entry's transform, using the
existing conservative numeric-domain/gutter culler; unknown geometry keeps the
old lead. No contour creation, source query, raster clip, byte or work-limit
change. Same native offscreen<=8, region-compaction and browser oracles apply.
