# Group scope outline during member editing

Source base: `7bae582ec205616ff5c9af03b1c27d5c13d7e613`. User observed that a Group appeared to disappear on double-click / Enter group and asked to keep the Group range visible during member movement, with the range reflecting the new positions after release.

Root cause: Group scope remains in SelectionSession and membership remains canonical, but rendering switches to the selected member's chrome. It did not draw a separate Group scope outline; Enter group with no selected child also discarded the selection overlay.

The Runtime now appends a read-only Group scope outline from the existing Scene subtree bounds, with no new semantic object, membership store, or hit target. When an existing transform preview moves a member, scope bounds are recomputed from actual descendant bounds and transient world overrides, without changing canonical state. The committed Scene supplies bounds after release; Undo/Redo also updates the outline. Empty groups do not gain invented bounds. The same shared render path is consumed by Windows and Web.

Red/green: `SelectionGroupScope.KeepsAndUpdatesGroupOutlineDuringChildDrag` initially failed with zero read-only outlines (expected one). It now covers explicit Enter group with no selected child, double-click entry, actual pointer Move, no canonical generation change during preview, final group bounds, group membership and Undo/Redo; it passes.

Validation:
- Windows host / selected test targets and locked Emscripten web target built successfully.
- `ctest --test-dir <absolute out/fc04-mse-p32/build> -R '^(FC01ASelectionInputIntegration|FC08SectionConsumer|FC02Text|SelectionBatch3Summary|SelectionBatch3Ui)$' --output-on-failure --no-tests=error --output-junit <absolute out/fc04-mse-p32/group-outline-regression.xml>` — exit 0, 5/5; raw log and XML are in raw.zip.
- Real Chrome/WASM with synthetic CDP input: `chrome-group-outline.mjs` — exit 0, 14 assertions. Entry, during-drag and post-release screenshots are preserved and visually reviewed; see raw.zip. This does not establish physical input or PHYS pass.

Tested source hashes and raw artifact hashes are in facts.json. Final physical user retest remains pending. Previous MSE-20/23/26 gaps and D07 Protobuf/Abseil OOM blocker remain unchanged; no Gate or release status is asserted.
