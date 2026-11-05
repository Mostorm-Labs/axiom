# Nine-object exact convergence: contour replay and budget priority

Classification: **PASS_WITH_FINDINGS**, bounded local engineering result ready
for review. This is not a G5/P34 or physical-platform PASS. Code revision:
`334cd68cf84962b940f8246848f44522a6233eff`. Contract:
`docs/planning/G5_EXACT_CONTOUR_REPLAY.md`.

## Changes and ownership

The existing v4 contour compiler runs at the same32-knot checkpoints. Within
one exact job, the first device tile records the existing Skia helper's commands;
later tiles replay those immutable records. The device grid, AA, path ordering,
material, caps/taper and seed remain unchanged. Records are admitted before
allocation, bounded by the existing entry/shared budget, and released on
completion or cancellation. Denial returns to the original per-tile compiler.
No additional Brush geometry, canonical state or consumer API was introduced.

Record-only measurements initially showed little benefit. Opt-in admission
logging found38–45MB charged against the ordinary32MB budget, so records were
correctly refused even though critical raster targets could use the existing
96MB hard ceiling. During cooperative exact preparation under this pressure,
Render now releases rebuildable world tiles/source snapshots. It retains the
complete-image feedback source, prepared geometry and GPU backpressure. Neither
budget nor8ms/4096 preparation allowance was raised. Regression tests require
released tile residency, exact Direct pixels and immediate retained-image
navigation afterwards. Cold nonempty frontiers still require coverage work;
eviction does not establish a universal continuity guarantee.

Skia R-tree recording was separately measured and reverted because it did not
improve the corpus. Temporary admission logging was removed. Intermediate runs
remain separately labeled; their timings are not substituted for final results.

## Matching Web results

Before: clean anchor3783d457, SDK0.3.9. After: the exact source inputs in
`source-inputs.json`, tree SHA256
`b12c296af6d2edd8c5ced83b97a61abb57b0e0badc1460a02095e8baf228df0e`.
The SDKs were built from that dirty working tree before code commit334cd68c;
their embedded revision is3783d457 plus dirty=true. `materialization.json`
records the code/input binding. No clean334cd68c SDK build is claimed.
`consumer-install.json` binds all36 public files in each ON/OFF package.

Real Chrome WebGL2, identical root-SDK runner, nine mixed objects, three8192
sample lines, draw/navigation zoom.57, thinning.99, streamline.13, smoothing.61,
DPR2. Inputs are synthetic; GPU execution and input-to-photon remain unmeasured.
Performance sampling ran after builds and full regression finished.

| Phase, timing OFF | Before exact settle | After exact settle | Before input submission gap | After input submission gap |
| --- | ---: | ---: | ---: | ---: |
| Repeat0 | 880.8ms | 730.8ms | 17.7ms | 17.5ms |
| Repeat1 | 847.2ms | 797.1ms | 33.9ms | 25.8ms |
| After idle | 913.6ms | 747.8ms | 33.2ms | 17.5ms |
| After CDP thaw | 3544.7ms | 2912.9ms | 33.7ms | 78.8ms |

Diagnostic ON repeated coverage CPU:353–364ms before,300–305ms after;
total canonical-attempt CPU390–400ms before,330–340ms after. Timing-OFF
repeat settlement median improves about12%; diagnostic ON about15%. These
are finite paired observations, not a product SLO or a universal speedup.

All corresponding phase final pixel hashes match across before/after and
ON/OFF. Document, selection, operations, camera return and receipt checks pass;
canonical receipts during input remain0 and maximum fence timeout remains0.
The after-OFF foreground idle interval is60 seconds, versus1 second for the
baseline/ON runs; this is recovery corroboration, not an equally timed idle pair.

Final actual in-app browser check: nine objects, exact settle **764.5ms**, input
submission gap **31ms**, maximum RAF interval20.1ms; final pixel SHA matches the
corresponding baseline phase. `iab-final.json`/`iab-final.png` retain the result.
The existing user's Demo/document was not reloaded or replaced. New ordinary
Demo: http://127.0.0.1:4215/example.html ; prepared reproduction:
http://127.0.0.1:4215/mixed-navigation.html?lines=3 .

