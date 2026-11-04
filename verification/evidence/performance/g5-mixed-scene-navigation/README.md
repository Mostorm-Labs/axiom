# Mixed-scene navigation feedback repair

Classification: **PASS_WITH_FINDINGS**, local implementation/performance evidence.
No official Gate or P34 acceptance is issued. Production revision:
`31e88f2b94d9027f1614be64f37882dc058bca2e`, clean when both SDK packages were
configured and built. The subsequent evidence commit adds only evidence and a
common-suite call to an already verified renderer test; SDK source remains the
production revision. See `source-identity.json`, `build-facts.json` and
`consumer-install.json` for source, dependency and installed artifact identities.

## Reported case and ownership

The user confirmed that the long stroke was **not moved, resized or rotated**.
The primary reproduction uses two shapes, image, table, sticky and an untransformed
8192-sample scribble, draw/navigation zoom 0.57, and the brush settings observed in
the live page: size 16, simulated pressure, thinning 0.99, streamline 0.13,
smoothing 0.61. This creates seven semantic objects. The heavier nine-object
reproduction uses three long lines with the same mixed objects. It is not an
export or byte-for-byte reproduction of the user's personal document or image.
The live 4211 document was preserved without reloading it.

Runtime continues to own camera, semantic state and canonical receipts. Scene
owns object bounds and transforms. Render owns disposable navigation feedback,
cooperative exact preparation and Skia submission. No consumer SDK API, semantic
schema, dependency lock or version was changed.

## Root cause and repair

Retained-image navigation rejected newly exposed viewport borders even when they
contained only plain background. Tile preparation then delayed presentation while
RAF continued running. Runtime now supplies an authoritative Scene object-query
proof for exposed strips. Only proven-empty background extends retained coverage;
unknown, nonempty, stale or failed proofs keep the existing tile fallback. Proof
and tile queries share the existing bound. A failed proof is disabled for that
immutable source so repeated attempts cannot starve tile progress. Full viewport
coverage and canonical identity/receipt rules remain enforced.

An independent transformed-ink defect was also reproduced and repaired, but it is
not attributed to this user's original case. Plain affine compact ink now uses
cooperative exact preparation and navigation, with transformed chunk bounds in
the correct coordinate space. Pure-ink canonical output retains strict Direct
pixel equality using a bounded frame composite. Transformed erased ink and
unsupported legacy/vector forms retain the existing fallback.

## Measurements

All rows below use the same current runner and corresponding-phase pixel checks,
real Chrome WebGL2 and synthetic wheel pan/zoom through the public root SDK. Frame
timing is OFF in both before and after packages. `gap` means observed maximum
canvas color-submission gap during input; it is not physical display latency.

| Case | Before gap | After gap | Exact settle after input |
| --- | ---: | ---: | ---: |
| Seven objects, repeated navigation | 32–118 ms | 18–24 ms | 382–417 ms |
| Nine objects, repeated navigation | 284–619 ms | 19–35 ms | 829–864 ms |
| Nine objects, after idle | 284 ms | 35 ms | 848 ms |
| Nine objects, after CDP freeze/thaw | 282 ms | 33 ms | 2730 ms |

Final clean-build verification in the actual in-app browser (`iab-final.json` and
`iab-final.png`): nine objects, gap **23.9 ms**, maximum RAF interval **20.2 ms**,
exact settle **880.8 ms**. The final pixel digest matches the corresponding old
build's first phase exactly. Earlier same-IAB seven-object baseline/prototype
measurements are separately labeled in `iab-prototype.json`; stale prototype
build metadata is not presented as the final clean revision.

All corresponding before/after phase pixel hashes match. Document, selection,
operation and camera-return checks pass, with no canonical receipt during active
input and zero blocking-fence timeout. Pixel hashes may differ between repeated
phases because camera floating-point accumulation differs; comparisons use the
matching phase. The baseline's 36 installed files were checked against the prior
archived SHA map (`baseline-binding.json`).

Remaining finding: precise canonical convergence is still about 0.8–0.9 seconds
for nine objects and 2.7 seconds after thaw. Continuous feedback does not mean
canonical preparation is complete. Physical trackpad/display latency and every
possible document/camera configuration have not been qualified. The after-off
seven-object idle run is 60 seconds; its baseline and other runs idle for one
second. CDP freeze duration is five seconds, which is a lifecycle stress test,
not a claim about the user's original foreground-idle state.

## Verification and limits

- Native targeted: G5RenderCache, G5PreparedGeometry, G3Visibility,
  G3DirectReference and G5Navigation: **5/5 PASS** (`final-targeted.log.gz`).
- Common-suite follow-up: G5RenderCache **PASS**
  (`final-ci-cache-test.log.gz`). Only this already-tested call/indentation change
  follows the production revision; full regression was not rerun for it.
- Node targeted: **14/14 PASS** (`final-node.log.gz`). Boundary checks:
  **117 contract files PASS** (`final-boundaries.log.gz`).
- Real WebGL2 background and affine modes: **PASS** (`gpu-*-final.json`).
  Canonical cached/Direct equality is strict. Navigation's independent GPU oracle
  draws the complete contour stream with Ganesh; CPU AA and GPU AA edge pixels
  are not treated as the same raster oracle.
- Full native build/CTest: **999/1066 PASS**, 67 existing failed/not-built cases,
  exactly matching the archived prior failure names; no new failure names.
  Five existing semantic/qualification build targets remain broken. See
  `regression-comparison.json` and compressed full logs; this is not a full PASS.
- Full GPU suite retains the previously reproduced parent failure
  `denied geometry lost full-stroke distance phase`. Parent reproduction is in
  `../g5-navigation-continuity/parent-fallback*.json`.
- Full npm verification is blocked in nine workspaces by missing `tsc` (exit 127),
  preserved in `final-npm.log.gz`. No toolchain workaround is hidden as success.

## Reproduction

From the controlled worktree, use the accepted prebuilt Semantic/Skia consumer
setup and existing configured builds. No third-party SDK rebuild is needed.

```sh
cmake --build out/host --target axiom_render_cache_tests -j3
ctest --test-dir out/host -R '^(G5RenderCache|G5PreparedGeometry|G3Visibility|G3DirectReference|G5Navigation)$' --output-on-failure

AXIOM_NAV_SELECTED=0 AXIOM_NAV_MODES=wheel AXIOM_NAV_PAN_ONLY=0 \
AXIOM_NAV_OPTIONS='{"shapes":2,"lines":3,"points":8192,"scribble":true,"zoom":0.57,"drawZoom":0.57,"brush":{"thinning":0.99,"streamline":0.13,"smoothing":0.61}}' \
node verification/runners/g5/navigation_lifecycle.mjs \
  out/g5-mixed-install/js out/g5-mixed-repeat 1000 2

node verification/runners/g5/serve_mixed_demo.mjs out/g5-mixed-install/js 4214
```

Ordinary Demo: `http://127.0.0.1:4214/example.html`.
Prepared nine-object reproduction:
`http://127.0.0.1:4214/mixed-navigation.html?lines=3`.

`matrix-status.json` records exact matrix commands/options. `before-*`, `after-*`
directories retain raw compressed observations and summaries. `SHA256.json`
binds every evidence file except itself. The final source identity contains a
full source-file map; runner revisions and package artifacts are separately bound.
