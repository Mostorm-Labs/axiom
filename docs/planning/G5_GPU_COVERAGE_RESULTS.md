# Shared exact coverage: implementation result

Execution branch `codex/g5-settle-timing`, merge base
`a1f724cd2c6cc44c773253a5e9aa92f3272ed05c`. Consumes brush tuning v3
`de274161`; no locks, schemas, public JS/DLL contracts or brush algorithms change.

Implemented in existing Render owners:

- Compatible coverage surface creation via SkCanvas, opt-in internal candidate.
  Default remains pinned CPU coverage. GPU candidate fails strict pixel oracle,
  so it is NOT enabled for normal consumers or used to qualify canonical pixels.
- Four bounded full/edge coverage target slots, ResourceBudgetManager scratch
  leases, reuse across camera demands; target/metrics/provider generation change
  releases resources. Immutable snapshots preserve Skia copy-on-write safety.
- A suspended compact stroke job processes at most 32 knots per device tile unit.
  It max-combines all coverage before applying material once. V3 distance/taper,
  caps, predecessor, original device grid and shader coordinates are retained.
- Cooperative exact preparation now checks a knot-step cap and elapsed time
  inside one stroke, retains admission bounds over valid retries, and cancels
  work on content/camera/metrics/resource/target change. Synchronous drawing
  discards pending scratch work. No partial present or canonical receipt.
- V3 coverage pictures recorded at identity are bypassed for exact cache draws:
  V3 analytic AA width depends on the actual device matrix. V1/V2 world-source
  picture reuse remains. This fixes a pre-existing zoom-dependent cache mismatch.
- Diagnostic GPU coverage submission is a separate stage from CPU coverage;
  neither is a GPU execution timer. No public diagnostic API was added.

GPU blocker: narrow readback-only diagnosis found 102 differing coverage pixels
for legacy geometry and 4 for v3 on a 256x128 fractional transform. Differences
were generally one 8-bit coverage level; nonlinear material mapping amplified
legacy final differences to 24 channels / maximum 23. The candidate also fails
resumed tapered v3 output vs Direct. This is BLOCKED_EVIDENCE for GPU exact
qualification, not permission to weaken the oracle. Default CPU cache/prepared
paths pass byte equality on native and real WebGL targets.

The 8ms/64-step owner policy is provisional, not an SLO: a 32-knot unit, admission
scan, final image copy and provider work are not preemptible. Larger 16ms/64-knot
units had worse observed peak callbacks and were rejected. Immediate CPU snapshot
texture upload was also rejected after a slower diagnostic. Old snapshots and
navigation tiles remain feedback only; no progressive partial exact receipt.

Cross-platform: shared scratch/admission/AA improvements apply to all Skia
providers; cooperative settlement is used where the Runtime already requests
RenderPreparationBudget (currently deferred navigation). Android raster/Windows
GDI continue CPU realization; their host pumps are not silently converted to Web
scheduling. EGL/D3D/Metal can reuse the same compatible-surface seam once GPU
pixel authority is satisfied. Host/Web tests do not qualify physical platforms.

Verification and measured timing are in
`verification/evidence/performance/g5-exact-coverage/README.md`.
