# Exact settle: measured stage costs

Measurement-only branch `codex/g5-settle-timing`, based on
`89cbd51e1930c8fbd95637614190d7de17aa975c` in Mostorm-Labs/axiom.
In-progress compact-material/brush tuning in the original worktree is excluded.
No preparation-context optimization or pixel algorithm changes are implemented.
See G5_SETTLE_TIMING.md for execution scope, and the evidence README for timers.

## Findings

The cost attributed earlier to repeated plan construction is real but secondary
for this corpus. The principal cost is CPU coverage rasterization, including
raster tile allocation/clear and the pinned coverage draw. A recorded SkPicture
avoids regenerating commands; replay into a CPU raster target still shades pixels.
The final WebGL provider does not make this stage execute on the GPU.

Chrome ANGLE Metal Apple M2, 800×500 logical pixels, fixed-timestamp 16-sample
strokes, 120 synthetic RAF-paced ctrl-wheel events, current round-handdrawn
carrier at the base revision. Each row is a diagnostic run, not a product SLO.

| Strokes | DPR | Exact receipt delay | Canonical-attempt CPU wall elapsed | CPU coverage | Query/source/encode/digest/FramePlan | Gaps between attempts |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 16 | 1 | 397ms | 164.0ms | 143.7ms / 87.6% | 3.9ms / 2.4% | 96.5ms |
| 16 | 2 | 519.6ms | 403.9ms | 367.3ms / 90.9% | 4.2ms / 1.0% | 11.2ms |
| 64 | 1 | 1209.9ms | 567.4ms | 412.6ms / 72.7% | 46.7ms / 8.2% | 493.6ms |
| 64 | 2 | 2146.7ms | 1927.2ms | 1774.0ms / 92.1% | 44.2ms / 2.3% | 60.2ms |
| 64, reverse | 2 | 2118.7ms | 1920.2ms | 1735.7ms / 90.4% | 46.8ms / 2.4% | 66.5ms |
| 64, repeat | 2 | 1845.3ms | 1641.3ms | 1443.0ms / 87.9% | 49.2ms / 3.0% | 67.3ms |

Percentages use exclusive stage time divided by canonical-attempt elapsed.
Inclusive times are nested drill-down only, never added to their parent. The
phase between user input and first preparation includes the existing 100ms idle
policy; gaps include RAF pacing, browser/bridge/logging/scheduling, not measured
GPU execution. Fence insert/poll observations are recorded separately. The CPU
present/flush call is not the final platform completion or input-to-photon.

Every 64-stroke run observed 64 attempts, 63 retries and one accepted submission:
- 4096 source entries traversed and 4096 entries passed into FramePlan copies;
- 2,807,680 canonical reference bytes encoded;
- 49,152 knots scanned by preparation support/admission;
- 64 groups actually rasterized (not 4096);
- 240 CPU coverage tiles / 15,581,184 allocated coverage pixels at DPR2;
- no provider present during partial preparation; one final provider present.

These work counters explain the two distinct opportunities. Source/plan work
repeats quadratically in this all-visible, one-group-per-attempt case. Each
stroke's expensive pixel generation happens once per exact camera preparation.
For the measured corpus, eliminating all measured query/source/plan cost could
save only about 44–49ms of attempt work at DPR2; this is a measured subtotal, not
a promised end-to-end speedup or bound for larger/mixed scenes.

At DPR1 the 64 groups still cause one attempt per RAF and roughly half a second
of gaps, despite less CPU work. At DPR2 per-group atomic work is itself long;
raising maxGroups alone cannot solve it, and can worsen responsiveness. The
4ms check occurs between groups, not inside CPU coverage.

## Recommended next slice

1. Retain immutable exact-demand sources/plan and admission metadata over retries
   with complete scene/camera/metrics/surface/resource stamps. This prevents the
   observed 4096-entry repetition and future scene-size amplification, but should
   be described as housekeeping/locality work, not a one-second cure.
2. After the separate brush tuning result is frozen, target coverage pixel work:
   shared-GPU coverage experiment or bounded exact support/raster optimization,
   always compared to the pinned CPU Direct oracle for all materials and masks.
3. Make exact coverage/material work resumable inside a stroke, then adapt the
   cooperative admission to actual spare frame time. Preserve owner-thread Skia
   context rules and do not create a private worker pool.
4. Progressively replace validated exact regions while retaining complete visual
   feedback; stale/nearest-scale regions never qualify CanonicalVisible/ARC.

## Evidence quality and limits

Instrumentation is compile-time opt-in/default OFF. It adds no public SDK command,
export, accessor or runtime semantic state. Numeric fixed-size rows include view,
semantic/scene/camera/surface/metrics/frame stamps; no user geometry is serialized.
All rows pass exact inclusive/exclusive accounting checks, outcome checks and
retry/present isolation. Clock probes and stderr collection still perturb timings.
No claim that observer overhead is zero is made. Some measurements overlap local
builds/other verification; repeat data is retained to show variability. This is
not a controlled before/after speedup benchmark or a physical trackpad test.

Matching ON/OFF browser inputs produce identical final RGBA SHA256 for 16 DPR1,
16 DPR2 and 64 DPR2. OFF emits no timing records. Timing-enabled G5 CTest passes 27/27, including actual settle retries,
nested-accounting/exception/disabled macro tests and existing material/cache
parity. The unchanged baseline G5 suite passes 25/25; SDK Node checks pass 2/2.
Existing root SDK browser acceptance, context loss/resize/pending fence handoff,
Shell/Axis and navigation feedback regressions use the diagnostic WASM unchanged.
The usual reference source/canonical identities and receipts remain authoritative.

Raw rows, aggregation commands, pixel identities, checks, artifact/source SHA256
are in verification/evidence/performance/g5-settle-timing. This is engineering
measurement evidence; it neither closes the 22 previously recorded full-suite
failures nor issues G5/P34/physical platform acceptance.

All seven modified Runtime translation units also compile with diagnostics OFF;
their object files contain no timing-record marker.

No full 236-test repository rerun was performed for this instrumentation-only
slice; its existing 22 failures remain in the preceding evidence. This result
does not represent that suite as green. Final SHA256 binds source/assets even
where cached BuildInfo retains its configure-time revision.
