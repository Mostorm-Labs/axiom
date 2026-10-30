# Standalone JavaScript SDK v1 implementation result

The user prioritized the standalone JS SDK; Windows external interactive sample
work is paused. The implementation uses the existing public Composition,
Facade, History, semantic codec and Web adapter. It adds no parallel canonical
store or transaction system. Native ABI, schema, SDK locks and historical
Authority/evidence remain unchanged.

`sdk/web/` contains the ES module factory, declarations, modular WASM bridge,
input/IME adapters, licenses and a JS-only example. A caller supplies an empty
mount rather than a prescribed page or `_sdk_*` symbols. Runtime instances have
separate memories and documents; full Id128 and decimal uint64 values are
preserved. Foreign stamps/tokens reject, selected-property requests fence their
captured scope, and actual receipts determine command completion. Binary outputs
and local-operation deliveries are JS-owned copies.

Verification actually performed:

- `cmake --build out/preview-fresh-web --target axiom_js_runtime --parallel 6`
  under Emscripten 6.0.6 — exit 0. The new bridge uses
  `-Wall -Wextra -Wpedantic -Werror -fexceptions` and links `-fexceptions`, matching
  the locked Semantic SDK's published exception model.
- `cmake --install out/preview-fresh-web --prefix out/sdk-js-v1/install` — exit 0.
- `node verification/sdk_js/verify_browser.mjs out/sdk-js-v1/install/js <Chrome>
  out/sdk-js-v1/installed-example-green.json` — exit 0. The runner copies only
  distributable JS/engine/type/license files to a fresh outside-repository
  consumer; it does not compile or link a C++ application. Chrome version,
  exact invocation, file hashes and per-assertion observations are in
  `installed-example-green.json`. This includes actual rendered pixels, separate
  instances, History, native-carrier synthetic text, large checkpoint revision,
  restore/remote dedup/no echo, multiselection/stale/foreign scope, Group/Section,
  Image/Crop, Connector, resource validation, context loss/restore and disposal.
  It also opens the shipped example and uses Chrome CDP mouse input to select
  Rectangle, check its button indication and observe real rectangle pixels.
- `npx --yes --ignore-scripts --package typescript@5.9.3 -- tsc --noEmit --strict
  --target es2022 --module nodenext --moduleResolution nodenext
  verification/sdk_js/api_types_test.mts` — exit 0, positive and negative consumers.
- Node public error-contract, pointer collector and legacy control-wrapper tests
  — exit 0. Release packaging unit tests — 4 passed, exit 0, including the new
  required-JS fifth-asset check.
- The existing installed C++/WASM external consumer was rebuilt outside the
  repository; both browser drivers passed. Debug-configuration and altered-Skia
  manifest negative controls rejected as expected. Exact commands/results are
  in `legacy-regression.json`; its `PASS_SCOPED` is not physical acceptance.

`invalid-range-red.json` preserves the real WASM abort that occurred when the
bridge initially used Emscripten's default disabled exception catching. Enabling
the matching exception model makes oversized ranges/enums return errors and
allows subsequent commands. `browser-final.json` also preserves a failed new
fixture that tried generic appearance editing on Sticky, which the current
native API does not support. That rejected capability was retained; the stale
single-object style test now targets a real Shape. Neither failure was relabeled.

The release workflow now builds the JS engine, checks public TypeScript types,
tests an independent JS consumer and tests it again after package extraction.
The future `axiom-sdk-js-preview.zip` contains ready-to-use JS/WASM without C++
libraries/toolchains. The existing GitHub release has not been changed or
replaced. Local tests do not establish a hosted CI run for this new revision.

The public API v1 surface is materialized and exercised, but production/stable
release readiness, physical IME/touch, other browsers, the remaining MSE-20/23/26
provider/fault obligations and the known D07 Protobuf/Abseil allocation hang are
not claimed closed. Persistence/network delivery and durable-close coordination
remain owned by the embedding application. This result does not assert a Gate.
