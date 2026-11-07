# Masked navigation investigation

Result: **BLOCKED_ENDPOINT**. No official Gate/P34 PASS. The single paired
engineering workload is7200 samples, 15 nearly overlapping duplicates, two erasures
at 2x, then settled cold/hot zoom-out/pan. It is not physical trackpad, competitor,
DPR4, scale or full snapshot/History acceptance.

`result.json` and `summary.json` own the machine-evaluated totals and latency.
`raw/red`, `raw/on`, `raw/off` use the committed fixture without intrusive
snapshot capture or profiling. Runtime source is 10a9a0f0 for red and d3960d6b for
candidates; fixture source is a5746d13. Each observation binds full revisions and
artifact SHA256. Every recorded input/navigation DTO check and closure check
passed, but candidates did not reach the 300ms display-gap endpoint.

The ordinary candidate matches the baseline's settled camera and GPU readback
hash at all three phases. `pixels/` is separate screenshot corroboration; the
image-neighbor edge check is not a substitute for the independent geometric-AA
qualification in the GPU checks. All native full-suite failures match the prior
66 names, and 215 native executable hashes stayed unchanged throughout CTest.
Native binaries were compiled from the implementation tree before its commit;
clean WASM artifacts independently bind the committed runtime revision. This is
not a native platform release qualification.

`exploratory/` retains development measurements, partial RSS-limited workers and
snapshots; they are not clean acceptance observations. Development runner source
was evolving: use the committed fixture and exact clean observations for repeat
measurement. Snapshot files are content-addressed through `snapshot-index.json`.
Worker 3GiB process-family RSS is a conservative sum, not unique physical memory.
No runtime or worker memory limit was raised. Intrusive serialization lanes stay
blocked; latency observations do not assert full snapshot/History qualification.

Reproduce the primary lane from the repository root using the accepted locked
SDK/toolchain and an installed matching SDK:

```sh
python3.12 verification/runners/g5/bounded_worker.py --prefix out/masked-run --timeout 180 --rss-mib 3072 -- node verification/runners/g5/masked_navigation_reentry.mjs out/g5-masked-clean-off/js out/masked-run-data 2 candidate
```

The candidate command exits nonzero when the endpoint remains blocked; inspect
`observation.json.gz` rather than equating process status with semantic failure.
For optional intrusive snapshot capture set `G5_CAPTURE_SNAPSHOTS=1`; it is a
separate lane and previously exceeded the worker bound.

`g5-masked-pixel-capture.mjs` and `g5-masked-demo-smoke.mjs` are exact diagnostic
runner bytes. Copy them to `out/` before invoking so their repository-relative
imports resolve. Screenshot capture is outside measured phases. The new Demo is
served at 4269; previous user pages were neither refreshed nor modified.
