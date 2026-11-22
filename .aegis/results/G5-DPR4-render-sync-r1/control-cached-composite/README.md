# Cached pressure composition continuation

`READY_FOR_CONTROL_REVIEW`. The third GPU continuation and staging prefix gaps are fixed. No commit/push, Gate/P34 or performance PASS. The original navigation tail remains.

The repair stays in existing Render: actual `compositeCompactCoverageToSkCanvas` drawPaint branches now record GPU work after successful matrix inversion. `captureOnly` hits do not invoke this helper; CPU canvas cannot mark GPU work. Existing cooperative staging also marks actual background, scratch clear, body-image copy and material-composite writes before quota/allowance early return. No completion architecture, API, budget, Brush or navigation scheduling change.

[Single reusable native fixture](source-delta.patch) now covers pressure continuation, mask-only continuation, cached material composition, and exhausted-allowance staging background. Its ready captureOnly lookup is a negative case and remains unused. The cached-material probe uses DPR2 and the **second cached tile reuse**, so the first scratch clear cannot mask a missing drawPaint marker. The fixture's original source, modified sources, compile setup mistakes and corrected execution are retained.

| Actual owner case | Exact previous Render | Final candidate |
| --- | --- | --- |
| pressure continuation | pass | pass |
| bodyComplete mask-only | pass | pass |
| cached material composition, call38 | unused;0 pending pixel bytes | pending epoch;1979904 pending pixel bytes |
| staging background, call1 | unused;0 pending pixel bytes | pending epoch;131072 pending pixel bytes |

All candidate pixel leases drain to0 after actual Metal completion. The [counterfactual RED](same-fixture-counterfactual-red.json) and [final GREEN](continuation-final.json) use the same compiled test fixture and exact prior Render archive; private layouts are unchanged. See [path audit](path-audit.json).

[Final targeted regression](verification-summary.json):46/48 native, including13/13 package tests; only the same `G5CanonicalPreparedCompatibility` and `G5TransientDisplayContinuity` baseline failures remain. P36 material and21 Runtime lifecycle rows,122 boundary contracts and diff check pass.40k v5/v6 entry-limit cases still complete within128 commands/call with independent exact pixels. Node/Python source and runners are unchanged; previous25/26 and1/1 results are referenced with the same missing immutable historical SDK finding.

A separate ordinary matching SDK is `out/G5-DPR4-render-sync-r1/control-cached-composite/install-normal/js`. [Freeze](final-freeze.json) binds996 source files and42 SDK files; diagnostics/timing/qualification OFF and existing512/1024/128MiB profile. Semantic/Skia prebuilt SDKs reused.

Only one new candidate Web correctness run was started:61 actual ceil seconds, all processes cleaned. It reuses the exact sealed clean reference and unchanged consumer input, without rerunning the long scenario matrix. New erasure Operation,857085-byteSnapshot and37440000-byteRGBA match byte-for-byte; final commit/qualified receipt generation19 matches; SDK errors0 and runtime closed. Observation: `out/G5-DPR4-render-sync-r1/control-cached-composite/narrow-web/observation.json`. Final RGBA SHA256 `ec76b828373df947a4baa70daacc238d2fc163babbffb1761ba3bbf3c14389bb`, Snapshot `3f66f418505124e4623450bb725a65c0a7a59b4a14ebf9ce0223261b5d724f50`. [Ledger](browser-ledger.json), [raw buffers](raw-manifest.json), [reference identity](reused-web-reference.json).

Originalr1 and control-followup source/SDK/results remain traceable through [preservation](previous-preservation.json) and [validation](validation.json); old runtime build files have exact archived copies. Demos4351–4356 remain. New normal Demo **http://127.0.0.1:4357/example.html** serves the exact new SDK. Live logs are mutable and excluded; old frozen prefix snapshots remain.

Review [result.json](result.json), [source manifest](source-manifest.json), [delta](source-delta.patch), [artifact manifest](artifact-manifest.json). R1's13.0/16.5s navigation recovery, unmet3s/5s bands and unavailable hardware/scanout timing remain findings; no performance result is transferred to this build.
