# GT-G3-05 implementation context

G3-04 is closed and integrated into canonical `main` at
`11fec1b308de03886ef5294b54654f48df95455f`. The Render Core already owns an
immutable `FramePlan`, a complete nine-kind `ReferenceDrawList`, and the private
renderer-neutral `IRenderBackend` submission boundary.

G3-05 adds only a private Skia-backed Headless implementation of that boundary.
It must consume the plan by const reference, use the existing locked `r1-full-v1`
consumer SDK without rebuilding or modifying Skia, rasterize into a fixed
offscreen configuration, and expose renderer-neutral RGBA/digest observations for
tests. No surface handle, acquire/resize/present feedback, platform host, or
presentation state belongs here.

This package binds the current macOS executor to the published
`macos-arm64-metal/release` asset with SDK ID
`c80d17c76fb2ea4f7f5b3e3c859173c44399cf1e76bb573dbe086dff49c80731`.
Fetch it only with the command in `execution-contract.json`, and pass
`$PWD/.deps/skia-sdk/macos-arm64-metal/release` as `CANVAS_SKIA_SDK_ROOT`.
There is no source-build or alternate-target fallback.

The focused fixture is a hand-authored 256x256, RGBA8888 premultiplied sRGB,
transparent-black, top-left, non-antialiased raster. Its eight pixel-producing
command kinds occupy disjoint integer-aligned regions, Group stays in traversal
but draws nothing, and a simple supported erase mask removes a strict subset.
The test constructs expected bytes independently with test-side primitive fills;
neither Skia nor the production backend may generate the expected raster.

First action: add the frozen Headless contract test and CMake wiring, run the
required RED against the pre-change tree, and record the missing backend boundary
or locked-consumer target failure before adding production implementation.
Then implement the smallest authorized backend, run the focused golden/negative
tests and all frozen regressions, and materialize exact evidence under
`.aegis/results/GT-G3-05/`.
