# Coverage work reuse and frontier dependencies

Repository: GitHub Mostorm-Labs/axiom, controlled branch
codex/geometry-navigation-integration. Trusted ancestor:
fcd553e9b898e1d9071f3566b29a6bbe06288cfd, containing implementation0a1bd103 and its
committed BLOCKED_PERFORMANCE result. User explicitly selected on this turn:
review and implement coverage work reuse before GPU lifetime separation.

Authority remains architecture v0.4 from docs/notion/manifest.yaml, existing shared
Render/Scene/resource/Skia/receipt contracts and current SDK consumer contracts.
No missing semantic decision, public API change or dependency release is owned.
Runtime owns camera/Scene/document/commit; SkiaNavigationTiles owns derived
coverage tasks, prepared sources and valid regions; provider owns GPU/present.
The predecessor package/result stays historical and blocked; do not overwrite it.

Review basis:full paired0a1 versus1a39 default zoom recovery median219.45->184.25ms;
reverse selected60s203->186.2ms. Warm exact wait248.4ms and ON CPU coverage57.1ms.
No new correctness failure;112 native tests retain the same5 existing failures.
Raw ON shows repeated coarse-mask starts, bounded contour CPU work and discarded
job steps, without GPU throttles. Distinguish completed-region eviction from
incomplete-job cancellation and re-rasterization before changing scheduling.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required_changes:trace the actual stable task identity, invalidation and
  handoff/reclaim path. Preserve/reuse completed coverage and eligible partial
  preparation across camera-only requests, using immutable Scene/source/surface/
  resource/grid stamps. Actual missing frontier owns critical priority; optional
  speculative/refinement work must not become a required dependency. Keep bounded
  task storage and real resource admission, with cancellation/eviction fallback.
- allowed:private shared Render task/cache/diagnostic seams and the existing
  Runtime navigation owner integration if needed for explicit suspend versus
  invalidation. No consumer SDK API/ABI/version change. Implementation design
  may reuse existing owners but must not add a second render/geometry owner.
- forbidden:Brush/semantic changes, raster-clip/grid/AA/order changes, false
  valid-region unions, incomplete-viewport present, premature GPU resource
  release, larger work/query/GPU/byte limits, background RAF loops, new workers,
  Skia/Semantic builds or locks, official Gate/P34 changes, user browser/document
  changes. Preserve4219/4220/4221/4222 packages and pages.
- required_tests:RED/GREEN behavior demonstrating cross-request reuse, latest
  frontier priority and no repeated already-completed contour work; immutable
  identity invalidation on source/scene/resource/provider/surface/metrics changes;
  bounded denial/eviction/cancellation and exact handoff not blocked; existing
  sharp-overlap/frontier/partial-region/unknown-corner and independent Direct
  byte parity. Native112 with exact5-failure comparison, seven actual Chrome GPU
  modes,28+ G5 Node and118 boundary checks, normal36-file ON/OFF packages.
- required_browser:paired same dense corpus against frozen1a39d1a0 ON/OFF packages
  out/g5-canonical-alpha-final-{on,off}/js, preserving the0a1bd103 packages
  out/g5-restore-final-{on,off}/js for incremental exploratory/recheck comparison.
  All selected/unselected pan/zoom,warm,idle/freeze cases and reverse selected60s.
  No builds/tests during timing. Exact source/artifact hashes, real GL submissions,
  pixels,document/selection/Operation/camera/receipt checks,180s/3GiB owned workers
  and cleanup. Optional exploratory traces bind dirty inputs and remain separate.
- performance_oracle:default restored zoom-to-pan median maximum submission gap
  <=150ms and<=60% of matched1a39 baseline (the predecessor target stays unchanged); inspect all phases and reverse order,
  no repeatable warm/clarity regression. Preserve default warm exact wait<=300ms
  and ON CPU coverage<=80ms. Do not pool warm/idle with frozen recovery to hide a
  miss. No universal physical latency or real-device acceptance claim.
- evidence:locally committed exact implementation and standalone proof compilation
  under verification/evidence/performance/g5-coverage-work-reuse/,including rejected
  hypotheses. New separate demo only after clean-source package/UI verification.
