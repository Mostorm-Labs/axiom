# Web C++ SDK consumer

The Release Web package provides `Axiom::Runtime` and `Axiom::PlatformWeb` for
the matching Emscripten toolchain and locked `web-wasm32` Semantic and
`web-wasm-webgl2` Skia SDKs. Configure an outside project with the Emscripten
toolchain, `AxiomRuntime_DIR`, `Protobuf_DIR`, `CanvasSkia_DIR`, and the Semantic
SDK in `CMAKE_PREFIX_PATH`; use `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=NEVER` when
those explicitly supplied SDK directories are outside the Emscripten sysroot.
The package checks both dependency manifest hashes and compiler version.
Link the final Web application for WebGL2 (`-sMIN_WEBGL_VERSION=2` and
`-sMAX_WEBGL_VERSION=2`); the application owns its HTML, event listeners and
any font preloading.

Create and open one `RuntimeComposition`, create two distinct WebGL2 contexts
whose drawing buffers match the requested dimensions, then construct
`WebRuntimeAdapter` with that owner. The application owns the canvases and
contexts and keeps them and any preview-visibility callback state alive until
after the Composition and its providers are destroyed. `bind` stages both
providers; on physical resize the application updates both drawing buffers and
binds again. Keep each Runtime-assigned contact key for move/up/cancel and
cancel active contacts before surface loss. The optional preview callback must
not throw.

Call `prepareFrame` when there is work. A pending result exposes the Runtime's
work IDs; yield to the browser and call `pollFrame` until it completes. Only
completed GPU fences cause Runtime feedback; no scanout time is claimed.
`kRetry` asks the caller to schedule a later opportunity. On surface loss,
discard pending work through `loseSurface`, recreate or restore the WebGL2
contexts, and bind again. Shutdown the adapter and Composition before
destroying the borrowed contexts.

`run_external.py` copies this consumer and the locked Skia SDK to a fresh
outside-repository temporary directory, installs the Web package, configures
and builds the consumer, and launches actual Chrome. Its mouse input is CDP
synthetic input, not a physical-touch or real-IME acceptance. The fixture checks
both contexts, preview and canonical GPU completion, an in-flight loss,
actual `WEBGL_lose_context` loss/restoration events, generation advance, rebind, application font loading, synthetic text input and
shutdown. The consumer does not depend on the Demo HTML or private Runtime
headers. Applications supply font files and IME wiring; this fixture does not
establish their physical behavior. The provided `loadHostFonts` entry is the
existing fixed application catalog and must run once at startup, before other
resources or document content.
