# Navigation feedback scheduling evidence

Scope and interpretation: docs/planning/G5_NAVIGATION_SCHEDULING_RESULTS.md.
Execution scope: docs/planning/G5_NAVIGATION_SCHEDULING.md.
Previous GPU coverage commit 04b3f56cc35811c177f3f11d700ed51861bc7025 is pushed.
The scheduling follow-up is a dirty worktree result, not a release/Gate result.

Reproduce timing (repository root, matching built SDK):

```
node verification/evidence/performance/g5-navigation-scheduling/browser_runner.mjs OUTPUT SDK_DIR 16 forward
node verification/evidence/performance/g5-navigation-scheduling/browser_runner.mjs OUTPUT SDK_DIR 64 forward
node verification/evidence/performance/g5-navigation-scheduling/browser_runner.mjs OUTPUT SDK_DIR 64 reverse small
node verification/evidence/performance/g5-navigation-scheduling/browser_runner.mjs OUTPUT SDK_DIR 16 cold small
```

`before-*` consumes the preserved hybrid demo build. `final-*` and `isolated-64`
consume scheduling artifacts before the final present-failure propagation fix.
All have final Direct-equivalent pixel hashes and full artifact SHA256. The latest
`final-smoke` and `present-*` verification consume that final fix. `identity.json`
records exact source hashes and the latest SDK artifact hashes. Historical root
failures are copied with their existing classification, not newly verified here.

The fail-first logs are red-feedback, cold-red and present-red. They demonstrate
query-before-feedback, cold setup ignoring a work grant, and a failed retained
present returning accepted. Green native/WebGL logs retain actual exit/results.
The WebGL probe writes timing diagnostics on stderr; its `code:0`, ordinary errors
and pixel/behavior assertions determine success, not absence of timing rows.

Browser input is synthetic and GPU execution is Chrome ANGLE Metal. Submission
timestamps are not completed physical display. Actual trackpad, Android and
Windows evidence remains BLOCKED_EVIDENCE. Host/browser scheduling result is
PASS_WITH_FINDINGS due cold first-response and occasional long display gaps.
