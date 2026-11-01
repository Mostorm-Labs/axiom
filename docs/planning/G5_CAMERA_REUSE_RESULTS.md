# Camera/navigation optimization checkpoint

The ordered work requested in this chat is implemented through a first bounded
Tile/scheduler/budget realization. This is an engineering checkpoint with
remaining obligations, not a completed formal G5/P34 closure.

Production behavior:
- Compact coverage SkPicture commands are retained in object coordinates and
  reused across camera changes. Final canonical raster still uses the pinned
  coverage/material shader and exact current-camera device grid.
- One bounded absolute-coordinate scratch target is reused instead of allocating
  it per stroke miss. Cropped retained rasters remain physically allocated crops.
  A fully rebased cropped scratch experiment was rejected after exact pixel
  comparison found six differing channels (maximum error 55).
- Navigation reprojects a last exact, plain-background snapshot. Runtime excludes
  active ARC/pending handoff, selection, creation and stale scene/surface states.
  Navigation has no canonical work/receipt. An owner-clock idle interval (100ms,
  provisional implementation policy) leaves canonical demand dirty and requests
  exact rendering. Repeated clean ticks do not resubmit the same camera.
- Web gates navigation on the same nonblocking fence/backpressure as canonical;
  a completed fence may prepare latest demand in the same RAF. The JS SDK public
  declarations/exports and input samples are unchanged.
- World Tile keys/local generations, negative coordinates, dirty-lazy visible
  requests, stale result rejection and eviction/recreation identity are present.
  The Skia-private raster candidate is opt-in, accepts integer device boundaries,
  independent unmasked compact strokes and opaque plain backgrounds; other cases
  use Direct. It preserves global coverage tile anchoring, and composites only
  current local-generation sources. Background/clip/surface/camera changes clear
  candidate pixels. Metadata visits and source copying remain measurable costs.
- The cooperative scheduling core supplies priority inheritance, dependencies,
  cycle rejection, exact dependency generation, dedupe/supersession, cancellation, deadline eligibility and
  bounded queue admission. Coverage and candidate Tile jobs execute through it
  on the existing owner thread. No new threads/pools are created.
- Renderer resources use shared RAII budget reservations: recorded commands,
  cropped rasters, scratch, navigation and Tile pixels. Soft denial stops optional
  recording; hard denial falls back Direct. This is not yet an Axiom-wide CPU/GPU
  accounting or all-subsystem scheduler rollout.

Measured browser diagnostics (Apple M2, Chrome ANGLE Metal, 800×500, DPR1,
64 strokes, 16 samples/stroke, native CDP input): wheel zoom gesture callback
median about 0.2ms; native DOM two-contact touch RAF callbacks about 0.1–0.2ms.
One exact settle callback remains about 295–459ms in the final run. These are single-run
observations. All raw callbacks, slow settle, artifact identities and no-error
results are retained. These are CPU callback observations, not physical latency,
GPU timings, an SLO or a controlled speedup ratio. The user-visible improvement
comes from eliminating repeated full raster during the gesture, not making
precise canonical raster constant-time. Newly exposed viewport regions use the
plain background until exact settle; pattern backgrounds and active overlays
fall back to exact rendering.

Verification checkpoint: native component suite 25/25; exact cached/Direct CPU
and actual WebGL/Ganesh pixels including both material revisions and all five
compact families. Navigation changes pixels without canonical identities; dirty
gate and exact settle tested. Tile background/clip stale reuse was caught RED
then fixed. Node workspace 134/134. Python 193 tests retains the four earlier
failures and two skips. Runtime boundary/workspace checks pass. Full CTest
initial run had 23 failures/236; one extra was an invalid ImageResource test
FrameState (zero metrics), repaired and rerun PASS. Remaining failure names match
the preceding 22; FC02Text explicitly rerun at 240s timed out. Full CTest before the final dependency-generation/LRU guards: 214/236, exactly
the same 22 non-passing names. The final guards received focused 3/3 plus
GPU/browser verification, not another complete full-suite rerun. Candidate
regressions and exact build hashes are in the evidence bundle.

Remaining ordered work:
1. Precise canonical raster still rebuilds device rasters after camera changes;
   generic transform-independent material/coverage and cropped-target parity
   require more work. CPU coverage remains the Direct authority.
2. Tile candidate remains disabled by default pending fractional-scale overscan,
   masks, text/image/effect and scale-ladder benchmark/ADR. It is not a general
   production Tile acceptance, and still uses a shared viewport scratch.
3. Scheduler/budget are the first actual render consumer, not a unified rollout
   for Scene/Geometry, decode, font/image resources, driver/cache allocations,
   GPU queue time and all CPU/RAM categories.
4. Physical platforms remain BLOCKED_EVIDENCE. No previous P34/schema/SDK lock
   or upstream fixture authority was modified.

Interactive matching Web SDK demo: http://127.0.0.1:4187/example.html .
Evidence: verification/evidence/performance/g5-camera-reuse/ .
