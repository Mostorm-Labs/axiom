# G5 feedback timing v2

Method qualification: **READY_FOR_P31**. Runtime source comparison: **INCONCLUSIVE**. This is not a G5/P34/control-review pass. Earlier retirement arbitration remains **BLOCKED**,with its frozen thresholds and raw evidence intact.

## What changed

Only verification tooling changed. The actual event dispatch ledger excludes pre-input sampler RAF alignment from v2 response timing. Default-target WebGL operation timestamps are recorded after calls return. Potential content,clear,masked and offscreen operations are separated. Input-end coarse receipt timing starts at the actual last event and retains both first/last receipts. The observation ends only after SDK RAF pending0/quiet200ms,not merely a coarse onFrame.

The observer schedules no background work. Browser/native clock origins are never assumed equal. Lossless string identities are retained. The primary continuity proxy is browser-timestamped Runtime local accepted-submit diagnostics: navigation v5/presented=true or canonical v1/outcome=accepted. Its timestamp is the diagnostic emission after the call's work;it is not a provider-present callback,per-input camera-qualified receipt,GPU completion or physical presentation. A valid empty view can emit clear-only local submits. The corroborative dirty-source witness has34 local accepted navigation submits across a566.7ms draw-free interval;draw-only absence cannot prove a visible stall.

The first clean pilot attempt exposed an asynchronous dynamic-import startup race (`probe is not defined`). It is preserved in `failed-pilot`,excluded from timing estimates,and fixed by explicit fixture readiness. A delayed300ms public-module import gives RED/GREEN coverage. No Runtime code was changed for that failure.

## Execution and limits

- Method revision: `95eecbbc149742526b1ee27a199d6f243258b51a`.
- Baseline Runtime/SDK: `5672f6aea1007f5bc654038353a6f5f36851a6e4`.
- Candidate Runtime/SDK: `3fcf642bbfdda126246e29dd1b32ec7710154f62`.
- 32 sequential clean-source workers:two AAAA blocks andtwo ABBA blocks for each pan/zoom case;128 complete raw phases.
- Each worker: repeat0,repeat1,1s foreground idle,5s CDP freeze/resume;288 synthetic root-SDK wheel events perphase. This is not hardware trackpad injection,long foreground idle,physical sleep or a GPU eviction test.
- Fixed installed diagnostic-ON SDKs,Chrome/WebGL2,DPR2,636x923 logical canvas,round-handdrawn revision4,five2048-point strokes and mixed objects. Runtime reports11 objects. Unselected primary corpus;selected exploratory witness is separate and excluded from inference.
- Existing32/96MiB budgets,8MiB entry cap and scheduling limits unchanged. No concurrent build/tests/other performance runs. Each worker has180s/3GiB process-family bounds and owned-child cleanup evidence.
- All32 workers retain source/artifact stability,state/selection/operation/camera invariants,post-input receipt,SDK RAF quiescence,idle cessation and bounded nonblocking fence checks. Final pixel digests match across all source labels and repeats **within the same case and phase**. Pan and zoom are not required to share pixels.
- Fresh verification-only regressions:39 Node tests,16 Python tests,118 module-boundary contract files. Existing native failures were not rerun or declared resolved;production C++ code did not change.

## Counterbalanced pilot

Values are middle-pair minus outer-pair means for each of four-run blocks. AA uses the same artifact throughout and measures order/noise;AB uses ABBA. Positive means larger measured cost. The two numbers in each cell belong to distinct blocks;no per-event pseudo-replication.

| Case | First local accepted-submit ms | Steady accepted gap ms | Last event→last coarse receipt ms | Low-density fraction |
| --- | ---: | ---: | ---: | ---: |
| AA pan | 2.30 / -3.45 | -16.90 / 0.80 | 4.40 / -13.90 | -0.006358 / -0.005396 |
| AA zoom | -2.30 / 6.60 | -0.20 / -0.05 | 91.85 / 25.20 | -0.001340 / 0.006890 |
| AB pan | -1.30 / -0.05 | 14.45 / 3.00 | 1.10 / -3.40 | -0.007787 / -0.001466 |
| AB zoom | 5.90 / 0.05 | 0.90 / -1.40 | 17.75 / 182.65 | 0.003012 / 0.013352 |

Two blocks cannot establish nonregression. The frozen successor design requires at least8 independent blocks percase,prespecified margins,family correction and credible same-artifact controls. ABBA cancels linear order drift only. Keep raw extremes and absolute targets separate;do not retrofit new metrics to turn old failures into passes.

## Remaining work grounded in trace

- Worst observed local-submit gap: 182.8ms in `AB-zoom-0-0-A/repeat-0`. Observed callback/event wall coverage: 37.4ms;outside those observed spans: 145.4ms. This residual is not automatically browser scheduling or coverage CPU;the raw nested contexts are retained.
- Worst freeze-phase local-submit gap: 147.2ms in `AA-zoom-1-3-A`;LongTask overlap 138.5ms. Real long tasks remain after fixing metric boundaries.
- Slowest last-event→last coarse receipt: 956.8ms in `AA-zoom-0-2-A/after-freeze`,with 47 exact attempts. Inclusive CPU coverage 340.0ms,contour replay 298.2ms;915 record replays,7686 candidates and6765 rejected candidates. Stage times are nested,not additive.
- Next P31 should review exact tile/contour replay locality and bounded reuse using this corpus,while guarding first/steady submit,clarity,receipt and final pixels. Navigation calls whose total time greatly exceeds existing emitted substage timers also need narrow accounting before assigning their cost to coverage or GPU lifecycle. No source cause is proved by one maximum.

## Reproduce/verify

```sh
python3.12 verification/evidence/performance/g5-feedback-timing-v2/verify_evidence.py
node verification/runners/g5/feedback_observer_probe.mjs out/observer-check.json
node verification/runners/g5/feedback_startup_probe.mjs out/startup-check.json
python3.12 verification/runners/g5/feedback_reverification.py --baseline out/g5-material-retained-on/js --candidate out/g5-retire-on/js --output out/new-unique-pilot
```

`manifest.json` covers every durable file. `verify_evidence.py` checks source/artifact bindings,protocol,raw-byte hashes,pixel parity,block estimates and previous blocked result preservation;the snapshotted metric source recomputes all128 phase summaries. A successful verifier means internal evidence consistency,not Runtime performance acceptance.
