# G5 dense mixed-scene navigation repair

Repository: GitHub Mostorm-Labs/axiom. Controlled worktree/branch:
axiom-geometry-navigation / codex/geometry-navigation-integration.
Task anchor: 85f01aba (trusted ancestor). User authorizes investigation and repair
of the still-stuttering 4218 Demo, preserving its live document.

Authority: docs/notion/manifest.yaml architecture baseline v0.4; current G5
navigation coverage/quality and resource-budget contracts; SDK JS and Shell/Axis
preview contracts. Earlier sparse-source improvement remains valid, but is not
acceptance for a dense mixed viewport.

Owner path: SDK input -> shared Runtime camera/Scene -> SkiaRenderer ->
SkiaNavigationTiles -> registered provider. Render owns disposable images and
preparation; OperationEngine owns document mutation. No consumer contract change.

Diagnosis baseline: frozen ed8df6af SDK, 636x923 CSS, DPR2, zoom1.59,
two shapes + image + table + sticky + five overlapping v4 strokes (11 Runtime
records observed in this fixture). Unselected
wheel pan exhibits 252/269ms submission gaps, 409ms after freeze, despite small
per-call CPU cost. Public UI reads average1.2ms. Newly exposed strips wait for
whole coarse cells to finish; dense visible geometry defeats offscreen replay
pruning. This is a coverage-preparation granularity hypothesis, to be tested
against a bounded owner-work regression before changing implementation.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- scope: Render navigation coverage preparation, focused regressions, private
  browser reproduction/evidence. Preserve current4218 directory and live page.
- required: RED/GREEN dense-frontier regression under unchanged small owner/GPU
  quotas. Prepare only missing coverage first; any partial image must carry an
  explicit valid world region. Never advertise unfinished pixels as coverage.
  Reuse only scene/resource/provider/generation-valid pixels. Allocation and all
  in-flight copies remain budget-admitted; denial fails closed.
- required: partial/expanded coverage must match the corresponding full tile,
  with sampling gutters; eventual exact canonical pixels match Direct. Keep
  sharp overlap, object order, masks, transforms, latest camera, rejection,
  cancellation and coarse/canonical receipt distinction.
- forbidden: changes to brush algorithms, public API/ABI/version, semantic/schema
  or dependency locks, G3 fixture, old Gate records, configured work/time/GPU/cache
  limits; no direct JS drawing or unconditional overview/blur substitution.
- Web oracle: preserve baseline raw traces; run matching frozen before and new
  candidate ON/OFF SDKs, dense portrait mixed pan and zoom, selected/unselected,
  idle/resume. At least halve median cold/warm pan input submission-gap maximum
  in the dense reproduction; report tails, CPU, quality, UI reads, final pixels,
  document/selection/operation/camera checks and receipt wait. This is a task
  engineering check, not a product SLO or physical input-to-photon claim.
- required: relevant native Render/Runtime/provider/cache/prepared tests, real
  Chrome GPU partial/full-tile and canonical parity, existing quality/affine/
  background/replay oracles, Node metrics tests and runtime boundary checks.
  Compare new failures with known baseline; do not hide baseline failures.
- evidence: exact source/artifact/fixture hashes, RED/GREEN logs, raw browser
  traces, comparison and limitations in verification/evidence/performance/
  g5-dense-navigation/. No timings during builds. Own browser workers only,
  bounded180s/3GiB family RSS. Hosted/physical tests and publication optional.
- terminal_success: work/pixel/budget/state oracles hold and matching Web
  reproduction improves; deliver isolated candidate Demo and reviewable local
  evidence. No G5/P34/platform PASS implied.
- terminal_blockers: source divergence, locked input unavailable, authority
  conflict, new unresolved correctness failure or no improvement in reproduction.

Implementation preflight: prepare frontier cells with up to8 explicit valid regions
and preserve old valid cell pixels when extending them; publish atomically only
after all contributing sources complete. Fine-cell jobs remain separate. Reject
stale scene/resources as before. No unresolved semantic decisions; no authority,
verification or public contract conflict. Validate sampling and expansion before
accepting the design.

Reconciled diagnosis/preflight (same task, no semantic scope expansion):
- Query/init traces prove the coarse grid level changes on a pure pan when a
  viewport crosses grid boundaries; choose its maximum2x2 cover by viewport
  extent. RED oracle: identical-sized portrait viewports at different origins
  must request the same coarse cell span.
- Rendering must select full paths by device demand without narrowing their
  raster clip (Skia analytic-AA clip dependence). Publish only completed regions,
  keep texel phase/gutters, and repaint a rectangular strip after Src clear.
- A bounded quarter-cell lead at every exposed viewport edge amortizes repeated
  source setup, including long-axis growth of interior holes. Existing time,
  owner-step, query, GPU and byte limits do not change.
- Keep the existing conservative three-image tile reservation and GPU lifetime
  policy. Its high-DPR resolution/residency cost is a separate finding; reducing
  it requires explicit completion-owned staging leases, not a smaller counter.
  This repair charges its additional fixed region/Job metadata, rather than
  silently treating it as free. It does not claim to fix that accounting policy.
- Additional realistic paired cases use phase-offset distinct scribbles and
  verified SDK RAF quiescence before sampling. Preserve the initial exploratory
  fixture and rejected candidates as well. Normal OFF runs use the same public
  consumer fixture; no timings while builds/regressions run.
