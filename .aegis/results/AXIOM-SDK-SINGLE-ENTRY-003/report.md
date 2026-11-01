# Single public Composition entry — scoped result

The source bytes under test were dirty; manifest.json hashes them and the enclosing commit pins them. The previous wrapper was first detected by the new installed-consumer check (RED), then removed from source, build and installation. Consumers use the existing RuntimeComposition owner. Historical evidence is retained. This does not imply all host internals are hidden or both SDKs complete.

- Debug UI ON Windows build, real host, targeted CTest: 10/10 passed, zero skipped (see raw/sdk-legacy-removal-tests.log). Debug UI OFF Windows build and adapter test: 1/1 passed.
- Installed Composition consumer: install, external configure/build and one test all exited 0; the negative installation check now passes. Existing external consumer: three tests passed; incompatible /MD configure rejected as expected. Installed Windows SDK external HWND sample: install, configure, build and 1/1 test passed. Its input is synthetic and provider Present success is not a physical scanout observation. Complete command vectors and outputs are in the JSON raw evidence.
- Emscripten Web demo build exited 0; actual Chrome selection suite: 44 assertions, zero failures. This tests the existing demo, not a repository-external Web SDK.

The Windows Minimal Host still uses the private core for native input, IME, fonts and diagnostics. Web has a transitional private platform bridge and no installed independent SDK/browser consumer. Public unkeyed input still has the previously recorded stale-contact generation ambiguity; no claim of resolution is made. Font/resource loading in an external sample, real IME/touch, physical scanout and D07 Protobuf/Abseil OOM remain unverified or unresolved. This is not SDK, release, Gate or PHYS PASS.
