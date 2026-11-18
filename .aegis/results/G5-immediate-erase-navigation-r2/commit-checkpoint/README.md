# Immediate erase navigation r2 — progress checkpoint

This is an unfinished implementation checkpoint saved at the user’s request.
It is not an accepted candidate and does not replace the accepted 4346 demo.

Current changes add per-contribution pending display identity, retained complete
masked-plane navigation, stable-grid latest-suffix preparation, frozen mask
conversion reuse, and opt-in native GPU Composition regression fixtures.
OperationEngine remains the canonical commit owner; public SDK APIs and budgets
are unchanged.

Fresh verification: build succeeded and the Runtime boundary checker passed for
122 contract files. Focused CTest passed 3/4: real GPU Composition pan/pinch and
frozen-mask reuse passed. G5ImmediateEraseNavigation failed with
OLD_PREFIX_QUALIFIED_NEW_SUFFIX. The assertion and actual final-suffix/current-camera
pixels still require investigation; no correctness or performance pass is claimed.

The preserved diagnostic ledger has one start charged at 58 seconds. No browser
sampling was added for this checkpoint. Prior diagnostic data still has commit-mask
reuse rejection and long settled-pinch submission gaps; all remain unresolved.
The existing diagnostic SDK precedes the final source freeze and is not a final
matching normal candidate. No new user demo has been published.

See verification.json and ctest.log for exact source hashes, commands and results.
Generated builds, drivers and raw browser output remain under the local ignored
out/G5-immediate-erase-navigation-r2 directory. Earlier frozen results are unchanged.
