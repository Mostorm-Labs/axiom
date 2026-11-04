# SDK 0.3.8 release

Patch iteration on the existing 0.3.7 version line. The user authorized
fast-forwarding main to the completed integration and publishing a new version.
Main was fast-forwarded from `ff8cb00b34c6939a71ea3f8bca8771dbb012f189` to
`21270d3388dd9160ea1262791b901e11875e198f`, preserving both parents and the
original geometry-navigation commits. No squash or published tag replacement.

The integration connects GeometryChunk locality metadata and bounded chunk-range
consumption to the shared Direct/Skia path. Navigation feedback stays warm after
canonical commits while exact canonical rendering and selection overlays retain
their existing meaning. This also includes the coincident-pinch state fix already
present on main. No public SDK API/ABI, Operation or Snapshot format change.

All current version-bearing Web/API/RN files, platform packages, Android asset
cache path and RN release workflow use 0.3.8. The prior v0.3.7 candidate is retained:
its RN common package was published, while macOS upload timed out and the SDK
workflow was cancelled. Partially published versions are not overwritten.

Local release checks cover 17 focused native navigation, geometry locality,
chunk-range, material, visibility, damage, image, text, selection and gesture cases;
37 release-package tests; 15 workflow/server tests; strict JavaScript consumer
TypeScript; and the 117-file Runtime boundary check. Matching Release WASM/JS
consumer and selected-navigation/feedback probes are recorded under
`out/release-0.3.8-*`. The native build reused out/release-host with RF01 tests
explicitly enabled; the Web build reused out/release-web with the accepted Python
3.12 toolchain rather than Xcode's Python 3.9. Semantic resolves offline from the
persistent locked Store and Skia remains the accepted prebuilt SDK.

The v0.3.8 tag triggers the existing hosted gates: Web/Windows SDK and Demo ZIPs,
GitHub Release, Web npm publication, and native macOS/iOS device+iOS simulator/
Android arm64+Android x86_64/Windows builds followed by the five RN npm packages.
All outputs must share the exact tagged revision and version, with `latest` npm
distribution tags. Completion is recorded only after the corresponding jobs finish.

These are engineering and release regressions, not new physical input-to-photon,
IME/device or formal Gate qualification. Known allocation-probe and separately
recorded legacy failures are not represented as fixed by this release.
