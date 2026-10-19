# GT-DUI-M1 implementation plan projection

Goal: implement M1-01 through M1-07, preserving the approved input projection.
Executor: Codex. Perform repository/package preflight, then T1-T5 with a failing
behavior test, observed RED, minimal implementation, GREEN and focused commit.
Ordinary API/file reconciliation is execution work, not a reason to stop. New
control files/tests/wrapper below are authorized creations, not missing inputs.

## Exact file and responsibility map
Create runtime/foundation/include/canvas/runtime/canvas_control_types.hpp for
renderer-neutral values; extend runtime/foundation/include/canvas/runtime/runtime_facade.hpp.
Create runtime/control/CMakeLists.txt and:
- include/canvas/control/canvas_control_service.hpp; src/canvas_control_service.cpp
- include/canvas/control/canvas_control_port.hpp
- include/canvas/control/build_info.hpp; src/build_info.cpp
Modify root CMakeLists.txt and add cmake/AxiomBuildInfo.cmake.
Extend runtime/ink/include/canvas/ink/brush_package_catalog.hpp and its source for
actual catalog descriptors; resolve ordinary overrides through existing packages.
Create apps/ink_playground/common/canvas_control_binding.hpp and .cpp as the
composition adapter; extend ink_playground_host.hpp/.cpp only at owner seams.
Keep apps/ink_playground/common/brush_commit_adapter.cpp as the existing captured
BrushExecutionSnapshot path, without schema changes.
Wire debug_ui/src/controller.cpp, its headers/snapshots and panel helpers, plus
apps/ink_playground/platform/windows/main.cpp, platform/web/bridge.cpp,
platform/android/bridge.cpp and their existing UI hosts/build files.
Reuse runtime/interaction viewport/history/selection/eraser owners and the
runtime/render per-view FrameState/EditingOverlay and retained preview paint.
Add verification/tools/dui_m1_configure.py and .github/workflows/dui-m1-qualification.yml;
reuse AGENTS.md SDK resolution and verification/tools/g47_configure.py. Never
replace the locked Semantic/Skia supply chain or modify G4.7 evidence.

## Interfaces
Preserve RuntimeFacade::submitProductControl(const ProductControlRequest&).
Append typed payload/target and structured receipt details; legacy requests are
converted once in common code, never interpreted twice.
CanvasControlService provides enqueue(request), processPending(ownerSequence),
receipt(requestId), snapshot(), presets(). Its host port delegates typed actions
to existing owners and returns atomic outcomes. snapshot() returns immutable
CanvasControlSnapshot; readRuntimeBuildInfo() returns generated RuntimeBuildInfo.
Keep owner scheduling and presentation requests out of UI-owned state. Resolve
concrete port method signatures consistently with the value contracts before
coding each slice; no second command lane, store, camera or thread pool.

## T1 - common control and truthful state
Test runtime/control/tests/canvas_control_service_test.cpp:
rejectStaleTargetWithoutMutation; lateReplyCannotSwitchSelectedView;
duplicateUndoNotAppliedTwice; queueFullAndDestroyReachTerminalReceipt.
Also cover two clients with overlapping local counters, bounded expired receipts,
read-only requests and permission/lifetime changes between enqueue and apply.
Create target axiom_dui_m1_control_tests / CTest DUIM1Control. Implement the common
service and binding; assert no canonical/history mutation by local controls.

## T2 - tool, preset, captured options and erase
Test apps/ink_playground/tests/dui_m1_tool_options_test.cpp:
invalidPresetLeavesWholeStateUnchanged; optionsOnlyAffectNextSession;
zeroOpacityIsAValueNotClear; twoPointersRetainOwnPaint;
eraserDiameterUsesLogicalUnits. Fixed input+seed: size 4 versus 12, red versus
blue, opacity 1 versus 0.25. Validate actual geometry/paint AND committed snapshot;
active/old strokes stay unchanged. Cover invalid NaN/infinity/negative inputs,
ClearOverride and Busy atomicity. Keep vector/dab and preview/canonical/replay
parity. Create axiom_dui_m1_tool_tests / DUIM1ToolOptions.

