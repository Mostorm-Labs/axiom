# Axiom Windows consumer SDK

Versioning uses `x.y.z`: `z` for small improvements and fixes; `y` for small feature
iterations (reset `z`); `x` for large feature iterations (reset `y` and `z`). A
release with multiple change levels uses the highest one. API/ABI and data-format
compatibility are declared separately in the contracts and release notes.


Windows x64. `axiom.dll` privately links the locked Runtime, Semantic and Skia
implementations. Apps link only the small `axiom.lib` import stub. Headers and
helper JSON code compile inside the application's own CRT; the DLL uses /MT.
No Semantic/Skia CMake package or component archive is needed by consumers.
The shipped demo and example default to /MT and need no VC redistributable;
`-DAXIOM_EXAMPLE_CRT=MD` independently exercises a /MD caller against the same DLL.

Activate an x64 MSVC developer shell, then from this extracted SDK:

```powershell
cmake -S example -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAxiomSDK_DIR="$PWD/lib/cmake/AxiomSDK"
cmake --build build
ctest --test-dir build --output-on-failure
./build/axiom_example.exe
```

`Axiom::SDK` is an imported DLL target, and `axiom_deploy(your_app)` copies the DLL,
ICU data and font assets next to the executable. The app's working directory may
be unrelated. Keep `licenses/` when redistributing. Imported fonts retain their
individual licenses under `share/axiom/fonts`. An explicit absolute assetRoot may
override DLL-relative font discovery; ICU data must accompany the executable.
This 0.x release is not a cross-version C++ ABI promise.

```cpp
#include <axiom/axiom.hpp>
axiom::Runtime runtime({parentHwnd}); // owns a child canvas HWND, same UI thread
runtime.rectangle();
runtime.permission(1); // explicit local-single-user permission policy
// After the user selects two or more objects: runtime.group().
runtime.frame();       // service in the normal message loop
```

The parent app owns its message loop, layout, storage and network. Native canvas
messages are collected by the DLL using the existing Win32/IMM32 adapters.
Resize via `runtime.resize(logicalWidth, logicalHeight, dpiScale)` on parent resize.
Double-click and native IME share the same Runtime text session as public commands.
Format controls complete composition and retain editing; ordinary commands finish
editing. A BUSY result means pump real messages/frames and retry, not success.

Controls cover tools/two erasers, presets/ink, selection/appearance/order, camera,
Undo/Redo, Group/Section, RichText/fonts, Image/replace/Crop, Connector/Sticky,
multi-property projection/writes, snapshots, remote operation application and
resource tokens/receipts/publication. Named C++ methods use the same v1 command
vocabulary as JS. Advanced `invoke(command, args, bytes)` is explicitly decoded;
it is not arbitrary canonical patching. Id128 is 32 lowercase hex characters;
uint64 JSON values are decimal strings. See `axiom.hpp` and the shipped example.

For Product Shell / Axis integration, use `subscribe`, `unsubscribe` and
`dispatchEvents` (also serviced by `frame`). Operation subscription attaches a
bounded volatile custody queue. Read retained packets with `nextOperation`, copy
their exact IDs/bytes into the data owner's queue, then `releaseOperations` through
the accepted prefix. `reportOperationStatus` returns actual external durability /
sync facts through the existing Runtime status projection. Listener success is
not custody release or a saved file. A full publication queue rejects local writes;
controlled close stays BUSY until it is drained. `resourceManifest` and
`readResourceBytes` expose current metadata and validated bytes without Semantic
headers. `applyRemoteBatch` returns independent per-packet results, without local
echo or cross-operation atomicity. See the repository's
`docs/planning/sdk/shell-axis-integration.md` for the complete lifecycle and examples.

`api.h` is the C/POD boundary for LoadLibrary and other language bindings.
Check `axiom_abi_version`, size/version of config, and every transport status;
then inspect JSON `ok/code` and actual command receipts. Release every response
buffer through `axiom_buffer_release`; it can outlive its Runtime. Custom input
must register its own high-range source and retain the full returned contact
identity for every later batch. Delayed or foreign generations reject.

All Runtime calls and destruction run on the creating HWND thread. Callbacks are
borrowed and synchronous; copy operation bytes before returning and never reenter
the Runtime or throw. The C++ wrapper is non-copyable/non-movable to keep callback
addresses stable. Keep the DLL loaded until every handle/buffer is released.

Call `close()` until true before destroying the parent. It requests actual native
composition completion and rejects/pends while work is active. `abandon()` and the
C++ destructor are forced teardown, with no IME or persistence guarantee. Operation
callbacks mean in-memory commits, not a saved file. A transport/allocation failure
after invocation does not prove rollback; reconcile the resulting state/receipts.
Snapshots do not restore Undo history and do not embed resource blobs; persist and
republish matching resources using their manifest tokens. The known D07
Protobuf/Abseil injected-OOM hang remains unresolved. Physical IME/touch/DPI
acceptance remains separate from synthetic/native automated regression.

Image export (`imageExport: 1`, use matching SDK binaries) is a read-only
owner-thread query over committed content:

```cpp
auto image = runtime.export_image({{"format", "jpeg"}, {"scale", 2}});
// image.bytes is application-owned; image.value["value"] holds
// mimeType, width, height, world bounds and exact decimal-string generation.
```

