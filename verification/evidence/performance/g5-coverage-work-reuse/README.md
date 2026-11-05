# Coverage work reuse

Implementation61ede04e819edf7f8a280465d9be842ea1ade860, controlled branch
codex/geometry-navigation-integration. User explicitly selected coverage work
reuse before GPU lifetime separation. The predecessor0a1bd103/fcd553e9 result
remains BLOCKED_PERFORMANCE and is not overwritten by this evidence.

Scope:shared Render cache/task admission and retention only. Runtime/document,
Brush geometry, canonical AA/order/grid/clip, SDK0.3.9 consumer APIs, source locks,
old Gates and dependency artifacts unchanged. No official Gate/G5/P34/platform
PASS. This slice is independently reviewable; GPU lifetime/quality and formal
complex-editing/100K/1M qualification remain dependent work.

## Root cause and implementation

The diagnostic task trace recorded41 completed cells reclaimed after each warm
navigation. Optional recorded-picture allocation attempts could erase useful
coarse coverage, even when later mandatory buffers still kept usage above that
optional soft limit. The next camera request rebuilt that coverage. Navigation
LRU also preferred recent unshown fine overscan over old useful coarse fallback.

- Retain source-current coarse valid regions near and extending beyond the
  validated canonical-view bounds. Existing highest-density composition still
  wins in sharp overlap. Validity never includes unfinished/unrendered pixels.
- Optional recorded pictures yield to that useful coverage, using the existing
  canonical checkpoint fallback. Feasible pressure can still reclaim other cells.
- Actual visible admission retains priority. Unshown fine overscan is an eviction
  candidate before reusable fallback; optional prewarm cannot evict that fallback.
- Skip provably unattainable optional admission using a conservative bound on
  owner-reclaimable tile/job,geometry,source-list and shared-payload reservations.
  Aliases are over-counted safely; no limit is raised and no GPU lease is freed
  before the existing lifetime owner releases it.
- Bind optional-reclaim protection to full frame/provider/resource identities.
  Foreign budgets never mutate the cache. Source-list header bytes are admitted
  with their existing owner; telemetry distinguishes cumulative reclaim skips.

No second mask/geometry/document owner or new background worker/scheduler exists.
The existing2ms navigation allowance, query/step/GPU/byte/entry limits remain.
Verbose diagnostic task tracing is preserved as an exploratory source patch and
raw console, but is absent from final packages.

Native RED/GREEN tests demonstrate impossible versus feasible Recorded/Scratch
pressure,foreign budget rejection,next-camera presentation from completed coarse
coverage without rebuilding,and3-entry optional-prewarm protection. Existing
partial frontier,inside-corner,sharp overlap and final independent Direct byte
oracles continue to pass. Initial Scratch-only feasibility and an overly loose
Recorded bound are retained as rejected/incomplete hypotheses, not final proof.

## Installed SDK/browser result

Final ON/OFF consumer packages are clean-source61ede builds,36 normal files,
matching JS/declarations/WASM/resources,with no private headers or archives.
Frozen1a39d1a0 packages supply the paired baseline;0a1bd103 packages remain an
intermediate reference. Each Chrome worker owns its browser,180s/3GiB limit and
cleanup; no builds/tests ran during timed matrix/recheck. All existing user
browser tabs,documents and4219/4220/4221/4222 installations were preserved.

Same dense corpus:636x923 CSS,DPR2,five distinct2048-sample strokes,two shapes,
image,table,sticky,11 Runtime records,selected/unselected pan/zoom,warm repeats,
foreground idle and CDP frozen/active. Selected zoom idles60s. All16 paired workers
are measured,with complete final framebuffer/document/selection/Operation/camera/
receipt equality,exact source/package SHA and raw observations. The final reverse
selected60s run swaps candidate/baseline order;see result.json/recheck for tails.

Default OFF frozen-recovery maximum color-submission gaps:

| Case | Before ms | After ms |
| --- | ---: | ---: |
| Pan |87.1|17.6|
| Selected pan |53.5|17.7|
| Zoom then pan |352.9|17.5|
| Selected zoom then pan |249.7|18.4|

Zoom recovery median301.3->17.95ms. This meets the unchanged<=150ms/<=60% paired
baseline engineering target. Default warm exact completion median240.05ms remains
under300ms;ON exact CPU coverage median56.4ms remains under80ms. Warm selected
navigation no longer creates coarse masks,demonstrating actual completed-work
reuse. First cold gestures still need coverage preparation(~60..100ms);the slice
does not claim every unseen region is instantly ready or physical input latency.

Quality/cold-exact findings remain explicit. Diagnostic time-weighted low-density
source fraction changes by about+0.8percentage point across the four warm zoom
observations. This is a source-density proxy over presented planes,not an optical
same-pose comparison;the versions present different sets of camera poses. Strict
sharp-overlap/pixel validity is preserved, but this proxy is not proof of optical
clarity closure. GPU residency/staging work must track this finding. Cold exact
completion also retains a long tail,including the reverse recheck; UI continuity
improvement is separate from canonical exact completion.

## Regression and access

Native112 tests:107 pass,the identical5 baseline failures remain (full names and
logs in checks/native-comparison.json). Seven real Chrome GPU modes,28 G5 Node
checks and118 boundary contracts pass. Device/native platform product acceptance
and formal scale Gates are not established by these Web results.

Fresh default UI and dense mixed-scene smoke passes:

- http://127.0.0.1:4223/example.html
- http://127.0.0.1:4223/mixed-navigation.html?dense=1

Root-consumer screenshots/checks are in demo/. result.json is mechanically
compiled;comparison.json retains every phase,including quality and exact-wait
tails. consumer-artifacts.json,source-inputs.json and SHA256.json bind the source,
packages and standalone evidence. No dependency rebuild,lock change,semantic
fixture repair or public SDK capability was used to produce these results.