## Remaining recovery finding

The final ON and OFF thaw runs contain an approximately79ms input long task,
with submission gaps80.6/78.8ms. In ON, input dispatch reaches67.7ms while
instrumented C++ navigation is at most4.5ms. This does not identify the cause;
do not label it an environment issue or a closed regression. Paired profiled
repeats did not reproduce the long task (before/after thaw gaps33.0/17.7ms), so
the profile cannot attribute the original pause. Both original tails are kept.

Thaw exact settlement is **not consistently improved**: ON3128.9→3328.8ms;
profiled repeat3395.1→3644.8ms; OFF3544.7→2912.9ms. Profiling changes measurement
conditions. This slice improves ordinary settlement; recovery/first-input
latency remains open. Hours-long idle, OS sleep, physical trackpad/display
latency, arbitrary cold frontiers and other platforms are not qualified here.

## Verification

- RED work oracle: `exact tiles recompiled the same contour input` on the old
  job. Native and actual GPU oracles now require one input compilation across
  multiple tiles, immutable record reuse and strict Direct pixels.
- Native round/marker, cap/taper, affine, large origin,1/2/32/33-point boundaries,
  per-entry/global admission denial, cancellation/release: PASS. Cache pressure
  eviction retains immediate complete-image feedback. Focused five-case native
  suite:5/5 PASS (`focused-native.log.gz` and final native logs).
- Real WebGL2 replay, affine/pressure and background modes: PASS. Full GPU mode
  retains the archived `denied geometry lost full-stroke distance phase` failure;
  it does not qualify cases after that assertion. See `gpu-status.json` and
  `gpu-full.json`, compared with the prior mixed-scene evidence.
- Final full native:1000/1066 PASS;66 failed/not-built names, no names absent from
  the archived previous failure set. FC01SnapCreationDifferential passes in this
  run; no fix for it is claimed. Five old semantic/qualification build failures
  remain. Every failed name is in `regression-comparison.json`; full logs retained.
- Node14/14 PASS; Runtime boundaries117 contract files PASS. Full npm workspace
  test remains blocked by missing `tsc` (nine workspaces, exit127), retained in
  `npm.log.gz`. No whole-repository green claim is made.

Dependency locks and accepted prebuilt SDKs were reused unchanged; no third-party
SDK was rebuilt. No SDK publication, main merge, descriptor/snapshot alteration,
or old Gate-record change is part of this slice.

## Reproduction and artifacts

```sh
cmake --build out/host --target axiom_prepared_geometry_tests axiom_render_cache_tests -j3
ctest --test-dir out/host -R '^(G5RenderCache|G5PreparedGeometry|G3Visibility|G3DirectReference|G5Navigation)$' --output-on-failure
node verification/runners/g5/prepared_gpu_probe.mjs out/web-prepared-probe out/replay.json --exact-replay-only

AXIOM_NAV_SELECTED=0 AXIOM_NAV_MODES=wheel AXIOM_NAV_PAN_ONLY=0 \
AXIOM_NAV_OPTIONS='{"shapes":2,"lines":3,"points":8192,"scribble":true,"zoom":0.57,"drawZoom":0.57,"brush":{"thinning":0.99,"streamline":0.13,"smoothing":0.61}}' \
node verification/runners/g5/navigation_lifecycle.mjs out/g5-exact-final-off/js out/repeat-exact 60000 2
```

`before-*` and `after-*` are accepted paired inputs. Other case directories are
explicit experiments/diagnostics. `profile-driver.mjs` is the local diagnostic
copy which adds CDP profiling after thaw; it has host-specific paths and is not
consumer payload. Raw rows, consoles, source/artifact maps and profiles are
retained compressed. Case SHA maps refer to original uncompressed summary names;
`SHA256.json` binds all actual archived files except itself.
