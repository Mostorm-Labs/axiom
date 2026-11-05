# Canonical exact coverage locality

The canonical exact-rendering performance targets are met and that component is
READY_FOR_CONTROL_REVIEW. Overall performance acceptance remains
BLOCKED_PERFORMANCE because a restore navigation continuity guard worsens.
This evidence records a local implementation/verification slice on
`codex/geometry-navigation-integration`. It is not a G5/P34/platform Gate PASS.
The implementation commit is `1a39d1a0de65d7f22270c771160ae96b246d062c`;
the paired SDK baseline is the frozen `32418c78` ON/OFF build from the previous
navigation task. Both normal consumer packages remain SDK0.3.9.

## Change and ownership

`SkiaRenderCache` continues to own the pending exact preparation job. Each job
retains conservative bounds for the original32-knot contour batches, grouped
in16-batch nodes. Later256px device tiles omit disjoint batches; each step
checks at most32 metadata candidates/groups and executes at most one original
batch. The original clip, device grid, AA, order and material compositing remain
unchanged. The first tile still compiles incrementally.

Small compiler checkpoints and the index are active-job scratch, reserved
against the existing hard resource limit and per-entry cap. SkPictures and their
Skia bounding hierarchy remain optional/soft-admitted. Picture denial atomically
releases pictures but keeps checkpoint locality; hard/index-cap denial preserves
the full streaming fallback. Job cancellation/completion retires all leases.
This is an explicit temporary-metadata admission change, not a larger budget.
GPU image/staging lifetime is unchanged.

For v4 white coverage, exact jobs use a Skia A8 raster target with SrcOver and
transparent clear. The vector material reads the mask alpha rather than N32 red.
Unsupported formats or material combinations fall back to N32. The original Direct
oracle stays N32/Lighten. Independent byte-for-byte tests qualify this encoding
substitution, not a general equivalence for colored draws or arbitrary blend
modes. The pool keys include the mask encoding; a format fallback drops pictures
with incompatible coverage paint while retaining exact compiler checkpoints.
The four-byte-per-pixel reservation remains conservative; budgets were not raised.
Canonical provider targets and GPU image/staging lifetime are unchanged.

The new diagnostics distinguish compilation, selected record/checkpoint replay,
metadata/group search and coverage raster work. `cpu_coverage` continues to
include its compile/draw/record/replay children. Missing old counters are `null`,
never invented zero. Normal OFF artifacts contain no timing markers.

## Verification and limitations

- Native spatial oracle:183 coverage steps versus768 full fallback, identical
  full pixels, under both admitted pictures and denied optional cache. Hard
  denial preserves fallback. The grouping search is bounded per step.
- Expanded exact Direct parity covers round/marker, cap/taper, short strokes,
  reversal at batch boundaries, affine/mirror, clip/nonzero grid, large origin,
  perspective fallback, partial denial, reclamation and interruption/lifetime.
- Seven real Chrome WebGL2 oracle modes pass:frontier,quality,affine,background,
  contour,replay,exact-replay. This is raster correctness, not GPU timing.
-112 native tests:107 pass; the same five baseline failures remain. See checks/
  native-comparison.json. No semantic fixture or lock was changed.
-26 Node tests and118 Runtime boundary contracts pass. The root SDK normal Demo
  draw/shape/navigation smoke and dense mixed document checks pass.

The paired Web matrix is the unchanged dense root SDK fixture:636x923 CSS,DPR2,
Zoom1.59,five distinct2048-sample scribbles,two shapes,image,table,sticky,
11 Runtime records. It uses2cycles, selected/unselected pan/zoom, foreground
idle and freeze/restore, and waits for setup RAF quiescence. It is not the user's
saved document or a physical trackpad/input-to-photon measurement.

Both baseline and candidate are sampled without builds/native regressions running.
Each browser worker owns its process family and has180s/3GiB limits; logs include
cleanup and exact artifact/source hashes. The original4219 app/document is not
loaded or modified. The new4221 default OFF Demo uses a separate installation.

See `result.json` for the frozen task thresholds and actual outcome;
`comparison.json` includes every phase, final full-canvas equality, navigation
submission gap, receipt waiting, source density quality and coverage counters.
The public completion receipt is a coarse SDK proxy, not camera-qualified;
ON raw native frame stamps corroborate accepted exact submission.

## Findings

A grouped-picture-only prototype reduced8000 coverage calls to3948 but did not
reduce warm CPU cost:soft-budget fallback still compiled108150 knots. The
checkpoint tier reduces compilation to the selected local batches, but dense
intersecting CPU raster replay remained the principal warm cost. Detailed timing
showed ~1ms compilation/~2ms recording versus ~290ms selected replay. A subsequent
per-primitive hull-cull trial made no stable improvement and was removed.
R8 storage passed native/Chrome pixels and reduced warm coverage to~269ms, still
below the performance target. The A8/SrcOver candidate then reduced exploratory
warm coverage to~56ms and completion waiting to~248ms; final clean paired matrix
numbers are in result.json. The slower full checkpoint matrix is retained under
exploratory/index-matrix; the R8 experiment also preserves its source files.
Rejected prototype traces are preserved under exploratory/ with dirty source
fingerprints; they are not final artifact evidence.

The frozen50% warm CPU and30% default receipt-wait thresholds are retained.
The canonical component status uses both those targets and correctness checks.
The overall status remains blocked by the separate navigation continuity guard,
not promoted from traversal counts or a faster exact receipt. Exact CPU edge raster, debounce/
frame scheduling and navigation source quality can still have nonzero tails.
GPU resident/staging separation needs its own design/evidence. Formal
100K/1M acceptance and physical-platform qualification remain open.

## Reproduction

Use the existing locked Semantic and Skia SDK configurations. Node runners import
only the installed root consumer package; the private C++ GPU harness is never
installed. `collect_evidence.py` preserves raw data and generates the aggregate
comparison; it requires the recorded `out/g5-canonical-*` runs/installations.
`SHA256.json` binds all evidence files, excluding itself.

Demo:`http://127.0.0.1:4221/example.html`, or
`http://127.0.0.1:4221/mixed-navigation.html?dense=1` for the fixed mixed scene.

## Open restore navigation guard

The full ON/OFF matrix still contains zoom-to-pan periods with no color
submissions. Default selected restore gap:220->289ms; a reverse-order paired
recheck (candidate before baseline, same60s idle/freeze) reproduces389ms candidate
versus237ms baseline. Exact completion simultaneously improves~3712->778ms.
These are distinct metrics. RAF and input continue through the empty submission
interval; this is not evidence of GPU execution time or a staging-lifetime cause.
See restore-analysis.json and the complete reverse-order traces.

All final pixels/state/receipts still match. The canonical acceleration is a
reviewable component, but the complete navigation performance contract is not
closed. Next work must address shared Runtime/Render frontier coverage and
presentation scheduling while keeping A8 pixels/budgets. Do not create a JS
renderer, enlarge per-frame budgets or claim a Gate PASS from this result.
