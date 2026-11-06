# G5 GPU retirement arbitration

Result: **BLOCKED**,for this focused arbitration slice only. Prior material/navigation performance closures remain open. No official Gate,G5 or P34 PASS is issued.

Tested source:`3fcf642bbfdda126246e29dd1b32ec7710154f62`. Paired baseline installed SDK:`5672f6aea1007f5bc654038353a6f5f36851a6e4` (production-code equivalent to the stable task anchor07f55270). Demo: http://127.0.0.1:4239/example.html . Existing artifacts/servers/evidence remain separate. The candidate is retained for review on the development branch;stable4238 remains available and no performance acceptance is claimed.

## Corrected owner behavior

SkiaRenderer previously discarded the qualified navigation frame on critical admission failure and immediately requested cell eviction. SkiaNavigationTiles selected victims against charged bytes,including logically released GPU allocations. A real GPU allocation must remain charged until its actual completion callback,but those already-released bytes can satisfy future capacity without throwing away additional live feedback.

The stable priority is preserved:wait if existing pending quota suffices;otherwise retire the optional whole frame;recheck its released quota before reclaiming cells. Mandatory cell selection stops once enough space will exist after retirement. Optional replay admission/reclaim and its soft limit are unchanged. ResourceBudgetManager.reserve remains the sole actual admission gate;the projected count cannot allocate,shrink a lease,complete a GPU epoch or issue a receipt. Existing lost/stale/provider rules still apply.

No material-tile path,A2 replay extension,new resource pool,new SDK API or larger quota was introduced. Default32/96MiB,8MiB entry and existing CPU/GPU slices remain in force. This fixes avoidable collateral eviction;it is not a claim that all retained-frame value/capacity decisions are optimal.

## Real-owner RED/GREEN

The unchanged stable production owner fails two new acceptance cases:

- Released scratch is physically queued but already sufficient:old Renderer discards the qualified whole-frame feedback.
- A real8MiB GPU frame has been logically released:old Nav owner additionally evicts useful cells.

The corrected owner passes both. The released-frame control evicts0 cells;the live-frame control still evicts a needed cell. Actual allocation remains denied before real completion and succeeds afterward. Feedback does not increment canonical submissions;the resumed current frame exactly matches independent Direct pixels. Delaying clientWaitSync polls never reports false completion and never blocks the CPU. These private tests do not claim physical driver-OOM coverage.

Raw RED/GREEN,workers,source-test hash and exact artifact identities are under checks/. GPU retirement mode is one of10 actual Chrome/Ganesh modes. Core production changes are confined to private Render arbitration;the canonical Brush/Scene/FramePlan algorithms are unchanged.

## Paired measurements

There are 22 bounded browser workers (11 pairs),DPR2/3/4,diagnostics ON/OFF,pan/zoom,selected/unselected,60s foreground idle in the DPR2 selected-zoom cases,and5s CDP frozen lifecycle. DPR3/4 extra cases use1s idle. No builds/tests ran during this matrix. Final pixels,document/selection/Operation/receipt checks,physical framebuffer dimensions,clean source and stable installed artifact identities are independently verified.

| Representative metric | Baseline | Candidate |
|---|---:|---:|
| DPR4 after-freeze exact wait | 1443.1ms | 1461.1ms |
| DPR4 after-freeze low-density time fraction | 0.6423 | 0.6219 |
| Maximum input submission gap over matrix | 561.3ms | 562.6ms |

This slice's frozen nonregression guards require every ON-phase low-density time fraction<=baseline+0.02,and every phase's wait/input gap<=baseline+max(16.7ms,10%baseline). Deterministic delayed-GPU pressure must eliminate unnecessary eviction. The earlier broader15% cold-improvement/150ms goals were not relaxed or reclassified as passing;they remain separate open work. Source-density is a proxy,not optical blur;CDP freeze is not physical sleep;these timestamps are not input-to-photon/GPU execution timing.

Three predeclared identical-baseline A/A pairs were run after the complete A/B matrix,using the same parameters as DPR2 unselected pan and the same locked baseline artifacts on both sides. They also trigger the wait gate in 2 phases (about17ms). This demonstrates a one-frame wait-noise floor for the current single-pair method. It neither proves the candidate harmless nor dismisses A/B input submission-gap findings. A/A is corroborative and never replaces the frozen A/B decision;see corroborative-aa/ and checks/g5-retire-aa-result.json.

Guards:`{"clarityNoRegression2Points": true, "inputGapNonRegression": false, "waitNonRegression": false}`. Exact per-phase violations,if any,are in performance-violations.json. Do not interpret evidence-verifier exit0 as performance acceptance when result.json is BLOCKED.

## Regression and reproduction

Native:107/112,the same5 known failures:canvas_rf01_full_rebuild, canvas_fc02_shape_label_scene, G3HeadlessCanvasDemo, FC00XRuntimeComposition, SketchSemantic. No new native failure. All10 private GPU modes,31Node tests,118 boundary contracts,normal installed ON/OFF root packages and actual root Demo smoke pass. Root packages expose only'.',omit private Runtime headers/archives and keep developer components OFF.

Run `python3.12 verification/evidence/performance/g5-retirement-arbitration/verify_evidence.py`. It recomputes the frozen guards and verifies SHA256,package authority/input locks,RED/GREEN,workers and source/artifact bindings. Build and worker recipes are in reproduce/. The code revision and evidence are durable in the controlled branch. This is not native Android/Windows evidence or100K/1M qualification.
