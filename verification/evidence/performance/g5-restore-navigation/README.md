# Restore navigation continuity investigation

Implementation `0a1bd10338dc2ebda5d34691352b3d99278b0dd3`, controlled branch
`codex/geometry-navigation-integration`, ancestor
`2790f27ba95afd718f5c828dfd789a577b07ff9c`. Frozen comparison artifacts were
built from `1a39d1a0de65d7f22270c771160ae96b246d062c`; their complete package
hashes still match the previously committed canonical-coverage evidence.

This is a partial performance improvement, **BLOCKED_PERFORMANCE** for the
frozen continuity target, not an official Gate/G5/P34/platform acceptance.
`result.json` is mechanically compiled from the retained observations. The next
GPU lifetime and formal scale slices have not been implemented or accepted here.

## Accepted implementation and rejected trials

- Reuse qualified v4 A8/SrcOver white coverage for coarse CPU masks and prepared
  contour replay. Unsupported material/target cases retain N32. Geometry,
  primitive order, raster clip, sampling grid and material remain unchanged.
- Keep the ordinary64-texel speculative lead for amortization. Only when the
  existing conservative prepared culler identifies a stroke dependency added
  solely by that lead, bound the short-axis extension by observed motion. This
  is spatial scheduling only, not another Brush algorithm or rasterizer.
- When the fixed8 valid-region slots exhaust, render the tight bounding
  rectangle rather than the whole cell. Unknown inside corners are really
  repainted before validity publication. A metadata-only mutation fails the
  independent corner test.
- Opt-in mask/component/compaction diagnostics identify which work executes.
  Normal OFF consumer packages contain none of these timing strings.
- Motion-only lead reduction was rejected:it doubled warm navigation gaps.
  Full coverage scissoring was also rejected:Skia analytic-AA changes exact
  pixels even with a one-texel expansion. Neither failed trial is present in
  the accepted source. Do not weaken its existing raster-clip contract.

`exploratory/` binds all retained prototype observations to their dirty input
hashes; these are hypotheses, not final package qualification. `checks/` includes
actual RED/GREEN and an intentionally invalid-union mutation failure. The task
package also records the scissor failures and test-input-bound corrections.

## Paired installed-SDK measurements

Both normal packages remain SDK0.3.9,36 files, matching root wrapper/types/WASM/
resources, with no private headers/archives or added public consumer exports.
Each before/after worker uses its own Chrome process family, bounded180s/3GiB,
with cleanup proof. No build or regression test ran during timed matrix/recheck.
The existing4219/4220/4221 installs, user browser and documents were untouched.

The dense corpus uses636x923 CSS,DPR2,five distinct2048-sample scribbles,two shapes,
image,table,sticky and11 Runtime records. Both selected and unselected pan/zoom
cases include warm repeats, foreground idle and CDP frozen/active recovery;
selected zoom idles60s. `on/` and `off/` contain all16 final workers, full compressed
raw traces/console, summary, source/artifact identity, worker cleanup and hashes.
`recheck/` reverses candidate/baseline order for another selected60s run.

Default OFF paired matrix recovery gaps:

| Case | Before ms | After ms |
| --- | ---: | ---: |
| Pan |103.3|67.8|
| Selected pan |86.9|70.1|
| Zoom then pan |219.8|182.6|
| Selected zoom then pan |219.1|185.9|

Zoom recovery median219.45->184.25ms (~16% reduction) misses the frozen<=150ms
and<=60% baseline target. The old~389ms observation is not reused as the new
paired baseline. Default warm exact completion median248.4ms stays under300ms;
ON CPU coverage median57.1ms stays under80ms. Warm zoom submission gaps stay
about101ms. Full final framebuffer hashes and document/selection/Operation/
camera/receipt checks must match; see mechanical results rather than exit code
alone. Timing does not prove physical input-to-photon or GPU execution time.

Reverse-order selected60s recovery: after gap 186.2ms, completion 727.1ms; before gap 203.0ms, completion 677.2ms; the same recovery target remains unmet. The diagnostic warm-gap median changes
only101.25->101.30ms;the exact numeric non-regression predicate is conservatively
false, not evidence of a repeatable0.05ms product regression. No tolerance was
added to turn the blocked result into acceptance.

The current traces still put the uncovered interval in CPU contour coverage,
with no GPU backpressure. Stage2 resource residency may matter to cache retention,
but this evidence does not establish it as the remaining root cause. The next
architectural review should separate actual frontier completion from speculative
coverage and decide how completed work can be retained/reused across demands,
under unchanged hard budgets and exact AA/receipt rules. Do not keep tuning lead
constants, enlarge per-frame limits, or release staging before GPU completion.

## Correctness and access

Native112 tests:107 pass, the same5 baseline failures remain (full names/logs in
`checks/native-comparison.json`). Seven real Chrome GPU modes and28 G5 Node tests
pass;118 module boundary contracts pass. The prepared A8 oracle compares cached
and checkpoint100003-point geometry/material against independent N32 Direct,
including caps,tapers,marker,opacity,clip,mirror/skew. Frontier tests preserve sharp
overlap, verify partial-region/corner validity and final exact Direct bytes.

Candidate demo after clean-source installation and actual UI smoke:

- http://127.0.0.1:4222/example.html
- http://127.0.0.1:4222/mixed-navigation.html?dense=1

The demo contains the measured partial improvement; it is not a claim that the
remaining recovery target is fixed. `demo/` retains screenshots/UI checks.
`consumer-artifacts.json` hashes all package files; `source-inputs.json` binds
source files; `SHA256.json` covers this evidence directory. G3/semantic authority,
locked dependencies, old acceptance records and public SDK contracts unchanged.
