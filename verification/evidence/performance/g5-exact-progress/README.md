# Exact checkpoint progress

User-authorized continuation after the committed Web baseline `8dcaa021` in
`Mostorm-Labs/axiom`, branch `codex/geometry-navigation-integration`.
Result: engineering `MEASURED`, no official Gate/P34 or physical latency claim.

The old default stopped preparation at 64 calls to a 32-knot checkpoint, even
when the 8ms owner allowance had not elapsed. A valid 4096-knot v4 fixture with
explicitly available time fails this progress guard on the old default (`red.log`).
The private safety ceiling is now 4096 steps; the existing time check, finite
group allowance, explicit single-step overrides, cancellation/generation checks,
100ms navigation idle gate and qualified receipt protocol are unchanged.
The time allowance is cooperative, not a hard deadline or preemption promise.

Native cache/navigation tests pass 2/2, including small budgets, partial target
isolation, Direct parity, supersession and surface/resource lifecycle. The earlier
baseline's remaining tests are not presented as a fresh full-suite run here;
full configured regression is part of the following geometry slice.

The same four workloads, selected/unselected, 3 repetitions, cold/warm and
diagnostic/default builds completed again: 96 phases. Actual Chrome 156 WebGL2
uses Apple M2 Metal through ANGLE at DPR2. ON/OFF pixel checks pass 72/72;
old-default/new-default checks also pass 72/72 (initial, cold-end, warm-end).
The per-phase document/selection/operation/camera/receipt/close checks pass.
Source and full SDK hashes are stable during each run and source hashes match
between ON and OFF. Raw files and their original SHA manifests are gzip copies.

Default build group RAF p95 is 17.5–18.3ms. Group median exact receipt delay
is now **167.3–215.7ms**, compared with **583.3–600.0ms** in the old baseline.
Within each matched workload/selection/phase group, the median reduction is
**64.1–71.4%**. Diagnostic attempts fall from fixed 29 to **3–7**; the CPU work
and geometry are not bypassed. All samples/outliers remain in `analysis.json` and
per-case data. Sequential browser runs are engineering evidence, not causal
hardware-isolated benchmarks or a product SLO. No physical input-to-photon or
GPU execution time is measured.

Recompute both checks using the existing baseline analyzer and this comparison:

```sh
python3.12 verification/evidence/performance/g5-web-cold-warm/analyze.py verification/evidence/performance/g5-exact-progress/diagnostic verification/evidence/performance/g5-exact-progress/default out/g5-exact-progress/recheck
python3.12 verification/evidence/performance/g5-exact-progress/compare.py verification/evidence/performance/g5-web-cold-warm verification/evidence/performance/g5-exact-progress/diagnostic verification/evidence/performance/g5-exact-progress/default out/g5-exact-progress/recheck
```

`before-after.json` holds paired hashes, per-group before/after distributions
and exact source identities. `SHA256.json` covers delivery files except itself.
The next slice is prepared/local geometry and source residency, particularly
v4 full-contour traversal and the remaining 16 warm tile builds. This scheduling
fix does not claim those cache/geometry issues are closed.
