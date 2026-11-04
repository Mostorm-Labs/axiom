# G5 navigation quality and continuity, combined second slice

This evidence joins the original cold-frontier/level/residency/idle/selection
boundaries and the reported blurry trackpad navigation. It consumes
`docs/planning/G5_NAVIGATION_QUALITY_AND_CONTINUITY.md`. No public API, semantic
contract, dependency lock, old Gate record, SDK release, or physical-platform
qualification is changed.

## Implementation and independent oracles

Before: clean anchor3906372c74ade39b85079c0394d17df4b3972b8d on
codex/geometry-navigation-integration. Matching ON/OFF installed SDKs were built
before implementation. Candidate compiled Runtime inputs and current measurement
harness inputs are bound by source-inputs.json. Both install maps and immutable
artifact hashes are in consumer-install.json; SDK version stays0.3.9. The dirty
embedded build anchor is not represented as a clean candidate commit.

Render now partitions a complete navigation plane into disjoint world regions,
choosing the highest valid texel density in each region. Retained canonical
pixels can cover their sharp overlap while coarse tiles cover only the missing
frontier. Unknown/nonempty exposure needs real Scene/tile coverage; only Runtime
empty-background proof can extend a retained source. Alpha is composed once per
region. Fine ready children beat coarse parents. No incomplete plane is presented.

Missing coverage has priority, followed by the largest visible resolution debt.
One applicable fine job can be suspended while the frontier fills, then resumed;
scene/resource/provider generation changes cancel obsolete jobs. Ordinary draw
entries also consume navigation steps. A final refinement produced after an early
present keeps pending work until the sharper plane actually presents.

Exact preparation no longer clears all world residency for a conservative8MiB
headroom. Only an actual optional-record admission failure invokes reclamation:
offscreen, coarser and older derived tiles first, only as needed. The complete
canonical image remains available. Admission denial still uses the original exact
evaluator. Frame/GPU/query limits and32MiB soft/96MiB hard budgets are unchanged.

The sharp stripe overlap and populated cold-frontier tests fail on the anchor
renderer (RED logs), then pass on CPU and actual Chrome WebGL2. The RED capsule
uses the three anchor renderer/navigation files with the candidate test oracle;
anchor-oracle-inputs.json binds their exact hashes. It is not mislabeled as the
clean-anchor SDK build. Tests inspect every submitted overlap, complete frontier,
zoom level changes, eventual adequate density, pause/resume, Direct exact pixels,
canonical submission isolation, low-pressure retention, actual failed admission
reclamation, denied fallback and lease cleanup. Navigation resampling is not
claimed bit-identical to a fresh raster at every zoom; canonical Direct equality
is checked independently.

## Measurement semantics

The root-SDK fixture has selected/unselected mixed objects, long offscreen content,
level-crossing wheel navigation, repeated input, a60s foreground idle case, and
CDP freeze/active recovery. It uses the public root SDK and matching installed
JS/WASM/resources. Raw timings include tails. Actual framebuffer color submission
gaps, not RAF alone, measure display work. Post-input onFrame is a coarse SDK
completion, not a camera-qualified or physical receipt. CDP freeze is not OS sleep
or physical GPU eviction. Synthetic wheel events are not a physical trackpad SLO.

Version3 private diagnostics measure the fraction of the presented viewport whose
source needs >1.25 device pixels per texel, maximum upscale and retained-source
area. These are source-density gauges, not perceptual blur or photon latency.
Only presented rows qualify. Recovery timing integrates the observed presentation
sequence, and retains right-censored episodes. Old version2 quality is unavailable,
never invented zero. Disjoint regions without a better source may temporarily use
>2x coarse feedback while bounded refinement progresses.

The optical run is separate and explicitly intrusive: readPixels samples a fixed
physical256x256 ROI and records edge RMS/checksum/camera; screenshots and complete
final framebuffer SHA256 corroborate it. GPU readback samples cannot qualify the
latency runs. Before/after final-pixel comparisons use the same phase/pose and
selection, not an invalid comparison against setup after selection/camera changes.

## Regression boundaries

Focused native Render/Runtime suite5/5, focused Node16/16, Runtime boundary
checker117 contracts and four independent real-WebGL modes pass. Full native
1000/1066 pass; all66 failed/not-built names exactly match the prior exact-contour
baseline (regression-comparison.json). A keep-going native rebuild retained the
four pre-existing Semantic test compilation failures for unhandled enum values;
all available dependent consumers were rebuilt/relinked. No whole-repository
green assertion is made.

The full GPU mode retains its previous `denied geometry lost full-stroke distance
phase` failure. It does not qualify cases after that assertion. All new quality
and exact-reclamation cases run in the separate passing modes. Full Node143 tests:
132 pass,11 fail due to unavailable compiled platform-harness modules. The npm
workspace build remains blocked by missing `tsc`, as archived previously; these
unbuilt protocol checks are not claimed as passing. Full logs and changed-test
hashes remain available. Dependencies were not rebuilt or replaced.

