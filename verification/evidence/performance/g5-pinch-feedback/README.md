Engineering evidence for the corrective slice described in
`docs/planning/G5_PINCH_FEEDBACK_RESULTS.md`. No formal G5/P34 or physical
trackpad acceptance is claimed. `source.json` binds the implementation; each
delivery raw file and navigation browser observation binds matching SDK assets.

Reproduce from the repository root with the existing verification dependencies:

```sh
node verification/sdk_js/verify_navigation.mjs out/g5-render-cache/web/build/sdk/web out/navigation.json
node verification/evidence/performance/g5-pinch-feedback/browser-runner.mjs out/pinch out/g5-render-cache/web/build/sdk/web
node verification/evidence/performance/g5-pinch-feedback/browser-runner.mjs out/pinch-reverse out/g5-render-cache/web/build/sdk/web reverse
ctest --test-dir out/g5-integration/native/build --output-on-failure
```

`red.log`, `feedback-red.log` and `fence-red.json` are deliberate regression
failures before the respective production fixes. `original-dpr2-profile.json.gz`
is a diagnostic sample, not symbolicated performance attribution. Full CTest
and Node logs preserve every legacy failure; `failure-comparison.json` compares
the CTest names against the preceding committed evidence. Timing measurements
count default-framebuffer GL submissions, not physical presentation. The reverse
run retains its higher exact-settlement latency; it is not discarded.
