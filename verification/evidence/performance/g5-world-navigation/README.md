This is engineering evidence for world navigation feedback, not formal G5/P34
acceptance. See docs/planning/G5_WORLD_NAVIGATION_RESULTS.md for scope, tests and
limitations. `source.json` binds the dirty implementation; `artifacts.json` binds
the served SDK/probe. BuildInfo may retain its configure-time base revision.

Reproduce from repository root with matching installed verification dependencies:

    node verification/evidence/performance/g5-world-navigation/browser-runner.mjs out/g5-world-reproduce out/g5-render-cache/web/build/sdk/web
    node verification/evidence/performance/g5-world-navigation/repro.mjs

`full-ctest.log` preserves all 22 pre-existing failures. `node.log.gz` preserves
the C8 exact-source git ENOBUFS failure. `identity-red.log` is the deliberate
fail-first duplicate-source case; final guard and GPU logs record the fix.
