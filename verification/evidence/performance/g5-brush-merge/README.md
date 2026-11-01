# G5 and brush integration evidence

Scope/results: docs/planning/G5_BRUSH_OPTIMIZATION_MERGE.md and
docs/planning/G5_BRUSH_OPTIMIZATION_MERGE_RESULTS.md. Exact merge parents and
artifact identities are in summary.json/identity.json. Earlier branch evidence
is preserved without relabeling it as merged verification.

Reproduce from root with a completed matching SDK:

```
cmake --build out/g5-settle/native-on -j4
ctest --test-dir out/g5-settle/native-on --output-on-failure
node verification/sdk_js/verify_browser.mjs SDK_DIR CHROME_PATH OUTPUT_JSON
node verification/sdk_js/verify_navigation.mjs SDK_DIR OUTPUT_JSON
AXIOM_INK_TEST_DPR=2 AXIOM_INK_TEST_REVISION=5 node verification/sdk_js/verify_ink_v2.mjs SDK_DIR CHROME_PATH OUTPUT_JSON
node verification/evidence/performance/g5-shape-drag/browser_runner.mjs OUTPUT_DIR SDK_DIR 64
```

gpu-result.json.gz is the completed exact compiled WebGL probe result. It used
the existing run_probe harness with live browser console diagnostics and a240s
deadline instead of120s; gpu-debug.log.gz records it. gpu.log.gz preserves the
initial timeout. C++ assertions/pixel oracles and artifact hashes are identical.
The diagnostic harness changes only log forwarding and the deadline.

103 browser checks and34 native tests pass. These are synthetic browser and
native observations, not physical-platform or release/Gate evidence.
