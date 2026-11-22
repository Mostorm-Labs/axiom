# G5-DPR4-render-sync-r1 control correctness follow-up

`READY_FOR_CONTROL_REVIEW`. This closes the two correctness regressions identified by Control. It does not close the navigation performance tail or issue Gate/P34/performance PASS. Work remains uncommitted/unpushed on `codex/g5-post-range-convergence` at7d3ea0aa plus preserved dirty overlays.

## Repairs

1. Deferred v5/v6 source + pressure metrics use the unchanged shared hard capacity and actual critical leases. Optional1MiB entry residency is no longer a document limit. V4 keeps its prior per-entry/checkpoint contract. [Actual renderer RED](entry-limit-red.json) rejects both legal40,000-knot cases on the first call; [GREEN](entry-limit-green.json) completes both in313 calls, exactly one present, at most128 pressure commands per call,32-knot metrics advances, independent null-cache pixels exact. Real shared quota pressure yields retry and resumes after release. Source/metrics remain charged beyond1MiB and below96MiB; no synchronous replay or budget increase.
2. [Continuation RED](continuation-red.json) proves that resumed capture-pressure and bodyComplete/mask-only early returns were marked unused despite actual GPU draws. Actual pressure/mask draw sites now mark the existing `RenderExecutionScope` GPU work witness, which `SkiaGpuLifetime` already consumes. [GREEN](continuation-green.json) retains131072/283860 pending pixel bytes and an unresolved epoch before actual Metal completion service; all leases drain afterward. No new GPU queue/completion architecture and no GPU work asserted for pure CPU preparation.

Narrow source locations: `skia_prepared_geometry.cpp:438`, `render_execution.hpp:61`, `render_execution.cpp:77`, `skia_compact_material.cpp:131`, `skia_render_cache.cpp:148`. Regression tests are `dpr4_render_sync_test.cpp:234` and284. See [delta from r1](source-delta-from-r1.patch), [source manifest](source-manifest.json), [design](design.md).

## Verification

[Summary](verification-summary.json):13/13 package tests in the46/48 focused native run. The two failures remain `G5CanonicalPreparedCompatibility` and `G5TransientDisplayContinuity`, exactly classified/reproduced on old Render by r1. P36 material and21 Runtime lifecycle rows pass; Python1/1, Node25/26 with the same missing immutable historical SDK fixture,122 Runtime boundary contracts, diff check. No SDK/Input/Interaction/Scene/Operation/History/Brush/AA/budget changes or third-party rebuild.

A new matching ordinary Web SDK (timing/qualification off; same512/1024/128MiB configuration) is at `out/G5-DPR4-render-sync-r1/control-followup/install-normal/js`. The [exact freeze](narrow-freeze.json) separates it from r1 `install-review` and its preserved performance observations.

The correctness-only Web follow-up replays the same18 frozen Operations, then zooms, erases, immediately pans and returns the scale. A completed exact r1 reference is reused by the candidate continuation. The candidate has the same new erasure Operation, final Snapshot and37,440,000-byte RGBA, and qualified receipt generation19; no SDK errors, both runtimes closed. See `out/G5-DPR4-render-sync-r1/control-followup/narrow-web-candidate/observation.json` and the summary. Final Snapshot SHA256 `3f66f418505124e4623450bb725a65c0a7a59b4a14ebf9ce0223261b5d724f50`; RGBA `ec76b828373df947a4baa70daacc238d2fc163babbffb1761ba3bbf3c14389bb`.

Four guarded follow-up starts consumed422 actual ceil seconds, all cleaned. The first two were invalid fixture/identity setups, and the third180s guard preserved the clean r1 reference before stopping the candidate. The final candidate-only run completed in61 ceil seconds with larger base64 buffer transport; it reuses the exact r1 reference rather than rerunning old windows. See [ledger](browser-ledger.json). Every available original buffer is retained with [individual gzip copies and hashes](raw-manifest.json). No whole performance matrix was repeated. These correctness observations do not upgrade or relabel r1 latency data.

## Preservation and remaining findings

All r1 result/package/SDK/sample hashes remain. Its changed working-source/build files are mapped to exact archived copies by [r1 preservation](r1-build-preservation.json); [validation](validation.json) resolves all608 original manifest rows. The old running `review-demo.log` is append-only: its395-byte frozen prefix is preserved as `r1-review-demo-frozen-prefix.log`. New manifests bind that snapshot or an exact prefix, and mark live logs mutable. They never bind full live logs as immutable.

Original13.0/16.5s navigation recovery and unmet3s/5s target remain a future performance finding. Two native baseline failures and the missing historical Node fixture remain. No physical scanout or hardware GPU timing claim.

Old Demos4351–4355 are preserved. New normal Demo: **http://127.0.0.1:4356/example.html**. Its HTTP bytes match the newly frozen SDK. Start review at [result.json](result.json) and [artifact-manifest.json](artifact-manifest.json). No commit/push or new stage is requested.
