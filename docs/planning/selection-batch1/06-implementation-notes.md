# Selection Batch 1 implementation notes

Base: `2fee75f66a7f96f6aac834228ede372a9abda59a` (Section PR #155).
Branch: `codex/selection-unified-batch1`. This is a bounded implementation result, not a Gate certificate.
The six supplied Markdown documents remain byte-identical to the supplied ZIP.

## Ownership and task mapping

1. SelectionSession owns the only formal selection. `applyAcquisition` publishes deduplicated Replace/Add with an expected revision. Old `toggle`, `marquee`, and `cancel` keep their prior meanings. Trailing default PointerModifiers fields flow through the existing ingress.
2. RegionSelectionSession is a leaf Interaction transient session; it holds confirmed logical-view points, the Down combine mode and context. `region_selection_geometry.cpp` is linked only into the existing editor composition, never as a Scene dependency of the leaf library.
3. CanvasEditingRuntime composes current Scene query, precise shape geometry, Sticky-child/Group reduction and a single Up publication. Actual RuntimeComposition.submit ingress is tested. Blank Down remains pending; explicit rectangle/freeform starts over objects; point mode retains handle/port ownership. Select/Hand retain formal selection. Camera/surface/metrics/selection/binding fences reject obsolete results. The existing coordinator owns second-finger navigation; its 250ms and Ink slop policies are unchanged.
4. Existing RuntimeFacade / CanvasControlService carries mode intents and readback. Debug UI exposes Select, Rectangle select, Freeform lasso, Hand. Actual Minimal Windows WPARAM / POINTER_INFO and actual Web DOM/WASM carry Shift. All selected roots get read-only outlines; candidate/region geometry is transient. Unsupported primary-only edits are guarded both in Runtime and Facade; existing safe all-root Section/deletion paths remain. Fit Selection unions all selected bounds. Web host cancellation retires exact ingress keys before dropping host capture.

The supplied plan's proposed filenames were adapted to actual active consumers: `apps/minimal_runtime_host` and `apps/axiom_canvas_demo/platform/web`; the older Playground is not the consumer under test. The old `codex/g4-6-d-debug-ui-selection` branch contains only candidate-list `marquee`, no region session or geometric lasso to transplant. It was not merged.

## Engineering parameters

- Drag threshold: 4 logical view pixels; a displacement of 4 is active. Under-threshold explicit regions preserve selection. Pending point-mode blank clicks clear on Up (Add preserves).
- Confirmed freeform capacity: 4096 points; overflow cancels without publishing a partial path. Predicted samples never contribute final members.
- Geometry epsilon: 1e-9; singular affine determinant cutoff: 1e-12. Self-intersection uses even-odd, with boundary intersection included. Concavity is never replaced by its AABB.
- IDs/versions of tools and canonical schema/operations are unchanged. No SDK lock, historical descriptor lock, frozen expected value or global Aegis registry was changed.

## Coverage and intentional limits

- Rectangle / Ellipse / Triangle: independent geometry fixtures plus real production-input end-to-end Native and Chrome/WASM tests.
- Image / Sticky: existing affine rectangular frames have precise geometry unit tests. Complete Image-resource and Sticky RichText-child region end-to-end profiles are NOT_RUN; no all-object acceptance is claimed.
- Group: existing root/scope reduction is reused; full container-region coverage is deferred. Section, standalone RichText, Connector, VectorPath, VectorStroke, DabStroke and erased geometry have no precise region provider in this batch.
- An unsupported candidate yields Partial, preserves the formal selection and reports unsupported kinds. Failed/stale queries do not publish an empty selection. Supported candidates may be previewed but a partial query is not committed.
- Multi-root transforms, batch appearance/font/order/lock editing, Group/Ungroup and new Snap/Alignment are not implemented. Those primary-only selected controls are unavailable; explicit object-addressed APIs retain their existing purpose.
- Real mouse/pen/touch physical ergonomics, hardware navigation, physical IME, other platforms, G5 performance/fault qualification and the full FC08 302-variant product runner are NOT_RUN here.

## Executed verification

- Native Release /MT /WX: 47 registered relevant CTests passed, zero skipped. Discovery and exact target/CTest commands are in the result artifacts. Subsequent unit/input assertions strengthen the same targets.
- New input integration fixture exercises real RuntimeComposition / OperationEngine / Scene / Selection / History, not a substitute adapter. It checks canonical record identity, generation/outcome metadata, History size/cursor flags, and actual redo content across pure selection.
- Actual HWND mouse messages prove Shift-at-Down survives release before Move/Up. Existing Windows touch, text, Shape label, Image/Crop, Connector, Brush retry and Section drop/follow/detach regressions passed. Synthetic observations are not physical acceptance.
- Actual Chrome/WASM: 19 checks passed through CDP-delivered DOM mouse events, plus an independent screenshot pixel oracle and DPR 2. Node transports events and bytes; it does not emulate Runtime.
- 18 Python contract/font/descriptor checks passed with emitted SDK environment imported. Runtime boundary check passed for 102 contract files.

Preserved RED includes missing API compilation (not behavioral RED), four real-input failures, multi-primary mutation/handles, HWND Shift propagation, Fit-to-primary, selected Connector readback, old Select UI transport compatibility, Web post-cancel generation rejection and a primary-only label-affordance pixel failure. The initial CDP `button:none` drag fixture and invalid Shape-through-Section-select fixture were corrected as test setup issues; raw failures are retained. Frozen oracles were untouched.

## Reproduction

Resolve locked SDK facts with `tools/setup_build_environment.py --core --semantic --facts-output out/selection-batch1/environment.json --status`, then `--semantic --offline --status`. Cross-resolve `--semantic --target web-wasm32` separately. Configure with the emitted consumer helper arguments and locked R1 Skia release SDK, qualification hooks OFF. Exact CMake arrays and logs are retained under the result directory.

Native: build the targets in `native-regression-commands.json`, discover CTests, then run `ctest --test-dir out/selection-batch1/native --tests-from-file <absolute native-ctest-names.txt> --no-tests=error --output-on-failure --timeout 90`.

Web: build `axiom_canvas_demo_web`, then run `node apps/axiom_canvas_demo/tests/web_selection_batch1.mjs --repo . --chrome <verified Chrome executable> --python <Python with Pillow> --git <Git executable> --output <new output directory>`.

Manual: F12 -> Control -> create basic Shapes; Select blank drag; Shift-at-Down append; Rectangle select / Freeform lasso may start over objects; cancel or switch Hand and return Select. Selection must not add Operations or consume Undo/Redo. Unsupported candidate kinds preserve selection and display a message.