## T3 - view and history
Test apps/ink_playground/tests/dui_m1_view_controls_test.cpp:
panToolMovesViewWithoutStroke; absoluteZoomPreservesAnchor;
fitWorldRectUsesFullPayload; emptySelectionDoesNotChangeCamera;
twoViewsDprRebindRemainIsolated. Use zoom 0.5/1/2, DPR 1/2, nonzero high 64 bits
in ObjectId, resize/rebind and late target replies. Preserve existing numeric
Fit/clamp policy; camera-only controls leave canonical/shared Scene content
unchanged. Create axiom_dui_m1_view_tests / DUIM1ViewControls.

## T4 - actual UI and three-platform bindings
Tests debug_ui/tests/dui_m1_panel_model_test.cpp and
apps/ink_playground/tests/dui_m1_platform_control_test.cpp; register
DUIM1PanelModel and DUIM1PlatformControl. Route Select -> Ink/preset -> options
-> pan/Fit -> Undo/Redo through actual common binding entry points. Test Busy,
missing resources, rejected/stale targets, immutable truth readback and UI input
capture (color-picker drag must not draw on the canvas). Produce Windows exe,
Web page+WASM and Android APK using the common control owner. Record exactly which
UI routes were exercised by automation; builds alone are BuildReady, not physical
PASS. Ordinary options work with experimental BrushAuthoring absent/disabled.

## T5 - identity and qualification
Tests runtime/control/tests/build_info_test.cpp and
apps/ink_playground/tests/dui_m1_qualification_runner.cpp; register DUIM1BuildInfo,
DUIM1Qualification and axiom_dui_m1_qualification_runner. Add
verification/corpus/golden/dui-m1/manifest.json mapping M1-01..07 to tests and UI
scenarios. Test buildIdentityUsesActualSourceNotPocVersion and
experimentalOffKeepsOrdinaryControls. Missing mandatory coverage cannot be marked
complete. Keep assertions enabled in Release/RelWithDebInfo test executables.

## Frozen command interface (wrapper is created by this package)
The wrapper resolves/reuses the checked-in SDK tooling and flags from the G4.7
workflow; it creates the chosen build directory with tests and full Debug UI.
No source SDK rebuilds; environment failure is not a behavioral RED.

python verification/tools/dui_m1_configure.py windows --build out/dui-m1-windows
cmake --build out/dui-m1-windows --config RelWithDebInfo --parallel 2
ctest --test-dir out/dui-m1-windows -C RelWithDebInfo -R "^DUIM1" --output-on-failure --no-tests=error
ctest --test-dir out/dui-m1-windows -C RelWithDebInfo -R "^(G4EditorHistory|G4SelectionSession|G4EraserSession|C0ViewportInteractionController|G46BViewportConstraints|G46EditingOverlay|G47TextLayout|G47TextPipeline|G47TextHost)$" --output-on-failure --no-tests=error
out/dui-m1-windows/apps/ink_playground/axiom_dui_m1_qualification_runner.exe out/dui-m1-windows/qualification
python verification/tools/dui_m1_configure.py web --build out/dui-m1-web
cmake --build out/dui-m1-web --target axiom_ink_playground_web --parallel 2
python verification/tools/dui_m1_configure.py android --build out/dui-m1-android
cmake --build out/dui-m1-android --target axiom_ink_playground_android --parallel 2

The Android wrapper writes out/dui-m1-android/gradle-arguments.json. On the Android
build host run Gradle assembleDebug with -PaxiomCmakeArguments set to that file's
resolved absolute path, as the accepted G4.7 workflow does. Record the exact
expanded command and APK artifact. Finally run git diff --check.
Require the complete seven-name DUIM1 test inventory and the nine named regression
tests; regex success with a silently absent named test is not success. Reuse
additional affected Brush/preview/C1 tests when those implementations are touched.

## Return
T1-T5 are sequential slices of one task, not five new authorization ceremonies.
Publish a visible checkpoint as soon as actual controls work; continue the
remaining in-scope work without waiting for background or networking. Return
READY_FOR_CONTROL_REVIEW with runnable bundles and PHYSICAL_PENDING recorded
separately. Do not mark M2, real Sync, G4 Gate or G5 as passed.