## Paired results and remaining findings

All14 main runs (ten timing-ON, two default-OFF, two separate optical runs)
completed with stable source/artifact identities and no runtime/page errors.
Every matching setup and phase final framebuffer SHA256 matches before/after.
Document/operation/selection/camera checks, idle quiescence, post-input receipts
and nonblocking fence checks pass. Five routes include nine canonical objects
(selected/unselected), populated offscreen content, and the user's two shapes,
sticky, image, table and long line. That last route has seven canonical objects
because the table's text is a separate semantic record; it is not mislabeled as
six canonical objects. All per-phase tails remain in paired-results.json.

A bounded pan keeping content inside the sampling region independently reproduces
the reported blur. In actual root-SDK WebGL, fixed-ROI edge RMS at matching poses
is about5–9 before versus24–27 after once coarse sources arrive; those coarse
replacements recur on repeat/idle/freeze routes. The native/GPU stripe oracle
supplies exact sharp-overlap correctness, rather than treating contrast alone as
proof of geometric correctness. The optical check does not measure latency.
The nine-object zoom optical route is unchanged at all60 ROI samples: both
versions still use the same retained image while fine work is unavailable. Large
zoom can still upscale that image4.22x, with source-density debt around44% of the
observed zoom/pan phase. This slice prevents quality regression when a sharper
source exists; it does not promise instantaneous fresh fine pixels at any scale.

Separate default-OFF bounded-pan latency runs retain every input: candidate
maximum color-submission gaps18.3–19.0ms, before18.1–27.6ms. Matching final pixels
remain exact. The populated-frontier selected route improves its largest gap
from35.1 to18.0ms across the recorded phases. Main-route candidate tails include
one34.9ms gap; a separate ordinary default-OFF recheck retains a27.3ms first gap,
then18.3/18.5ms repeats. No data are removed to claim a universal frame deadline.

Exact completion is not declared solved. Nine-object ordinary settle stays about
0.70–0.80s; CDP freeze recovery remains about2.4–2.8s before/after. The user-combo
zoom route includes ordinary settle fluctuations of+50–85ms in two zero-idle
`after-idle` phases. Timing-ON comparison has the same5120 coverage checkpoints
before/after, with CPU coverage108.9 versus126.4ms in that phase and15 versus17
exact opportunities. These tails are retained as a performance finding, not
hidden by a larger budget or an early receipt. Bounded warm-pan settle is about
0.12–0.13s; one extra owner opportunity can be necessary to present a completed
refinement before declaring convergence. Further exact-work/source-budget and
large-zoom refinement optimization remains separate from this quality fix.

## Reproduction and provenance

```sh
cmake --build out/host --target axiom_render_cache_tests axiom_prepared_geometry_tests -j3
out/host/runtime/render/axiom_render_cache_tests --quality-only
ctest --test-dir out/host -R '^(G5RenderCache|G5PreparedGeometry|G3Visibility|G3DirectReference|G5Navigation)$' --output-on-failure
node verification/runners/g5/prepared_gpu_probe.mjs out/web-prepared-probe out/quality.json --quality-only
node verification/runners/g5/navigation_quality_matrix.mjs BEFORE_SDK AFTER_SDK OUTPUT on
```

For bounded pan, use navigation_lifecycle.mjs with AXIOM_NAV_MODES=wheel,
AXIOM_NAV_PAN_ONLY=1, AXIOM_NAV_PAN_STEP=1, AXIOM_NAV_SELECTED=0, the user-combo
fixture options from bounded-latency-before/wheel.json.gz, idle0/cycles3 and
QUALITY_EVERY=0. Optical evidence separately sets QUALITY_EVERY=6, cycles2.
source-inputs-main-matrix.json binds the initial harness; source-inputs.json binds
the final harness. Only the private pan-step parameterization changed between
these maps; Runtime/SDK inputs and installed artifact hashes stayed identical.
Changed tests also have explicit hashes in test-inputs.json.

Per-run summary.json files are stored as summary.json.gz. Their original
sha256.json applies to decompressed bytes; the enclosing SHA256.json binds stored
bytes. All raw rows/consoles, screenshots, supplemental rechecks and the invalid
over-zoom smoke are archived. The smoke had zoom delta12 (5653x peak upscale),
concurrent build/test load and an invalid setup-vs-changed-selection pixel check;
it is explicitly excluded from latency and correctness qualification.

New default-OFF Demo: http://127.0.0.1:4216/example.html. Existing user Demo pages
were not reloaded or replaced. This evidence is a local implementation/review
result with the findings above. It is not an official G5/P34 or physical-platform
PASS, nor an SDK publication.
