from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PAGE = ROOT / "platform" / "web" / "index.html"
BRIDGE = ROOT / "platform" / "web" / "bridge.cpp"
APP_CMAKE = ROOT / "CMakeLists.txt"
DEBUG_CMAKE = ROOT.parent.parent / "debug_ui" / "CMakeLists.txt"


def test_web_debug_ui_has_independent_imgui_surface_and_toggle():
    page = PAGE.read_text(encoding="utf-8")
    bridge = BRIDGE.read_text(encoding="utf-8")
    app_cmake = APP_CMAKE.read_text(encoding="utf-8")
    debug_cmake = DEBUG_CMAKE.read_text(encoding="utf-8")

    assert 'id="debugUi"' in page
    assert 'id="debugUiToggle"' in page
    assert "_axiom_ink_debug_ui_init" in page
    assert "_axiom_ink_debug_ui_render" in page
    assert "_axiom_ink_debug_ui_toggle" in page
    assert "_axiom_ink_debug_ui_pointer" in page
    assert "_axiom_ink_debug_ui_key" in page
    assert "ImGui::CreateContext" in bridge
    assert "buildImGuiPanels" in bridge
    assert "snapshot.product.value.tool.toolId" in bridge
    assert "axiom_ink_playground_web.js?v=web-debug-ui-physical" in page
    assert "axiom_ink_selected_tool" in bridge
    assert "_axiom_ink_selected_tool" in page
    assert "ImGuiKey_Z" in bridge
    assert "ctrlDown" in bridge
    assert "ImGuiSkiaRenderer" in bridge
    assert "#debugUi" in bridge
    assert "WantCaptureMouse" in bridge
    assert "WantCaptureKeyboard" in bridge
    assert "AddMousePosEvent" in bridge
    assert "AddMouseButtonEvent" in bridge
    assert "InputCaptureGate" in bridge
    assert "DebugSnapshotAssembler" in bridge
    assert "captureDebugSnapshot" in bridge
    assert "debugSnapshot(" not in bridge
    assert "canvas_debug_ui" in app_cmake
    assert "CanvasSkia::Skia" in debug_cmake
    assert "CANVAS_DEBUG_UI_HAS_SKIA" in debug_cmake


def test_web_debug_ui_does_not_replace_canvas_or_arc_surfaces():
    page = PAGE.read_text(encoding="utf-8")
    bridge = BRIDGE.read_text(encoding="utf-8")
    assert 'id="ink"' in page and 'id="arcPreview"' in page
    assert 'id="debugUi"' in page
    assert "registerPreviewSurfaceProvider" in bridge
    assert "WebGlSurfaceProvider" in bridge
    assert "#ink" in bridge and "#arcPreview" in bridge and "#debugUi" in bridge


def test_web_debug_ui_forwards_uncaptured_pointer_and_maps_named_keys():
    page = PAGE.read_text(encoding="utf-8")
    assert "imguiKeyForEvent" in page
    assert "event.code" in page
    assert "forwardedDebugPointers" in page
    assert "debug_ui_wants_capture" in page
    assert "event.button" in page
    assert "event.pointerId, mouseButton" in page
    assert "keyCode" not in page
    assert "_axiom_ink_debug_ui_capture_begin" in page
    assert "_axiom_ink_debug_ui_capture_route" in page
    assert "_axiom_ink_debug_ui_capture_terminal" in page
    assert "capture_begin(host, BigInt(event.pointerId))" in page
    assert "debugPointers.add(event.pointerId);" in page
    assert "debugPointers.add(event.pointerId);" in page


def test_web_debug_ui_requests_host_render_after_product_control_input():
    page = PAGE.read_text(encoding="utf-8")
    assert "_axiom_ink_debug_ui_pointer(host, 2" in page
    pointerup_handler = page.split('debugCanvas.addEventListener("pointerup"', 1)[1].split(
        'debugCanvas.addEventListener("pointercancel"', 1)[0]
    assert "requestRender();" in pointerup_handler


def test_web_debug_ui_owns_common_activity_log_and_router():
    bridge = BRIDGE.read_text(encoding="utf-8")
    assert "DebugActivityLog activityLog" in bridge
    assert "DebugControlRouter" in bridge
    capture = bridge.split("canvas::debug_ui::DebugSnapshot captureDebugSnapshot", 1)[1].split(
        "bool initializeDebugUi", 1
    )[0]
    assert "snapshotAssembler.capture" in capture
    assert "controlRouter->beginFrame" in capture
    assert "controlRouter->refreshReceipts" in capture
    assert "snapshot.activity = state.activityLog.snapshot()" in capture
