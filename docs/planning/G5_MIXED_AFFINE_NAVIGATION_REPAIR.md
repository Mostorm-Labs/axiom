# Mixed-scene affine ink navigation repair

User-authorized follow-up in `Mostorm-Labs/axiom`, controlled worktree
`axiom-geometry-navigation/axiom`, branch `codex/geometry-navigation-integration`,
ancestor `7a0a0c6c`. Continue until the local closure below is verified and
committed, or a concrete authority/toolchain/correctness blocker is identified.
No hosted acceptance or Gate/P34 claim is required.

The reported workload is two shapes, sticky, image, table and a long stroke,
with trackpad pan/zoom around fit scale0.57. Earlier coverage lacked shapes,
scribbling geometry, object transforms and this zoom path. A public root SDK
reproduction with a moved v4 stroke shows238ms to1s RAF tails: navigation rejects
nonidentity ink, then exact admission sends that ink to synchronous CPU coverage.
Navigation span selection also compares local chunks against world query bounds.

User clarified that their stroke was not transformed. Preserve the affine defect
as an independent finding. Same-IAB reproduction with their observed brush knobs
(thinning.99,streamline.13,smoothing.61) and draw/fit scale.57 shows84–135ms feedback
gaps without per-input canonical submission. A retained canonical image is rejected
on exposed borders even when the whole document fits inside it. Add a Runtime-owned
Scene object-query proof of empty exposed strips, sharing the existing query cap.
Only proven plain background may extend retained coverage. Query failure, nonempty
strips, stale stamps or budget denial retain the existing tile fallback. A failed
proof is not repeated for that immutable source, so it cannot starve tile progress.
Keep strict full-demand coverage; never infer empty space from optional chunk hints.

Owners remain unchanged: Runtime owns camera/document/receipts, Scene owns derived
local chunk metadata and object transforms, Render owns disposable sources,
cooperative preparation and Skia submission. Extend those owners; no public API,
semantic format, lock, third-party rebuild, UI gesture algorithm or parallel
renderer. Preserve the user's live4211 page/document; publish a separate build.

Plain transformed compact ink is the supported slice. Transformed erased ink
and non-solid/vector legacy forms retain existing fail-closed fallback; do not
relax mask isolation or invent a new material realization. Pure ink must preserve
Direct backdrop blending exactly, using a bounded cooperative frame composite
where transparent per-object caching would add a color-quantization pass.

Required repair: admit valid transformed compact ink into existing cooperative
exact preparation and navigation coverage; apply object transforms exactly once
to coverage/material/masks; select chunks in the correct coordinate space. Keep
budget admission, finite-value validation, stale stamps, clip/erase ordering,
GPU work quotas, zero-timeout fences and canonical completion rules.

Blocking proof: RED/GREEN transformed-stroke cooperative and navigation tests;
translated/rotated/scaled local-chunk tests; strict cached/direct canonical pixels
with and without non-ink; real WebGL2 affine tests; matching before/after root SDK
mixed workload including zoom and transformed scribble; state/pixel/fence checks;
relevant source/visibility/cache/navigation regressions. Run full configured native
and Node verification, report known baseline failures by identity. Bound evidence
to exact source and matching installed SDK SHA256, retaining all tails. Main
acceptance requires eliminating per-input synchronous exact rasterization in the
reproduction and materially reducing submission gaps/RAF tails. Local commits
are the durable review boundary. Physical trackpad and universal SLO qualification
remain separate. Implementation returns findings, not official Gate acceptance.

The untransformed case is the primary acceptance target after the user's
clarification. Require same-IAB before/after input measurement in addition to the
source-bound Chrome matrix. Cover empty exposed pan/zoom borders, unknown/nonempty
rejection, shared query cap, failed-proof fallback progress and unchanged final
pixels. Preserve the independently reproduced affine defect and fix as secondary.
