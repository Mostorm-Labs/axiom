# Control correctness follow-up (same frozen Render package)

The public flow remains ordinary SDK -> Runtime -> FramePlan -> SkiaRenderer.
Existing PreparedGeometryCache/PreparedBodyJob owns mandatory pressure metrics;
SkiaInkLayers and CompactStrokeRasterJob own coverage. No SDK/Scene/Brush,
GPU completion architecture, budget defaults or v4 contract changes.

The 40,000-knot v5/v6 native case is legal but source + metrics exceeds the 1MiB
optional entry cap. Deferred pressure metrics are mandatory bounded work, so
admission must use the existing shared hard capacity and real critical leases,
not treat optional residency as a document limit. Retain 32-knot advances,
128-command aggregate allowance and original target/pending-pixel semantics.
V4 retains its existing perEntry checkpoint policy. The test must also hold real
shared quota, observe retry/no present, release it and finish with independent
null-cache pixels. Unknown source validation and metrics advances stay measured.

Two continuation paths can record GPU work before SkiaRenderer's gpu.touch:
pressure draw into a capture tile created in an earlier opportunity, and
bodyComplete + historical mask replay followed by early return. Extend the
existing RenderExecutionScope work witness at actual draw sites, without
inventing a completion queue or claiming pixel work on CPU-only preparation.
SkiaGpuLifetime already consults this witness before deciding unused versus
finished-callback submission. Tests wrap real cache continuation in that exact
scope, resume actual GPU work, and finish without polling or a CPU fence. Clear
resources and require the epoch and pending leases to remain unresolved until
the real Metal completion is serviced. No forged ticket completion or GPU timing
assertion; final draining uses the real queue and callback.

RED: actual renderFrame v5/v6 40k/1MiB, plus exact capture-only and mask-only
continuations with real Metal recording. GREEN: progress, bounded commands,
provider pixels/present, lease and independent evaluator parity; targeted old
regressions and matching normal build. Existing B/C metrics belong to immutable
install-review; don't overwrite them or rerun the full browser matrix. New normal
build and narrow exported-SDK correctness check will have separate identities.

R1 files/SDK/evidence and Demos4351-4355 stay retained. Its live server log is
append-only; the old frozen prefix has an exact snapshot in this subdirectory.
No commit/push or performance/Gate/P34 PASS.
