# Correctness harness repair r2 — FROZEN_VERIFICATION_FAILURE

The authorized repair is implemented in the shared worktree. Runtime/SDK,
consumer/input schedule, native oracle, limits, old packages and old windows
remain unchanged. The source-scope audit identifies all 12 changed code/test
paths, within the 13-path allowance.

The driver retains hand selection HUD, uses the independently fixed snapped
transform, captures before/live/after state, and requires matching public/native
snapshot identity, order and transform before live-vs-canonical comparison. Cold
transport uses <=32 KiB units and <=8 units / 256 KiB per encoded batch, source /
received SHA256 and finish/cancel ownership. Coarse stages bracket render/settle,
readback/hash, transfer, snapshot, native compare and reload/close. The single
30s correctness phase and 90s outer guard remain unchanged. One exact repaired
worker/helper tuple is pinned in JS and Python; paired-init matching is strict,
unique and correctness-only. Other modes retain init-only admission.

114 Node tests and 120 boundary contracts pass. Python ran 60 tests: 59 pass,
one unchanged r4 accepted-reuse positive test errors because it requires the
current compiler to match the old frozen initialization_basis.mjs hash. The
production refusal is correct. The test is outside this task's source scope;
neither the reader nor old reuse packet was weakened. See frozen-failure.json.

All 17 immutable recordings were recompiled independently into new output.
canonical-chain.json, decoded.json, metrics.json and external-metrics.json are
byte identical to accepted baselines; every summary field is identical except
explicit compiler provenance. OFFLINE output is rejected as fresh admission.
The original cohort/120/240 remain recorded evidence; the new compiler requires
independent reuse acceptance before another sampling window.

Actual archived OFF1 reproduces the old 16112-pixel RED. With frozen public HUD,
file-only OFF0/OFF1 recomparisons both have 0 residual mismatches. Original FAILED
and timeout124 records remain unchanged; original live public capture is missing,
so these diagnostics do not retrospectively qualify the old partial worker.

This repair ran no browser worker, no build and no valid-snapshot native probe.
The full Python suite executed the 2 explicitly permitted malformed-snapshot
native checks. Historical sampling stays 33 starts / 1251 seconds. No old window
was reopened. Six correctness plus 30 main remain unauthorized and unexecuted.
No commit/push, UR0c/UR1, platform/performance or P34 PASS is issued.

Return the exact mandatory-suite failure to control for its narrow test-contract
correction and refreshed compiler/reuse acceptance. Preserve the implementation
and all invariant recompilation evidence instead of repeating completed work.
