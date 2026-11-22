# G5 post-range convergence r1 — control review

Return: **READY_FOR_CONTROL_REVIEW**, scoped fix plus explicit findings. This is
not a G5 Gate, P34 or overall performance PASS. Package and six references match.

Execution root: `/Users/qing/.codex/worktrees/g5-post-range-convergence`, branch
`codex/g5-post-range-convergence`, anchor/HEAD `7d3ea0aaf75c8cf6d26e9eaa691b5d99ee82038d`.
Index empty; no commit/push. Original worktree, dirty r3 and user demos preserved.
Normal candidate: http://127.0.0.1:4353/example.html (kept running).

## Implemented scope

`runtime/interaction/src/canvas_editing_runtime.cpp`, `EditingSceneCompiler::prepareMaskStamp`:
reuse only the already published cumulative hash prefix when body bytes and exact
immutable mask storage owner+pointee match. Sorted pointer identity ends reuse
at the first inserted mask. Existing cursor owner checks, full applied-record
comparison,32primitive/time slices and canonical OperationEngine path remain.
No new cache owner, public API, Render/brush/AA/budget or semantic contract change.

Final actual-owner counterfactual RED:258steps for8192old+64new primitives.
Candidate GREEN:2steps. Front insertion remains260; cold restored history258;
changed values after compensation produce a different correct stamp. Full Scene
records equal the independent original compiler. See final native outputs and
`counterfactual-owner.json`. The baseline owner was compiled into a copied archive
and identical test objects relinked; candidate source/library/SDK were untouched.

## Measurements and correctness

* Retained post-worker8.421s includes6.397s owner calls,0.940s other RAF work and
  1.084s outside RAF. This is not a hardware GPU wait attribution.
* Original interleaved7200Move baseline hit420s guard before Up. Candidate in
  that original pair was not started; partial complete records are preserved.
* Separate batched-before-RAF focused pair retains all7200point coordinates and
  timestamps and AB/Undo/Redo/followup/selection. AB drain34.843s→28.4195s;
  followup62.6654s→68.3428s. Mixed single-pair result; no performance PASS.
* Every Snapshot/Operation buffer matches B/C. Four92,499,840byte actual-target
  captures differ at14–15pixels each, maximum1/255,noalpha difference. This is
  explicitly a rounding finding, not a byte-equality or new AA policy claim.
* Isolated current DPR4 crayon:16defaultoffset copies,2erase/immediate navigation
  phases,AppleM2 ANGLEMetal. B/C final Snapshot and37,440,000RGBAbytes are exactly
  identical; canonical generation18 receipts match. Both have7–8s RAF spikes.
  Scene-prefix reuse does not solve that existing synchronous Render work.
* No private polling/native instrumentation inserted in ordinary timings.
  Lightweight status omits Scene stage counters: recorded as unknown, not zero.
  Physical first-correct feedback and GPU hardware timing remain unqualified.

## Verification limits

Targeted native14/14 pass. Expanded native initially59/63; mixedimage passed on
recheck with the exact original PNG fixture temporarily linked, then removed.
Three independent failures remain: G5CanonicalPreparedCompatibility,
G5SharedSceneRetention and G5CpuPendingRuntime-radius. First two do not link the
modified editor owner; radius also fails with the exact anchor owner. Full build
fails in unchanged object_store_differential_test.cpp and
stateful_kind_applicability_test.cpp on kSection/kTable switch coverage under
Werror; full CTest could not complete. These failures are retained, not patched.

Node25/26 pass with required VM flag; one historical immutable-SDK fixture is
missing. Python1/1 and122 Runtime boundary contracts pass. Both matching normal
Web builds pass using locked prebuilt Semantic/Skia. Initial install-component and
local Node-dependency setup errors are preserved in logs; corrections did not
alter production source or historical evidence. New source diff check is clean.

Five owned browser starts charged1169actual ceil seconds, including the failed
original baseline. All worker process families cleaned. No old r3 sampling window
reopened. Only the explicitly retained normal Demo server remains running.

`result.json`, `verification-summary.json`, `source-manifest.json`,
`source-delta.patch`, `sampling-freeze.json`, `browser-ledger.json`,
`command-ledger.json` and `artifact-manifest.json` are the review entry points.
Remaining priority: attribute the long material/feedback calls and review the
preserved r3 bounded preparation/cache work against current source; retain cold
restore and sorted-suffix costs, pixel findings and baseline failures explicitly.
