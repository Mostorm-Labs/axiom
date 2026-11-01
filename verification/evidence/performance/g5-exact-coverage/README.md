# Exact coverage engineering evidence

Execution scope: `docs/planning/G5_GPU_COVERAGE.md`. Result explanation:
`docs/planning/G5_GPU_COVERAGE_RESULTS.md`. Source and SDK identities: identity.json.
Engineering classification PASS_WITH_FINDINGS; GPU exact qualification and physical
Android/Windows evidence remain BLOCKED_EVIDENCE. No G5/P34 PASS.

Default native/cache and real WebGL probe require byte-for-byte Direct pixels.
Candidate probe deliberately exits 1: GPU counter proves actual GPU coverage,
then resumed tapered v3 geometry fails the unchanged pixel oracle. Earlier narrow
coverage logs isolate quantization before material composition; readback is test
only. Candidate option is false in ordinary Runtime/SDK construction.

The RED long-stroke test fails on the original atomic owner. GREEN tests cover
one-step admission, long v3 taper, caps, content/camera supersession, target loss,
no partial target/present/submission, eventual Direct parity, temporary reuse,
budget fallback, both material versions, five families, mask/affine/DPR/clip and
navigation receipt isolation. Native G5 suite and browser results accompany this.

Chrome ANGLE Metal Apple M2; synthetic fixed-timestamp 16-sample strokes,
800x500 DPR2, brush round-handdrawn revision 3, 120 ctrl-wheel events. Same source
baseline a1f724cd built separately with the same locked SDK. Single observations;
not stable SLO, physical trackpad latency or GPU execution measurement.

| Corpus | Baseline CPU attempt total | New | Baseline p95/max attempt | New | Baseline receipt | New |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 16 strokes | 483.1ms | 436.6ms | 43.7/43.7ms | 22.3/24.5ms | 627.8ms | 677.8ms |
| 64 strokes | 1968.2ms | 1730.1ms | 31.9/37.8ms | 19.8/23.1ms | 2146.7ms | 2282.5ms |

64-stroke admission knot scans fall 135168 -> 2112; attempts increase 64 -> 129,
so total exact receipt is NOT faster despite smaller callbacks/work. CPU coverage
remains the dominant limit. Default final RGBA differs from the original cached
v3 baseline because identity-matrix AA recording was incorrect; the new result
passes full Direct oracle at actual matrix, with no oracle tolerance change.
The work budget is admission only, not preemption or a maximum callback promise.

Reproduce:

```
cmake --build out/g5-settle/web-on --target g5_coverage_probe axiom_js_runtime -j 4
node verification/evidence/performance/g5-exact-coverage/run_probe.mjs out/g5-settle/web-on/sdk/web
node verification/evidence/performance/g5-exact-coverage/run_probe.mjs out/g5-settle/web-on/sdk/web --gpu-candidate
AXIOM_TIMING_SETTLE_FRAMES=1200 node verification/evidence/performance/g5-settle-timing/browser_runner.mjs out/run out/g5-settle/web-on/sdk/web 64 2 forward 3
python3 verification/evidence/performance/g5-settle-timing/check_trace.py out/run/trace.log
```

Set EMSDK_PYTHON to an installed Python >=3.10 for Emscripten. Probe target is
explicitly opt-in AXIOM_BUILD_G5_COVERAGE_PROBE and excluded from SDK installs.
Browser observations bind actual wrapper/WASM/data hashes. Full 236-test repository
suite was not rerun; the 22 previously recorded failures are not declared fixed.
Temporary reuse has four slots, budget leases, Skia immutable snapshot/COW safety;
transient backend copies are not represented as zero GPU memory cost.
