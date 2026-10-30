# Axiom Windows consumer SDK preview

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
This prerelease is not a cross-version C++ ABI promise.

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
