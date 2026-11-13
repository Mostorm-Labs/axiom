# Post-Worker Render material reuse

This scoped implementation extends the existing `SkiaRenderer` and
`SkiaRenderCache`. OperationEngine, input/pending intent order, Scene, SDK
contracts, Brush algorithms and budgets are unchanged. Local builds remain
0.4.3.0. The default Worker capability remains off; sampled builds enable the
existing private capability.

Previously entering live v4 erasure unconditionally retired complete canonical
material. The transient scene could then synchronously replay every nonresident
stroke. Render now retains material qualified by its existing target, camera,
metrics, resource and group dependencies. It prepares cold material through the
existing cooperative preparation allowance before touching provider pixels.
Presentation masks are applied after the complete canonical source; they never
enter the retained canonical image. When the material demand cannot reside, the
existing complete transient working set handles staging rather than a final
synchronous redraw. An oversized staged body set no longer re-admits its entire
prefix before every step.

Canonical preparation now shares complete isolated material across objects with
the same fully validated prepared body/config/seed, exact transform, properties,
ordered permanent mask geometry, crop and device/target/resource basis. Object
and mask IDs remain independent. Every object is still composed in original
order, with its own opacity. Shared pixels hold one budget lease; every alias
holds charged metadata. Deleting the original object cannot retire another
alias's pixels. GPU use remains charged until actual completion. Unknown or
changed material cannot reuse an unfinished job or stale image.

Native work regressions show one/eight identical 7199-knot objects taking
16/128 coverage steps before and 16/16 after. A warm canonical-to-erase request
adds zero coverage tiles after the repair (previously four), with original
reference pixels. Cold unique-transform transient work returns Retry without
altering provider pixels, resumes and finishes. Tests cover original pixels,
mask append/removal/replacement/reorder, seed/pose changes, equivalent fresh
allocations, source/alias deletion and GPU completion. Existing continuity,
allocation refusal, recovery, z-order, 513-object working-set, exact numeric and
checkpoint scratch regressions are retained.

Five finite ordinary Web windows used the original input sequence, source/SDK
freezes, image and restored fixtures. DPR4 paired AB Up-to-drain was 28.293s
baseline and 13.736s candidate; C was 17.481s and 11.566s. Input duration varied
materially (159.544s and 11.373s), so this is a paired observation rather than a
statistical performance qualification. DPR1 candidate AB was 10.233s and C
5.458s, without a new DPR1 baseline. Three final-C images satisfy the unchanged
independent original CPU AA rule and all five primary Operations/four snapshots
match original sequential replay.

The mixed 49-object immediate zoom-out rows show no material improvement:
append Up-to-settled was 9.014s/9.112s, partial erase 8.900s/8.862s. Their
feedback capture is explicitly the first onFrame after all six wheel events
with matching current camera and canonical presented generation. It does not
qualify earliest retained/LOD feedback. First default-FBO GL write remains a
proxy. Unsupported mixed brushes still use the original synchronous erasure
route. The approximately 10.6s C wait after the last Worker result is also open.
A changing active prefix must invalidate an incomplete composite containing the
old prefix, so oversized continuous cold working sets remain a limitation.

The first two mixed-scene CPU pixel references were interrupted by a mistakenly
inherited 10-second runner setting, not a package or environment limit. Those
attempts are preserved. Two authorized corrected attempts, each bounded to
180s/2GiB, completed in approximately35.7s. Both baseline and candidate have the
same4325 non-AA mismatched pixels against the original mixed reference.
They lie in non-ink bounds, primarily thin table/sticky rows; conservative
image-AABB overlap does not establish an image cause. The original reference
lacks text resources and may omit table/sticky material boundaries; spatial
identity alone does not explain all mismatches. This is an unresolved original-reference proof gap, not evidence of
a new candidate regression. No AA tolerance or oracle source was changed, and
no further attempt was made. RR3 therefore remains blocked and4333 is not
served. No official G5/P34/performance PASS or default Demo promotion follows.

Evidence: `.aegis/results/G5-postworker-render-reuse-r1/`. Existing 4331/4332 and
user documents are preserved. This package does not commit, push or merge main.
