# G5 live DPR correctness and duplicated-stroke drag

Engineering fixes on `codex/geometry-navigation-integration`, code `aff81fc1`.
No official Gate/P34 PASS. Original user recording/frames remain local and are
not included here. All distributed images are synthetic public-root fixtures.

The recording showed the prior scene shrinking toward the origin during live
erasure. `drawDamagedInk()` used `draw()` as a resident lookup on an identity
scratch canvas; its cache-miss builder lost the acquired destination DPR and
could compose into its own scratch twice. Lookup now forbids building and uses
the existing correctly transformed GPU fallback on misses. A128-sample public
SDK case reproduces10506 remote interior pixels lost atDPR2 before repair; the
same pixels remain unchanged while contact is down after repair. DPR1/4 also
pass. Private GPU tests independently cover opacity, mask append/cancel and
DPR1/2/4 against full CPU geometry with the existing geometric AA allowance.

The8-copy drag had a separate cause: object translation routed maskless v4
strokes past isolated resident caches into repeated CPU coverage. The existing
cache owner now admits that material on qualified GPU targets. It shares the
same eligibility in preparation and transient draws. Unchanged copies hit their
caches; only the moved source rebuilds. Legacy destination-clear masks retain
the prior path. An early attempt also widened CPU caching and caused1-channel
quantization differences in the strict affine oracle; `red/cpu-qualification.log`
preserves that regression. Final eligibility is GPU-only and strict CPU byte
parity passes without weakening that oracle.

## Public-root comparison

Same7200-point input,1original+8duplicates,48drag moves,800x500 CSS pixels,
DPR2, `round-handdrawn` revision4. Chrome156 / ANGLE Metal / AppleM2. Each row
is a fresh isolated worker. Primary rows are unprofiled. Exports/readbacks are
outside timing. Frame time includes the caller's RAF wait, not physical display
latency. Per-run raw samples, input/operation/receipt invariants, GPU calls,
worker limits and all36installed package SHA256 values are retained.

| SDK / private cache/shared MiB | p95 frame ms | max frame ms |
| --- | ---: | ---: |
| prior `ce5425e0`,64/192 |1402.5|1485.3|
| fixed64/192 run1 |24.0|29.4|
| fixed64/192 run2 |23.6|25.2|
| fixed256/768 run1 |24.2|31.0|
| fixed256/768 run2 |25.0|30.0|

All five rows preserve input/generation/selection/terminal/one-commit/presentation
checks. The four fixed rows have byte-identical final framebuffer hashes. Their
hash differs from the prior CPU path; GPU AA is qualified by independent pixel
oracles rather than requiring prior CPU framebuffer hash identity. Larger budget
has no demonstrated benefit here. This does not resolve capacity policy for all
highDPR or repeated-erasure workloads. Default source settings remain32/96MiB;
64/192 and256/768 are explicitly compiled diagnostic profiles, not public SDK
options or a silent default change. A cache limit does not cover every browser,
Skia or driver allocation. WebGL has no portable totalVRAM query; see the
[MDN WebGL resource budgeting guidance](https://developer.mozilla.org/en-US/docs/Web/API/WebGL_API/WebGL_best_practices).

Profiles under `corroborative/` are excluded from latency rows. Attribution uses
unique identical WASM function-body hashes against the symbol-bearing reference,
never function indices alone. Old hot CPU raster leaves have0sampled leaves in
the fixed drag where their exact bodies are matched; this is not a zero-CPU claim.
Diagnostic transfer/closure/incomplete-build mistakes are preserved and classified
in `corroborative/limitations.json`, not counted as production failures.

A further7200-sample dense final capture compares the GPU canvas with committed
CPU image export after clearing selection through public input without semantic
mutation.1,567,371flat5x5 pixels have0channel differences above2 (maximum1).
This corroborates the primitive-boundary AA tests; the flat-neighborhood check
is not their replacement. The initial selected-canvas comparison included UI
handles absent from exported content and is preserved as non-comparable.

## Regression and boundaries

Native1020/1086: exactly the66inherited failures, no new names;4inherited semantic
test compilation errors remain. Complete unfiltered JUnit, before/after216binary
hashes and build logs are attached. A wrapper postprocess variable error occurred
after JUnit and binary fingerprints were saved; comparison was regenerated from
those complete results, without rerunning/filtering failures.42G5Node tests,
119boundary contracts,9qualified realGPU modes and both demo startup/native-input
smokes pass. Private GPU probe fingerprints remain stable throughout its final
matrix. Primary consumer SDKs have matching clean BuildInfo at`aff81fc1`, root
exports only, and no installed Runtime headers/archives/qualification interfaces.
No semantic/Skia locks, Brush/Operation contracts or old evidence were changed.

## Reproduce

Use the task package for scope, the committed `verification/runners/g5/drag_copies.mjs`
and runnerREADME for commands. `drag` measures the full long-stroke fixture;
`pixel` isolates the live128-sample DPR regression. Private modes are
`--live-dpr-only` and `--translated-v4-only`, alongside the seven prior qualified
GPU modes. Installed artifacts must match JS/resources/WASM and BuildInfo; build
using accepted prebuilt SDKs only. All workers keep180seconds/3GiB bounds and
explicit process cleanup. `python3.12 verify_evidence.py` checks archived integrity,
full regression names and the raw public-root comparisons. This engineering bundle
does not authorize official Gate completion or replace physical device evidence.
