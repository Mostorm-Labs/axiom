# Local Chrome Web preparation — 2026-10-19

source_revision: `bd3eaf8af26bf710de7a0458a5fc9920d71b5589`. Project code unchanged. Build inputs and all served artifact hashes are in
`result.json`; exact configure arguments are in `configure-command.json`.

Windows user feedback: “经测试，各项功能正常”. The full post-test Windows logs and the
user statement are preserved in `windows-user-observation/`. No detailed per-item observations
were supplied; historical matrices and Gate/PHYS statuses were not changed.

## Build and automated checks

- Emscripten 6.0.6 / LLVM 22.1.8, locked Semantic web-wasm32 and Skia web-wasm-webgl2 release SDKs.
- `python tools/setup_build_environment.py --core --semantic --target web-wasm32 --facts-output out/p32-web/build-environment.json --status`
- `python tools/setup_build_environment.py --semantic --target web-wasm32 --offline --facts-output out/p32-web/build-environment-offline.json --status`
- `python tools/skia/fetch.py --target web-wasm-webgl2 --variant release --lock r1-full-skia-sdk.lock.json --profile tools/skia/profiles/r1-full-v1.json --summary-file out/p32-web/skia-fetch.json`
- `python .deps/emsdk/emsdk.py install 6.0.6` and `activate 6.0.6`; compiler installed, SDK libraries not rebuilt.
- `python out/p32-web/check_sources.py`: 8 source contract cases PASS.
- `cmd.exe /d /c out\p32-web\native_checks.cmd`: G3WebSurfaceHost PASS (1 CTest).
- `cmd.exe /d /c out\p32-web\web_build.cmd --local-baseline-filter`: two Web targets linked.
- `python out/p32-web/smoke.py`: Chrome CLI startup status PASS; no physical input inferred.
- HTTP probes: 7 exact artifact byte/hash matches, WASM MIME/magic verified, Cache-Control no-store.

## Strict build failures and local test exceptions

`strict-build-FAIL.log` preserves the unfiltered -Werror failure at unchanged
`runtime/foundation/include/canvas/foundation/stable_order_key.hpp:29` (-Wsign-compare).
`header-filter-build-FAIL.log` preserves the subsequent unchanged
`runtime/semantic/src/apply_plan.cpp:22` (-Wdangling-else) failure.
The local functional-test build isolates the former exact header using diagnostic push/pop and
applies -Wno-error=dangling-else to only the latter exact source through a deferred CMake include.
The project warning policy and sources are unchanged. Strict build result remains FAIL;
the local test consumer does not establish a clean production build.
The reference bridge also emits existing deprecation warnings (see build.log).

## Browser entries and remaining observations

Interactive entry: http://127.0.0.1:8765/ink_playground/ — existing Ink Playground with Debug UI button.
P32 RuntimeComposition reference: http://127.0.0.1:8765/axiom_canvas_demo/ — small canvas without the
full Windows Minimal Host controls. These entry points are distinguished explicitly.
Browser executable, version, PID, profile and server PID are recorded in `browser-process.json`.
The loopback server is a tracked foreground CLI session. No computer use was performed.
All Web manual items remain PHYSICAL_PENDING in `manual-matrix.md`, including any tool whose
availability in this Web UI has not been observed. Original AutoIntent semantics remain frozen:
pending second contact inside the 250 ms window enters navigation; later contact enters another Ink;
the existing first-contact Ink slop rule is preserved.
No official Gate, Control Review, true-device acceptance or PHYS PASS is claimed.
