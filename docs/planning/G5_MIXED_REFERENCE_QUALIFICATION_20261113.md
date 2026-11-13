# Mixed reference qualification

This reference-only successor preserves the accepted Render material reuse work,
all 17 previous changed files, production sources, SDK assets and historical
results. It does not rebuild an SDK, sample a browser, render full-scene CPU
RGBA, promote a Demo, or change the geometric AA or exterior color rule.

The retained twelve comparisons originally report 4325 unexplained non-AA
pixels each. The old independent boundary helper only describes explicit Shape
paint and ink geometry. It omits Table cell/grid material transitions and
Sticky default fill/stroke boundaries. A native microfixture using the actual
old helper fails at a table's internal grid (zero eligible boundary pixels).
The supplemental source-only helper marks that real boundary while leaving
strict cell interiors unmarked.

The new helper decodes the original typed Snapshot. Table boundaries come from
actual cell rectangles and the filled outlines of all grid strokes, including
internal lines. Sticky boundaries come from the default or explicit fill and
the actual stroke outline, respecting width, cap/join/dash and read-only sketch
geometry when applicable. Paths are mapped to the original f32 device matrix
and traced with the same 2-device-pixel boundary construction. The old edge map
is retained and unioned with this supplement; reference RGBA is unchanged.
Unknown paint, parented material and visible text fail closed. The literal
fixtures have four empty Table cells and no nonempty child/shape text; Sticky
v1 itself contains dimensions only. No text-rendering qualification is claimed.

Exactly two edge-only full-viewport jobs completed, each in about 0.2 seconds,
with process-family RSS below 105MiB and verified cleanup. Both supplementary
maps are identical. They add 33348 boundary pixels beyond the old map, leaving
17365031–17390216 pixels strictly compared. All twelve original images are checked
again, with their snapshot content, current camera, target dimensions and
presentation generation bound independently. Redo uses equal canonical content
rather than assuming its label matches final.

Every comparison falls from 4325 to 302 unexplained pixels. All 2148 Table and
1875 Sticky discrepancies are accounted for by true material boundaries. Six
negative microfixtures render actual wrong material through the original mapper
and call the unchanged comparePixels: bad cell fill, omitted wide grid, moved
table, wrong grid color, bad Sticky fill and moved Sticky all remain rejected.
Whole-object exclusion would fail those tests. Additional image tests preserve
strict color checks, exact-tie versus near-nontie behavior, crop/pose/resource,
opacity, occlusion and unsupported affine rejection.

The remaining 302 pixels lie on one interior image discontinuity row. The PNG
is 880x880, displayed as Fit 120x120 at world (800,700), with the exact resource
hash and camera preserved. At device pixel center y=3541.5 the exact rational
source coordinate is 80215784/729237, approximately109.999607809, not an integer
tie. Explicit f32 matrix/inverse/FMA and normalized-quad diagnostic sequences
also fall below110. Five inspected Skia source files match the accepted pinned
commit byte for byte; they confirm source-quad normalization and varying-based
texture lookup, but do not establish this retained GPU backend's exact varying,
viewport, backing texture or nearest-selection precision. These diagnostics do
not authorize an alternate expected color. No epsilon, enlarged AA mask or
neighboring-texel acceptance has been introduced.

The narrow result is BLOCKED_EVIDENCE / UNPROVEN_REFERENCE_REALIZATION with a
validated material-boundary correction and 302 unexplained image pixels per
capture. The same residual occurs in baseline and candidate; it does not prove
a new candidate regression. It also does not qualify the original mixed output.
The old Render result remains immutable. Performance, earliest retained/LOD
feedback, the remaining C convergence wait and immediate zoom-out latency are
outside this package. No new Demo is served and no commit or push is made.

Exact artifacts are under
`.aegis/results/G5-mixed-reference-qualification-r1/`.
