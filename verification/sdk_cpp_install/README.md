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

`canvas::runtime::Runtime` is an owner-thread object. Its borrowed facade is
valid while the Runtime object exists and delegates to its single composition.
The owner thread opens, binds, pumps and closes it; stop using the borrowed
facade before destroying the owner. The initial surface entry is intentionally
limited; native surface/input, resources, frame presentation, installable Web
bindings and complete consumer-facing contracts remain separate work.

`run_external.py` installs to a fresh directory outside the repository,
compiles and runs the real Section edit/Undo consumer, then checks that an
incompatible CRT is rejected. It uses locally resolved SDKs and does not build
Protobuf, Abseil or Skia. The negative check is not a product acceptance test.
