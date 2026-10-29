# Initial C++ Runtime installation profile

This is an initial Windows x64, Release, MSVC `/MT` static consumer profile,
not a stable binary ABI or a complete product SDK. Use the exact MSVC version
recorded by the installed CMake package, the locked prebuilt Semantic runtime
and the locked prebuilt Windows Skia SDK. The package validates their manifests;
their contents must still be verified by the repository's SDK resolvers.

Set `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` before `find_package(AxiomRuntime
CONFIG REQUIRED)`, provide the Semantic SDK in `CMAKE_PREFIX_PATH` and point
`CanvasSkia_DIR` to the locked Skia SDK's `lib/cmake/CanvasSkia`. Link
`Axiom::Runtime` and compile as C++20. Debug, other CRTs, other compilers and
other platforms are rejected rather than assumed ABI-compatible.
Call `axiom_runtime_copy_assets(your_target)` after creating the executable to
copy the installed ICU data alongside it. Other application fonts and resources
are not included by this initial profile.

`canvas::runtime::Runtime` is an owner-thread object. Its borrowed facade is
valid while the Runtime object exists and delegates to its single composition.
The owner thread opens, binds, pumps and closes it; stop using the borrowed
facade before destroying the owner. Register or unregister a source and submit
the existing `PlatformPointerBatch` as supplied by the platform. The Runtime
retains source, pointer and generation ownership; it interprets capabilities,
history and coordinate facts in the existing input ingress. The initial surface
entry remains limited; native surface resources, frame presentation,
installable Web bindings and complete consumer contracts remain separate work.

`run_external.py` installs to a fresh directory outside the repository,
compiles and runs the real Section edit/Undo consumer, a two-Sticky mixed-stroke
consumer, and a two-Shape mixed-stroke consumer created and selected using
pointer facts. It checks one Undo/Redo for both and rejects an incompatible CRT.
All consumers use the installed facade and existing Runtime owner. It uses
locally resolved SDKs and does not build Protobuf, Abseil or Skia. The negative
check is not a product acceptance test.
