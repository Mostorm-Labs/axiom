# Crayon renderer performance repair

User authorizes fixing lag after visual crayon revision 3. Preserve current dirty
crayon changes on codex/sdk-shell-axis-bridge@45938333 ancestor. Runtime/Ink still
owns input; OperationEngine owns persistence; SkiaRenderer owns disposable
coverage surfaces. No public SDK/wire/material/geometry change, no commit/push.

Measured baseline, 36 confirmed points at size32 on 1000x650 logical surface:
input <1ms total; native raster crayon3 preview p50 ~24ms DPR1 / ~74ms DPR2.
Preview allocates CPU Raster coverage even for GPU targets. A 2x canvas exceeds
8MiB per surface and falls back to full-history tiled CPU reconstruction. Direct
canonical path also uses CPU Raster coverage tiles. Extend existing owners only.

Repair: allocate crayon coverage through the acquired Skia target's compatible
surface factory; keep bounded view cache through normal 2x canvas sizes with
explicit aggregate residency limits; reuse transient compatible scratch surface;
retain fallback bounds for oversized views. Do not redesign the scheduler or
geometry. Old materials and shader bytes remain unchanged.

EXECUTION_CLOSURE_CONTRACT
Required: actual target-compatible surfaces in crayon preview/direct; no full-history
fallback at 1000x650 DPR2; bounded retained+temporary bytes; clean frame no-op;
pixel/seed/range/transform/rebind/cancel/erase parity; before/after real public SDK
browser input/render timing at matching scale; native focused and full CTest with
known22 baseline failures reported; matching Web rebuild, ordinary/DPR2 browser
functional runs, local page available; boundary/types/diff checks.
Forbidden: new canonical representation, material restyling, dependency rebuilds or
locks, platform geometry, SDK publication, commit/push. User visual/perceived lag
validation is pending even if measured synthetic timings improve.
Terminal: LOCAL_REVIEW_READY_WITH_BASELINE_FINDINGS or explicit environment/new
regression blocker. Continue through verification; no Gate or release verdict.


Measured Web baseline (DPR2, same headless Chrome/software GPU, 32 points x2):
p50 565.84/644.11ms, release 2129/2455ms. Target-compatible surfaces + retained
prefix yield p50 49.98/50.15ms, release 99.83/131.51ms. These are CDP+two-RAF
completion durations, not isolated frame timings. Shared machine build load can
influence results. Native CPU-only timings cannot demonstrate the GPU benefit.
Before/after screenshots differ at most6/255 per channel (CPU/GPU rounding),
mean absolute error0.01835/255 over full DPR2 image; no channels exceed8/255.

Regression: InkV2RetinaPreview observes rendered knots4352->951 on80 samples,
retained10,400,000 + temporary10,400,000 bytes, under32MiB. Scratch matrix resets
each draw; resize/rebind/viewport invalidates retained target resources. Native
wax/pixel/lifecycle tests continue to assert exact parity on the raster target.

Repeatable browser timing harness: verification/sdk_js/verify_crayon_latency.mjs.
No sampling/input throttling or quality change. All texture shader bytes and
resource identity remain those of crayon-material-v3.
