# G5 mixed-scene preparation results

The user-authorized branch sync is complete: `codex/g5-settle-timing` and its
remote were fast-forwarded to main at
`5254b087468db9e91e04f35c705f77f63040e103`. This implementation is an uncommitted
local delta over that anchor, isolated from the other chat's current main checkout.

The three implementation slices are present:

1. A public-JS-SDK workload builds100/500/1000 strokes with a rectangle, real
   text and an imported PNG, then measures ctrl-wheel navigation, rectangle
   drag and immediately resumed ink. Raw timing, semantic state, final pixels,
   build hashes and checks are under
   `verification/evidence/performance/g5-mixed-scene/`.
2. Mixed scenes admit compact sources individually. Shapes, text, images,
   transformed/masked commands retain the shared Direct mapping. Capacity-denied
   compact sources stream in traversal order into one bounded full-frame
   offscreen target instead of accumulating raster work at final present.
   Unsupported per-object commands do not disable preparation of eligible ink.
3. `CanvasEditingRuntime` retains an immutable exact demand and its original
   frame identity across deferred canonical retries. Visibility, reference
   copying/encoding/digest and FramePlan building are skipped on unchanged retry.
   New input, document/scene, selection revision, resources, camera, metrics,
   provider or surface changes invalidate it. Active editing adaptations remain
   outside retention; idle selection chrome is regenerated over the same demand.

Every deferred canonical frame now uses the cooperative preparation policy,
including drag terminal frames. Immediate native callers keep synchronous
behavior. No new consumer API, semantic mutation owner, brush algorithm, lock
file or third-party build was introduced. No partial target publication or
approximate CanonicalVisible was added. GPU offscreen submission is nonblocking;
provider present still qualifies only the complete frame.

Representative measurements (ms, single runs; no SLO or physical display claim):

| Strokes | Scenario | Maximum canonical attempt before → after | Exact receipt after input, before → after |
| --- | --- | --- | --- |
|100|zoom|57.7 →10.0|95.4 →125.5|
|500|zoom|240.2 →15.9|266.5 →574.3|
|1000|zoom|493.5 →30.6|507.5 →1949.3|
|500|continued ink|232.2 →13.3|308.0 →509.3|
|1000|continued ink|470.2 →27.8|623.5 →2128.0|
|1000|drag terminal|367.5 →64.0|516.7 →1410.1|

All9 matched scenario pairs have identical final RGBA SHA256. Every retained
retry reports zero source/encoded-byte/plan-entry rebuild counts. The1000 zoom,
drag and ink runs retained116/82/124 attempts respectively. Concurrent builds
from other chats drove machine load above200 during some samples: these values
are diagnostic observations, not isolated benchmark estimates.

The change reduces long canonical callbacks by spreading work; total settle time
has increased. The1000-stroke live transient drag is still slow: its maximum RAF
callback was986.9ms versus371.0ms before, while post-input callbacks were bounded
to64.7ms in that run. Its synchronous warm-target capacity fallback is a remaining
performance defect. Do not describe this result as smooth large-scene dragging or
as a settle-time speedup. The next independent slice should address transient
composition/residency, then reduce total coverage/material work, with the same
Direct pixel oracle.

The first500-stroke ink samples differed across before/after; the last semantic
Operation differed only in generated identity fields when decoded. Fresh paired
runs produced identical pixels. Initial mismatched runs remain in ignored local
`out/g5-mixed`; the paired observations are the accepted comparator inputs.

Verification:

- G5 component CTest34/34; final changed-source focused rerun3/3.
- Actual Chrome WebGL/Ganesh probe passed including mixed capacity, transparent
  background, transform/mask, supersession, ordering and exact Direct pixels.
- Public SDK browser, frame handoff, Shell/Axis and demo checks passed on matching
  artifacts; navigation feedback checks passed. Retry timeout logs are retained.
- Seven-family DPR2 public SDK ink verification:105 checks; no runtime errors or
  exceptions, including preview/canonical handoff, undo/redo and persistence.
- Node facade/retry/baseline comparator8 tests; module boundary, C ABI manifest,
  workspace validation and timing-OFF compilation passed.
- Root CTest219/246,27 failures listed verbatim in `logs/root-failures.json`.
  Some are present in older diagnostics, but exact-main native attribution was
  not completed. They are findings, not all classified as pre-existing. G3
  snapshot/fixture authority and old P34 conclusions were not modified.

The SDK identity was reconfigured after verification to correctly report dirty
source state; its complete source/artifact hashes are in `logs/identity-final.json`.
The refreshed demo was smoke-tested and reran100-stroke mixed scenarios and the
navigation contract. Other platform device evidence remains separate and cannot
be inferred from these host/WebGL runs.

Classification: `PASS_WITH_FINDINGS` for this local implementation slice, ready
for review with the outstanding performance and root-regression findings above.
No official Gate, P34, or physical-platform PASS is issued. Demo:
`http://127.0.0.1:4195/example.html`; prior demo ports were preserved.
