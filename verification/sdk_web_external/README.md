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

For a new, empty document an application may use
`bindInitialResourceManifest` once to bind validated resource metadata, obtain
`resourceToken(id)`, and pass the full token and bytes to
`publishResourceBytes`. The owner validates the token, size and hash; poll
`resourceReceipt` after `ownerPump`. Late tokens are rejected. This startup
binding is an alternative to `loadHostFonts`, which also binds a manifest; it
does not load a saved document or replace a live document's resource manifest.

`openDocument` accepts a nonzero full document ID before surface binding;
legacy `open` retains its existing default ID. `setLocalOperationCallback`
delivers borrowed canonical local operations on the owner thread; it is not a
durability acknowledgment. Callbacks must not reenter or destroy the owner.
`applyRemoteOperation` uses the existing remote apply owner and does not echo
through the local callback. These APIs do not perform file or network I/O.

`captureDocumentSnapshot` captures committed canonical state only when the
Runtime has no active editing/input session. The caller provides an expected
semantic generation and the data owner's document revision and canonical recovery
frontier for that same state. Capability requirements are derived from the
captured objects when omitted, or validated if supplied. A mismatch, busy
session, invalid metadata or wrong thread is rejected. The returned bytes are
not an acknowledgment of storage durability, and resource bytes remain the
data owner's responsibility. `openDocumentFromSnapshot` restores a verified v3
checkpoint into a fresh owner, rebuilds the Scene before publishing the owner,
and rejects mismatched document/frontier/digest. The data layer must supply the
correct checkpoint binding and replay only operations after that frontier; the
snapshot does not restore Undo history or the old applied-operation ledger.
Live-document replacement and a document-close/durability barrier are not yet
provided.

Call `prepareFrame` when there is work. A pending result exposes the Runtime's
work IDs; yield to the browser and call `pollFrame` until it completes. Only
completed GPU fences cause Runtime feedback; no scanout time is claimed.
`kRetry` asks the caller to schedule a later opportunity. On surface loss,
discard pending work through `loseSurface`, recreate or restore the WebGL2
contexts, and bind again. Shutdown the adapter and Composition before
destroying the borrowed contexts.

The example `consumer/pointer_input.mjs` collects DOM pointer observations,
keeps the source/pointer identity, forwards coalesced samples, and cancels
contacts on capture loss, blur or disposal. `consumer/main.cpp` retains the
Runtime-assigned key for each contact and forwards subsequent facts through
that key. The sample maps CSS coordinates to the current logical surface once;
the Runtime owns camera conversion. Run `node pointer_input_test.mjs` from
`verification/sdk_web_external` for the collector tests. Real hardware and all
browser variants require separate testing.

`run_external.py` copies this consumer and the locked Skia SDK to a fresh
outside-repository temporary directory, installs the Web package, configures
and builds the consumer, and launches actual Chrome. Its mouse input is CDP
synthetic input, not a physical-touch or real-IME acceptance. The fixture checks
both contexts, preview and canonical GPU completion, an in-flight loss,
actual `WEBGL_lose_context` loss/restoration events, generation advance, rebind, application font loading, synthetic text input and
shutdown. The consumer does not depend on the Demo HTML or private Runtime
headers. Add `--export-dir <new-directory>` to retain the verified assets and
serve that directory over HTTP; open `index.html` for the responsive
interactive example. It exposes common Ink/Text, selection, shape, Sticky,
Connector, Section, Group, Undo/Redo, text-format and image/crop controls via
the public Runtime Facade. It is a separate HTML toolbar, not the complete
internal Debug UI. Its application-owned text bridge forwards native browser
composition facts through the public Runtime Facade; it uses nlohmann/json as
an application dependency. The checked-in carrier and bridge mirror the Demo's
implementation, and the repository runner checks they remain identical.
Chrome automation verifies synthetic preedit, one result and Enter. Real IME,
candidate windows, physical touch and page teardown during composition remain
manual checks. The provided `loadHostFonts` entry is the
existing fixed application catalog and must run once at startup, before other
resources or document content.

Run `python serve.py --port 4186` from the exported directory and open the
printed loopback URL. The supplied server explicitly serves `.mjs` as
JavaScript and `.wasm` as WebAssembly; on some Windows systems the standard
`python -m http.server` serves `.mjs` as `text/plain`, which browsers reject.
The status becomes READY only after both Runtime and DOM input handlers initialize.
Tool buttons turn blue only after the Runtime accepts the selected tool; Undo,
Redo and other one-shot actions leave the active tool unchanged.

`consumer/control_api.mjs` is an application-side preview wrapper for this
example's matching WASM bridge, not a stable standalone JavaScript SDK. Create
it with `createCanvasControls({module, beforeCommand})`; the callback must
finish any active native text composition. Commands are serialized and reject
if the bridge rejects them. The adjacent `control_api.d.mts` describes this
preview subset for TypeScript. For example, `await controls.selectShape('rectangle')`,
`await controls.selectTool('objectEraser')`, `await controls.setSelectionMode('lasso')`,
`await controls.group()` and `await controls.undo()`. See the module for the
supported names; image, text, document, resource and multi-selection APIs are
not covered by this wrapper. The wrapper schedules rendering after acceptance,
which does not itself mean that a frame was presented.
Object eraser removes a hit stroke; Partial eraser masks only the crossed area.
New connectors use orthogonal routing by default, including connectors created
from a selected object's attachment points. A clear aligned route can still be
rendered as one straight horizontal or vertical segment.
