# SDK-01 external consumer — verification candidate

This standalone CMake project links only the provided installed V1 header and library. It does not add the Axiom repository or link internal targets. The C smoke invokes real V1 symbols and checks ABI, rejection of invalid handles/version without publishing an output, two-runtime lifecycle and isolation after destroying one runtime. The C++ program tests C-header linkage. It intentionally contains no mock implementation. It does **not** cover an edit, Id128, view/document, threading, multi-selection or platform presentation; those require additional independently bound cases.

After installing a real SDK to a separate directory, copy this directory to a fresh location outside the repository and configure with:

```text
cmake -S <external-consumer-copy> -B <external-build> -DAXIOM_SDK_INCLUDE_DIR=<installed-header-directory> -DAXIOM_SDK_LIBRARY=<installed-library> [-DAXIOM_SDK_SHARED=ON]
cmake --build <external-build> --config Release
ctest --test-dir <external-build> -C Release -R "^SDK01External(CSmoke|CppLink)$" --output-on-failure --no-tests=error
```

Both named tests must actually run; a configuration or link failure, missing SDK, missing runtime DLL, skip, or zero tests is not PASS. For a shared SDK the operating system must find its real distributed DLL at execution. `requested_backend=Auto` is used; if the SDK's supported environment cannot initialize an available backend, the run is a platform/environment blocker and must not be reported as passing. All assumptions and exact error expectations require P20 review and acceptance before being frozen as P32 completion criteria.