- terminal_success:required correctness,performance,package and durable-evidence
  closure; READY_FOR_CONTROL_REVIEW for this slice only, then preflight the already
  authorized GPU lifetime slice. Never official Gate/G5/P34/platform PASS.
- terminal_blockers:identity/source drift,unavailable locked tool/artifact/browser,
  authority/oracle/ownership conflict,new correctness regression or failure to
  improve the frozen reproduction after systematic investigation. Preserve exact
  work/evidence and review architecture with the user before further failed fixes.

Unresolved semantic/API decisions:none. Proposed implementation details are not
frozen authority. Start with task/eviction diagnostics, then bind the minimal
measured reuse design and its RED tests here before production behavior changes.

Review result before behavior changes:the task trace confirms41 completed world
cells reclaimed after each warm gesture, including its coarse fallback. After
all those cells are gone, used39,282,434 bytes is still above the33,554,432-byte
optional-record limit; the picture allocation still cannot succeed. Subsequent
camera requests recreate the discarded coverage. This is a futile admission
reclaim, not a missing second geometry/cache owner. Raw evidence:
out/g5-reuse-task/wheel.console.json.gz, bound by that run's source identity.

Minimal first reuse design:before optional-record reclamation, compute a safe
lower bound on non-reclaimable usage:all non-Recorded classes minus this owner's
live tile/job reservations. Recorded source/geometry leases are excluded entirely
from the lower bound, since some could be released. If even that bound exceeds
limit-requested, skip reclamation and use the existing recorded-picture denial/
checkpoint fallback. Otherwise keep the existing necessary reclaim path. Foreign
budget owners must never affect this owner's tasks. No resource limit increases,
GPU lease changes, new mask owner, or changed pixel representation. This preserves
already completed valid regions for the next camera demand without holding exact
convergence behind unfinished optional work.

Blocking RED tests:unattainable soft admission retains completed cells and the
next in-bounds camera request immediately presents them without new coarse masks;
feasible pressure still evicts enough and admits the request; foreign owner does
not evict. Continue existing identity/lifecycle/exact/frontier/Direct oracles. Add
lightweight skipped-reclaim/eviction observations; remove verbose task tracing
from final packages once the root trace/patch is preserved.

The first conservative bound passes Scratch-pressure tests but the browser still
rebuilds the same cells. It excludes all Recorded usage, including canonical
record/job metadata that navigation cannot reclaim. Add the same RED cases with
mandatory Recorded pressure, then bound reclaimable usage by this owner's actual
admitted tile/job bytes, prepared-geometry usage, shared-source pool usage, and
admitted source-list bytes. Count aliased source lists more than once if needed:
that overestimates reclaimable bytes and makes the skip test conservative; it
must never underestimate possible reclamation. Unknown ownership keeps the old
fallback. This refines the same admission-feasibility hypothesis, not the limit.

Precise feasibility alone is not sufficient:the accounted prototype still builds
58 cells/53 queries per warm gesture. Initial optional recording can fit by
reclaiming useful coarse coverage before later mandatory buffers grow; navigation
LRU also evicts it before unshown fine overscan. The minimal reuse policy must
therefore preserve its value, not just avoid provably impossible admission.

Before implementation:add a RED feasible-pressure continuation that pans8 world
units beyond the old fine cell and must present existing coarse coverage in one
step, without another coarse build. Protect only source-current completed coarse
regions near and extending beyond the validated canonical retained-view bounds,
with a cell span at least that view's extent; this is the useful fallback for
future exposure. With no retained source,use current visible bounds. Optional
recording may reclaim other cells but must fall back rather than destroy this
critical coverage. Navigation admission may still evict it when necessary for
actual visible keys; unshown overscan loses its absolute residency protection
and ranks below that fallback. Keep all existing bytes/entry limits and workload
limits; near/overscan job continuation remains separate from residency priority.
Provider/scene/resource/metrics changes invalidate the focus through the existing
owner reset. This is reuse of already-qualified images, not a new raster cache.

The useful-fallback prototype restores true reuse:hot gaps~18ms,restored~41ms,
zero coarse masks in warm phases instead of45. Finalize the same policy with a
RED3-entry cache test proving optional prewarm cannot consume the only preserved
coarse frontier. Its old path fails; restrict optional eviction to lower-value
victims while actual visible admission can still evict fallback. This closes the
specified priority invariant without new reservations or background work.
