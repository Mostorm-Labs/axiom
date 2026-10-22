# FC-02 public consumer parity repair

Base: `d54029d91aa8e67c9314b05992e749848629c64f` on `codex/fc02-windows-ime-resume`; origin had no delta when work started. The four audited consumer gaps were repaired without adding canonical semantics or host-side editing state.

## Changes

- MinimalRuntimeFacade now forwards Runtime font catalog and single-font status readback. Its ordinary-consumer test compares catalog, status, formatting and text geometry against the Composition facade, and checks invalid width rejection without canonical mutation.
- The Web JS/WASM text consumer now exposes typed format intent with object identity, explicit client/request receipts, font catalog/status and complete editor format readback. Observed preedit selection is sent through the existing TextInputEvent; it is never synthesized from unobserved data.
- Web width ingress uses nullish defaulting, so zero reaches Runtime unchanged and is rejected consistently.
- Removed concrete `canvas_runtime_facade.hpp` includes from host-facing headers. `runtime/semantic/src` is PRIVATE to Composition and separately PRIVATE only to the white-box FC02 text test which includes its internal protobuf mapping header.

## Verification

- MSVC 19.51 `/W4 /WX`: ordinary-consumer target build and Windows `windows_main.cpp` compile passed.
- CTest list from shape-label editing plus the new consumer test: 234/234 passed. `FC02Text` and `FC02MinimalTextConsumer` were rerun separately and passed.
- Python contract/font checks: 17/17 passed with the resolved locked Semantic SDK environment.
- Isolated Web build using Emscripten 6.0.6, locked Semantic web SDK and locked Skia release package completed with `-Werror`.
- New ordinary Web JS/WASM consumer scenarios: 5/5 passed in Chrome 155. Existing Web text: 7/7; Minimal host: 13/13; Shape/DPR reentry: 6/6 passed. These are synthetic browser tests, not physical IME qualification.
- Compile database inspection: ordinary host/application translation units have no `runtime/semantic/src` include path and no `AXIOM_RUNTIME_QUALIFICATION` definition.
- Native Windows syntax was checked with LLVM Clang 24.0.0git targeting `x86_64-pc-windows-msvc`, using VS 2026 headers and `-Wall -Wextra -Wpedantic -Werror`; it passed. The `clang-cl` driver itself is absent, so the bundled LLVM `clang.exe` was invoked directly with a native MSVC target.
- Existing Windows host PID 262440 remained running from its original executable; it was not replaced. Manual native IME, touch and DPI checks remain `PHYSICAL_PENDING`.

Raw build/test outputs and artifact hashes are in this directory; machine-local result binaries are under `out/fc02-resume`.
## RED/GREEN provenance

- Native baseline RED is retained at `native-audit-red.json` from the frozen A1 audit: the ordinary MinimalRuntimeFacade returned an empty font catalog and no per-font status while the Runtime composition facade returned both.
- Browser baseline RED is retained at `web-consumer-red.json`, run against the pre-repair served build on port 4174; the ordinary JS consumer lacked the font catalog/receipt APIs and format/preedit fields. This is implementation-failure evidence, not an environment failure.
- GREEN is `native-consumer-test-final.log`, `web-consumer-result.json`, plus the regression matrix recorded in `web-regression-summary.json`.