Default export covers complete visible content with 16 output pixels of padding,
including objects outside the current camera. It excludes editor chrome and
uncommitted previews. `png`, `jpg` and `jpeg` are supported; JPEG is opaque. Options
match the Web `exportImage` contract, including bounds, scale, quality, background,
includePattern and optional generation fence. Limits are 8192 per axis and
16,777,216 pixels; oversized queries reject before raster allocation. Storage and
filenames belong to the caller. Response buffers retain the existing C ABI
copy/release ownership; no new DLL symbol or rendering handle is exposed. Automated Windows DLL and external-consumer verification runs in release CI;
physical device qualification remains separate.

Table/1 shares the Session `table` command vocabulary with Web (see the Web README).
The caller-side wrapper provides `table(Json)` and `readTable(id)`; command calls
must include the generation observed from readback. No new DLL export, internal
object handle or dependency header is required. Windows device/IME acceptance is
separate from the local Web and native tests.


Sketch appearance v1 uses the shared Session `appearance` vocabulary. The caller-side
wrapper has `appearanceCommand(Json)` and `readAppearance(defaults=false)`; no new DLL export,
C++ Runtime object or rendering handle crosses the ABI. Actions are read/readDefaults,
patch/defaults, begin/preview/commit/cancel. Forward the full eight-element decimal
stamp array and explicit eligible IDs for patch/begin; preserve both stamp and decimal
serial in preview/commit/cancel tokens. Readbacks/buffers follow the existing owned JSON
and C buffer rules. Rejections do not imply a commit. See the Web README for parameters,
applicability and seed/session lifetime. Web/native shared-path checks do not qualify
Windows DLL packaging or native HWND/IME behavior; those require a Windows build.


### Sketch v1 extended object capability

`readBuildInfo().features.sketchExtended === 1` / shared Session feature `sketchExtended: 1` extends appearance to VectorPath, Table grids and Section document boundaries. `strokeEligible`/`strokeExcluded` and `fillEligible`/`fillExcluded` report per-parameter applicability. Table grid paint remains exclusively in `table.setGridStyle`; generic stroke/fill patches reject when all selected targets are inapplicable. Group is not recursive.

`createVectorPath({commands, fillRule?, x?, y?, objectId?})` returns `{objectId, generation}` after shared owner application; omitted identity is allocated by Runtime. Commands are owned value tuples (`move`/`line`: `point`, `quad`: `control`/`point`, `cubic`: `control1`/`control2`/`point`, `close`), at most 4096, finite coordinates with absolute value ≤1e7, exact two-number points. Fill rule is `nonzero` or `evenodd`. `readVectorPath(objectId?)` returns owned canonical commands, transform, appearance and generation, never a mutable path or private engine handle. Windows caller-side equivalents take/return owned Json through the unchanged C transport ABI.

New paths always retain versioned appearance metadata even with sketch disabled; canonical curve geometry/fill rule are preserved. Open outlines are not implicitly closed. Snapshot reader capability `extended-sketch-v1` uses the additive internal bit 256 (current full profile 511); older profile 255 rejects these object extensions, including disabled metadata. SDK version 0.1.0 and styleVersion 1 are distinct. Preview/patch/error/thread/lifetime rules remain those of the shared appearance owner. This local development verification does not establish Windows DLL ABI qualification.

Windows keeps the existing `appearance(Json)` creation-palette control. Versioned object patch/session requests use `appearanceCommand(Json)` with `readAppearance(bool defaults=false)`, avoiding an overload that differs only by return type.


### Rounded corners / handwriting resources

Appearance values/patches add `cornerRadius` in object-local units [0,16384], independent of roughness. Radius0 clears the canonical property. Rectangle/triangle and Connector support it; `radiusEligible/radiusExcluded` report applicability, with mixed values only across eligible roots. The requested radius is clamped geometrically to prevent overlapping fillets. Existing preview/token/History semantics apply, and creation defaults materialize only on supported object kinds. Connector route/endpoints stay canonical; only its displayed interior bends round within obstacle constraints. Snapshot requires `rounded-corners-v1` (bit512).

The application font catalog adds Excalifont and Xiaolai v3.126 under OFL1.1. Font selection still uses owned resource IDs through existing `text.format({font})` and Table cell format. Excalifont carries explicit one-hop Xiaolai fallback in font metadata schema/layout version2; bytes/hashes/resource revisions remain in the existing resource owner. Fallback is shared C++ shaping/rendering, never browser/OS font selection or canonical run rewriting. Missing/corrupt bytes or unsupported metadata reject/diagnose explicitly. Snapshot requires `font-fallback-v2` (bit1024); full internal reader profile2047. Old v1 font resources and ordinary API lifetime/thread rules are unchanged.

The Demo has tools, appearance properties, text/object properties, then board tools. Create/edit/select text to choose the font; the selector is not an implicit mutation of unrelated objects or global settings. Assets keep original upstream names and copyright; UI display labels can be separate. Renaming/modifying a font does not remove its OFL obligations or transfer upstream design ownership. SDK0.1.0 remains an unpublished candidate; Windows DLL ABI requires separate Windows evidence.

Ink v2 caller-side wrapper adds `setBrushConfig(Json)` / `readBrushConfig()` using
the existing JSON command transport. Family values1..6 and explicit pressure source
1=simulated(default),2=device are shared with Web. No internal C++ BrushSession,
Skia object, allocation or STL ownership crosses the DLL ABI. Configuration is
copied and frozen per new session; invalid versions/enums/numbers reject. This
Mac/Web execution does not qualify a Windows DLL or a physical pressure device.
