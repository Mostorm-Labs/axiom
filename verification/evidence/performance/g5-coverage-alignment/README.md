# V3 exact hybrid coverage evidence

Scope/result: docs/planning/G5_GPU_COVERAGE_ALIGNMENT{,_RESULTS}.md.
Source/artifacts: identity.json, dirty descendant at a1f724cd; prior in-progress
slice preserved. No brush shader/manifest, semantic/SDK lock or public API edits.

RED: pure GPU v3 fails 3 channels / max 4 on first full-frame fixture; long
resumed tapered stroke also fails. Capturing GLSL and forcing highp in a
throwaway browser interception still fails. No shader interception is shipped.
GREEN: actual WebGL/Ganesh validates the new exact saturated-interior/CPU-edge
realization with byte-for-byte CPU Direct equality. Tests include 48 tuned
crossing/mixed-radius cases, size 1..4096, round/marker, butt/round caps, taper,
transparent background, masks, reflections/affine/DPR, 600x300 multi-tile seams,
edge extents, and denied region-budget fallback. Long jobs keep no partial
present, latest camera/content wins; legacy material1/2 five-family corpus
remains on the pinned CPU exact path. GPU/correction target lifetimes and
bounded resource leases remain under Render owner. No readback in production.

Final matching SDK benchmark, 800x500 DPR2, round-handdrawn revision3, 120 RAF
synthetic wheel events; Chrome ANGLE Metal Apple M2. These are diagnostics with
other verification jobs active, not a physical latency/SLO/GPU timing claim.

| Corpus | Prior CPU fallback exact receipt | Hybrid | CPU attempt total prior -> hybrid | Peak attempt prior -> hybrid |
| --- | ---: | ---: | ---: | ---: |
| 16 strokes | 677.8ms | 447.7ms | 436.6 -> 226.4ms | 24.5 -> 14.1ms |
| 64 strokes | 2282.5ms | 1495.2ms | 1730.1 -> 1164.6ms | 23.1 -> 20.0ms |

Earlier matching-shader hybrid run observed 1.32s/872ms for 64 strokes. Variance
is retained; final source/artifact row is authoritative for delivery. Final RGBA
SHA256 matches prior CPU fallback on 16 and 64 strokes; no loosened threshold.
The work counter classifies conservative region/tile area, not measured GPU
execution or count of every segment-fragment. Total CPU pixel/shader work and
resumption scheduling remain optimization opportunities. Legacy gradient GPU
and exact GPU AA edge matching remain open. No new physical Android/Windows PASS.

Native G5 component suite 27/27; v3 tuning passes. Actual external root SDK browser,
pending input/resize/real context loss/restore, Shell/Axis, demo and navigation
regressions pass with artifact hashes in their observations. Node baseline 6/6,
SDK error contract, Runtime boundary and workspace pass. Timing OFF render units
also compile with warnings-as-errors. Full-root Release build was attempted:
old tests lose assert-only variable uses under NDEBUG and fail -Werror. Unmodified
source was additionally built diagnostically with assertions enabled only for 43
assert-dependent test compile rules in the ignored generated Ninja file; production flags
and source remain unchanged. The excluded SDK test target was built explicitly. Default/incomplete and diagnostic CTest logs are
kept separately; see full-failure report for exact remaining names. Neither is
represented as official Gate evidence.

Reproduce probe with AXIOM_BUILD_G5_COVERAGE_PROBE=ON; use
run_probe.mjs in sibling g5-exact-coverage with --gpu-candidate. Root SDK timing
runner supports positional brush revision3 and AXIOM_TIMING_SETTLE_FRAMES=1200.
Interactive immutable artifact copy: http://127.0.0.1:4190/example.html.

Final diagnostic root CTest: 218/238 passed; 20 failures remain.
Exact names are in full-diagnostic-failures.json; comparison to the pre-brush-v3
236-test historical set is in failure-comparison.json and is not equal-config proof.
