# Local mask damage and navigation pixel validity

This is a bounded engineering result, **READY_FOR_CONTROL_REVIEW**, not an
official G5/P34 PASS. Exact metrics and classifications are in `result.json` and
`summary.json`.

The public root SDK workload draws7200 samples, duplicates the long stroke15
times with nearly overlapping placement, performs two25point local erasures at2x,
then measures settled cold/hot zoom-out and pan. The300ms endpoint measures actual
canvas content submissions, not merely RAF callbacks. Raw input/document/receipt
checks, quiescence, artifact identity and GPU readback are retained per run.

Runtime still owns the ordered Scene query and its exact generation/revision.
Render compares complete immutable source bodies, localizes only supported
permanent mask changes and subtracts padded affine control-hull bounds from
fixed8region validity. Unaffected old plane/tile pixels can serve navigation;
old captured FrameState identity never becomes a current canonical receipt.
Unknown bodies, source bindings, lifecycle, resources or bounds fail closed.
Masks, draw geometry and canonical semantic state have no second implementation.

Source comparison witnesses share the existing payload pool and critical recorded
admission used by navigation source proofs. Shared hard96MiB, optional soft32MiB,
per-entry caps and navigation work quotas remain unchanged. Denial drops optional
reuse. Exploratory soft-admission rejection and native RED/GREEN are archived.

The first full regression exposed an invocation-region bug in already-proved
plain-background extension. That RED full run is preserved under `exploratory/`;
the final source updates both bounds and invocation validity after successful
proof. A plane with mask holes never receives that extension callback. The focused
regional mode includes the existing shared canonical/navigation regression.

Native all-build retains four inherited Semantic test translation-unit failures;
`checks/native-comparison.json` names every failed/not-built test and compares the
final full CTest run with the previous66failure baseline. No new native failure or
binary replacement during CTest is accepted. Actual Chrome/Ganesh regional,
v4geometric-AA, progress, mixed backdrop and affine-mask probes are independent of
the main latency worker. G5 Node and Runtime boundary checks are also archived.
An exploratory unrestricted Web all-target build stopped at the existing
`canvas_semantic_g1_07_replay_inspector` unused-template error; the final consumer
SDK and GPU probe targets built successfully. Auxiliary Web tools are not claimed
as qualified by the consumer build.

The clean ordinary/diagnostic SDK builds bind the exact `code_revision`. The
paired baseline uses the previous committed implementation with the same current
fixture. `packages.json` records all36consumer files per package. Ordinary settled
camera matches baseline at all three checkpoints. Main latency-lane readback
hashes are not byte-identical. A separate raw GPU optical lane finds1/1/0changed
pixels, all on AA image edges with unchanged alpha. This is corroborative only;
independent geometric-AA probes remain mandatory. Pixel acceptance uses the
user-authorized AA edge exception and is recorded as `PASS_WITH_FINDINGS`, never
byte-identical parity. Optical capture is excluded from primary latency totals. Old evidence,
Semantic/Skia locks, SDK version and Gate records are preserved.

Reproduce with the accepted locked SDKs and installed matching consumer package:

```sh
python3.12 verification/runners/g5/bounded_worker.py --prefix out/regional-run --timeout 180 --rss-mib 3072 -- node verification/runners/g5/masked_navigation_reentry.mjs out/g5-regional-fixed-clean-off/js out/regional-data 2 candidate
python3.12 verification/evidence/performance/g5-mask-regional-validity/verify_evidence.py
```

This is synthetic Web evidence, not physical trackpad, competitor CPU-percentage,
DPR4, scale, full snapshot or History qualification. Snapshot serialization is
excluded from the latency lane; its earlier separate blocker is not superseded.
The new isolated Demo is at4271; older user pages were not refreshed or modified.
