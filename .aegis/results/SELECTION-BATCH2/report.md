# Selection Batch 2 — implementation receipt

Implemented A–D on the actual Batch1 branch subject; Batch1 prerequisites were PRESENT, not inferred from main. Legacy cancel remains explicit clear; region cancellation stays transient. No main merge or Gate/DEV/PHYS/release claim.

- Base: `5daf623eba9a943706d813f8e197e00a9f6a8b2e` (Batch1 receipt, preserved branch).
- Tested source: `6ab8d388e425a8c28a136dadf661ac7ad31f48aa`; clean before both final builds.
- Branch: `codex/selection-hierarchy-coverage-batch2`.
- Receipt: the evidence-only descendant containing this directory; final HEAD is reported separately. No PR/push created by this task.

## What changed and who owns it

A: one pure SelectionSession root projection handles scoped Group/Sticky identity, deduplication and ancestor pruning. B: Scene defers Section interior backgrounds behind eligible content; point consumers filter active Group scope. C: Interaction queries coherent Scene candidates and delegates visible paths/strokes, published routes, cropped Image and existing TextLayout to Render; geometry is never replaced by broad-phase AABB success. Scene bounds include acute miters. D: actual consumer and no-mutation regressions cover platform ingress and existing Section/Crop/creation behavior. Created sibling keys retain all seed bits; no historical key migration or new topology model. Exact changed paths, hashes and ownership are in [source-files.json](source-files.json).

## Actual final execution

| Execution | Result | Evidence |
|---|---|---|
| Locked Native SDK + Skia consumer, MSVC19.51 x64 Release /MT /WX | configure/build exit0 | commands.json; raw/release-native-* |
| Registered scoped Native CTests (real HWND, synthetic input) |77/77 PASS,0 skipped, exit0 | raw/release-native-regression.xml |
| Actual Chrome155/WASM/WebGL2 Minimal Host, headless SwiftShader |28/28 checks PASS, exit0 | raw/browser-004/results.json; screenshots/CDP |
| Python descriptor/semantic/host/font checks with emitted AXIOM_PROTOC |18 PASS,0 skipped, exit0 | raw/release-python-0.log |
| Runtime boundary checker |102 contract files PASS, exit0 | raw/release-python-1.log |

Commands with exact arguments/exit codes are in [commands.json](commands.json); emitted environment/toolchain identities and selected test names/discovery are preserved under raw/. Build directories were reused from Batch1 but artifacts were rebuilt from the clean tested source; old committed evidence was not modified. This is real browser execution, not Node-only simulation. All browser input facts and Native HWND fixtures remain synthetic, not PHYS acceptance.

## Geometry scope and invariants

The named per-kind oracle/profile table is in [implementation notes](../../../docs/planning/selection-batch2/06-implementation-notes.md). Covered: Rectangle/Ellipse/Triangle, VectorPath curves/holes, canonical vector Brush outline and legacy segments, simple solid Dabs, published Ready Connector body/arrowhead, Image crop/Fit frame, RichText typographic fragments, Sticky child identity, Group scope/promotion and Section boundary/background. Shape label fragments use the published scale/clip, not new text measurement. Filled/swept erase masks match the current painter. Textured/chalk/membrane/unsupported-package Dabs, pending/missing routes and unavailable/partial text metrics remain explicitly Unsupported/Partial, preserving prior formal selection. Text is typographic line coverage, not glyph-alpha precision.

Tests assert unchanged semantic generation, last Operation/object counts, History size/cursor availability and redo state for selection-only/cancel paths; preview is transient. Actual Chrome independently checks operation/object invariance. No host owns semantic selection or canonical mutation.

## Failures retained and review

One independent read-only whole-branch review found3 Important and1 Minor. The Important partial-font, unfilled-label and miter-bounds findings have behavioral RED→GREEN coverage. No second reviewer was used. A styled-Ellipse tolerance-union regression was caught by actual Chrome and a literal coordinate oracle; exact overlap is now checked before the contact ring. Real Native/Chrome ingress also caught old creation-key bit loss, now fixed in the existing creation owner.

Supplemental failures were not hidden: the initial Native matrix hit a locked-SDK header classification error, then a legacy missing-route arrow fixture; its expected green pixel remains unchanged. Expanded77-test execution exposed miter port-axis drift and an old Snap fixture's unconditional free-endpoint access. The former is fixed; the latter now checks FC05 attachment identity using independent reference candidates while retaining coordinate/guide/camera/commit assertions. Isolated old-unit probe commands and failures are preserved, not represented as a full historical baseline rebuild. Unknown-target setup attempts are setup failures, not zero-test passes. Earlier invalid fixture/tool/identity setup attempts are distinguished in the ledger/notes. Final complete77-test and28-check results above supersede these attempts for the tested source, without deleting them.

[Review dispositions](review-disposition.json) and the [complete decision ledger](implementation-ledger.md) preserve every ruling and deferred item. Minor deferred to G5: repeated growing path unions per Move may scale poorly; no demonstrated hang or performance claim. Physical input/IME, other platforms, full multi-edit, Group expansion, G5/fault qualification and full302 remain NOT_RUN. No remaining failed final automated check; unsupported profiles remain explicit limitations.

## Reproduction and integrity

Use locked emitted Semantic/Skia paths, run the saved release Native/Web scripts under the discovered VS2026/Emscripten environments, then the browser command in commands.json and Python checks. `--no-tests=error`, exact-name discovery and nonempty target matching are enforced. Do not assume these local absolute SDK/toolchain paths on another machine. Raw emitter whitespace is retained byte-for-byte; code whitespace validation excludes only raw artifacts. [build-artifacts.json](build-artifacts.json) binds local exe/JS/WASM hashes. manifest.json hashes all evidence payloads except itself. The receipt commit must be an evidence-only descendant and every source-file hash must match the tested subject.
