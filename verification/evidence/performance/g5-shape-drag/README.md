# Accumulated ink and rectangle drag

Scope/result: docs/planning/G5_TRANSIENT_EDITING_CACHE.md and
docs/planning/G5_TRANSIENT_EDITING_CACHE_RESULTS.md. Local browser result only;
physical platforms BLOCKED_EVIDENCE, no Gate or P34 claim.

From repository root with the matching completed SDK build:

```
node verification/evidence/performance/g5-shape-drag/browser_runner.mjs OUTPUT SDK_DIR 64
node verification/evidence/performance/g5-shape-drag/browser_runner.mjs OUTPUT SDK_DIR 0
cmake --build out/g5-settle/native-on -j4
ctest --test-dir out/g5-settle/native-on --output-on-failure
node verification/evidence/performance/g5-exact-coverage/run_probe.mjs SDK_DIR
node verification/sdk_js/verify_browser.mjs SDK_DIR CHROME_PATH OUTPUT_JSON
node verification/sdk_js/verify_navigation.mjs SDK_DIR CHROME_PATH OUTPUT_JSON
```

The browser runner imports the public facade, creates deterministic revision3
ink and a rectangle, then selects/moves it through synthetic pointer input.
Checks require one terminal semantic generation increment, the moved transform,
no canonical attempt during live drag, actual GL feedback, and no runtime errors.
It also records final pixel SHA256 and SDK artifact SHA256. ReadPixels occurs
after the timed gesture. CPU RAF/GL submission is not GPU execution or physical
presentation. Timings are observations, not universal acceptance thresholds.

before-64 contains the preserved 4192 artifact's original behavior. clean-64 and
clean-0 use the warm target correction before the final terminal overlay-visibility
guard. terminal-64 and terminal-0 consume the final build served at 4193.
diagnostic-64 is corroborative only, with temporary cache instrumentation that is
absent from production. Hashes distinguish all artifacts.

red.log and runtime-red.log show the cache bypass and duplicate canonical
submission before their fixes. terminal-* contains final native/SDK/WebGL and
boundary results. The GPU probe's errors array also carries expected timing
stderr; exit code plus non-timing errors determine success. The earlier invalid
concurrent-link attempt is preserved separately, not included in passing totals.
identity.json records dirty source hashes and evidence hashes; it does not claim
that the base revision alone contains this fix.
