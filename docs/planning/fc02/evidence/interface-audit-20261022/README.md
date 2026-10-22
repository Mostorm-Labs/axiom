# FC-02 Runtime interface and Host parity audit

Status: source audit with synthetic browser/native consumer probes; not a Gate review or FC-02 completion claim.

Repository: `https://github.com/Mostorm-Labs/axiom.git`
Branch: `codex/fc02-windows-ime-resume`
Audit source HEAD: `ae6653bdfae6cecf6fe6909806dede73886bdb4c`
Web product exercised: `f0ccfd3ae981a3c1fce6caebb47fb95e0bb84e45`
Remote branch delta at audit time: `0 0` after fetch.

## Findings

1. **Windows Minimal facade drops font catalog/status readback.** The Runtime contract and concrete Composition facade expose `readFontCatalog()` and `readFontResourceStatus()`. `MinimalRuntimeFacade` does not override either, so the base defaults return an empty catalog and `nullopt`. Its forwarded `TextEditorSnapshot` still includes font options, which masks this for current Debug UI. A strict MSVC consumer probe against one bound CJK font read catalog size `1` and status present through `Composition.controlFacade()`, but size `0` and status absent through `MinimalRuntimeFacade`. This is a concrete public-consumer interface defect.

2. **Web public JS/WASM bridge cannot perform the existing Runtime formatting API or read it back.** The C++ bridge rejects every intent after `kCompleteComposition`; therefore `kFormat` returns status `4`. The bridge also accepts no object target or `TextFormatPatch`. Its text snapshot omits `format`, `formatObject`, `fonts`, and `mixedFormat`. CDP calls to the actual page's public text consumer reproduced the rejection and absent fields. The Web Debug UI can appear complete because it invokes the in-process RuntimeFacade through a separate C++ path; that does not close the JS consumer surface.

3. **Web drops native preedit-caret offsets.** The common `TextInputEvent` and Runtime geometry support `preeditSelection`. Windows IMM32 maps `GCS_CURSORPOS` into it. Web `compositionupdate` stores only text, and the WASM entry has no preedit-selection arguments. `Input.imeSetComposition` moved native selection from offset 1 to 3 in the same preedit; browser textarea readback followed both offsets, but Runtime caret geometry was byte-for-byte unchanged at the composition end. This is a real platform parity gap for IME preedit caret positioning. It does not affect the three manually confirmed label reentry interactions.

4. **Web width adapter changes invalid input instead of letting Runtime reject it.** The Web adapter uses `fact.width || 320`, converting width `0` to the valid default. A public consumer request after setting width `420` returned an Editing snapshot with width reset to `320`; the equivalent direct Runtime request returned rejected status `4`. This changes user input semantics and makes platform behavior inconsistent.

## Host boundary assessment

The shared Runtime has the canonical editing owner: the text intents/input facts, `TextEditSession`, Semantic/History submission, formatting, Shape label, TextLayout geometry and Runtime presentation state remain behind `RuntimeFacade`/`RuntimeComposition`. In the inspected production host paths, no Host-side Paragraph/Run/delta/Undo construction or direct concrete editing-runtime access was found. Web textarea/DOM selection and Windows IMM32/focus/candidate-window callbacks are platform facts and should remain thin adapters; deleting them would make the integration less complete, not thinner in the useful sense.

There is a lower-priority source-surface cleanup: both Minimal platform wrappers include the concrete `canvas_runtime_facade.hpp` even though their public member signatures use only `RuntimeFacade`; the CMake Composition target also exports `runtime/semantic/src` as a PUBLIC include path. This broadens what consumers can include but was not shown to cause a current correctness failure. Consider a later header-dependency cleanup after fixing the four behavioral findings.

## Reproduction

- Web: `node --experimental-websocket docs/planning/fc02/evidence/interface-audit-20261022/probe-web.mjs http://127.0.0.1:4174/index.html?rev=f0ccfd3a out/fc02-resume/interface-audit/web-result.json`. The browser automation is synthetic and does not claim physical IME/touch acceptance. Captured result is `web-result.json`.
- Windows: `python docs/planning/fc02/evidence/interface-audit-20261022/run-native-probe.py`. It uses an existing configured native build's `compile_commands.json` and Ninja link inputs, MSVC 19.51, locked Skia runtime data and the checked-in font; it does not rebuild SDKs. A build directory can be supplied as argument 1, and a font path as argument 2. Captured output is `native-result.json`.
- Native observed result: `direct_catalog=1`, `wrapped_catalog=0`, `editor_font_options=1`, direct resource status present, wrapped status absent, direct valid format status `2` with bold readback true, and invalid width status `4`. Preedit caret X changed from `109.010002` at offset 1 to `126.669998` at offset 3, confirming the Runtime geometry capability itself works.
- Web observed result and exact geometry/fields/status values: `web-result.json`.
- New Web user-observed scenario: `../web-label-reentry-20261022/human-retest.json`. User confirmed nearest-glyph blinking caret, inside click reposition, and outside click finish/preserve text for Select + triangle + numeric text with Chinese IME selected. Native IME composition/candidate behavior, touch, DPI and broader shape/DPR matrix remain separate manual obligations.

These findings identify remaining implementation work; the audit did not edit production Runtime/Host source and does not self-approve a Gate or claim FC-02 complete.
