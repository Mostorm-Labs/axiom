# SDK 0.3.7 release

Patch iteration after the immutable `v0.3.6` release. Fast-forward integration
of `6c2cf00ecdcaedaaa82f8c79d55d0ceb9b81854e` preserves its original history.

The Runtime batches platform gesture updates and reuses retained navigation
tiles and GeometryChunk-local sources. Image and contour preparation are cached.
Selected-object wheel pan and zoom retain navigation feedback while selection
chrome is drawn at the current camera; exact canonical rendering follows settle.
No public JS/DLL API, ABI version, Operation or Snapshot format changes.

Local release checks passed: nine focused navigation/render/image/text/selection
CTest cases, 37 release-packaging unit tests, two standalone Web server tests,
strict JS consumer TypeScript checking, the Runtime boundary checker, version
matrix consistency and `git diff --check`. Locked Semantic dependencies resolve
offline from the persistent Store; Skia uses the accepted prebuilt SDK.

The tag-triggered SDK workflow builds and verifies matching Web/Windows packages
and publishes the GitHub Release plus `@mostorm/axiom`. The RN workflow builds
macOS, iOS device/simulator, Android arm64/x86_64 and Windows Runtime artifacts,
then publishes the shared RN package and its four platform packages. Every
package uses `0.3.7`, the same source revision and the `latest` npm tag.

The original engineering evidence is under
`verification/evidence/performance/g5-selected-wheel-navigation/`.
Synthetic browser regression results do not establish physical input-to-photon
performance or renew physical platform/Gate qualification. Existing SDK limits
and separately recorded failures remain unchanged.
