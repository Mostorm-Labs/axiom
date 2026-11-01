# Draw after zoom evidence

Interpretation: docs/planning/G5_DRAW_AFTER_ZOOM_RESULTS.md. Scope and retained
preconditions: docs/planning/G5_DRAW_AFTER_ZOOM.md. This is a dirty follow-up to
pushed04b3f56c plus the previous scheduling slice; no release or Gate claim.

Reproduce from root with a matching built SDK:

```
node verification/evidence/performance/g5-draw-after-zoom/browser_runner.mjs OUTPUT SDK_DIR 64
DRAW_DELAY=150 node verification/evidence/performance/g5-draw-after-zoom/browser_runner.mjs OUTPUT SDK_DIR 64
```

The runner uses synthetic DOM ctrl-wheel and pointer input. It asserts preview
submission, exactly one new semantic stroke, no canonical attempts during live
input, all post-zoom canonical attempts cooperative, no runtime errors. Timings
are observations, not universal pass thresholds or physical input-to-photon.
Pixel hashes are compared independently across before/after artifacts.

before-0/before-150 use the preserved4191 artifact; final-0/final-150 use the
scheduling+clip-empty correction. latest-* consumes the final settled-camera
scope guard too. Empty-scene/nozoom controls isolate preview from document size.
CPU profile has unresolved stripped WASM function indexes and is diagnostic,
not a symbol-level proof. Source inspection establishes CPU raster preview path.

red and offscreen-red preserve failing behavior tests, green/native/probe/SDK logs
preserve actual results. Timing stderr in the real WebGL probe is diagnostic;
its code and non-timing errors determine result. Source and artifact SHA256 are
in identity.json. All evidence is local and physical platforms BLOCKED_EVIDENCE.
