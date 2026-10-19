from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent.parent
CONTROLLER = REPO / "debug_ui" / "src" / "controller.cpp"
RUNTIME_FACADE = REPO / "runtime" / "foundation" / "include" / "canvas" / "runtime" / "runtime_facade.hpp"
WINDOWS_MAIN = ROOT / "platform" / "windows" / "main.cpp"


def test_debug_ui_uses_control_and_inspect_panels_for_selection():
    controller = CONTROLLER.read_text(encoding="utf-8")
    control = (REPO / "debug_ui" / "src" / "panels" / "control.cpp").read_text(encoding="utf-8")
    inspect = (REPO / "debug_ui" / "src" / "panels" / "inspect.cpp").read_text(encoding="utf-8")
    router = (REPO / "debug_ui" / "include" / "canvas" / "debug_ui" / "control_router.hpp").read_text(encoding="utf-8")
    facade = RUNTIME_FACADE.read_text(encoding="utf-8")
    main = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "ImGui::BeginTabBar" not in controller
    assert "Canvas / Selection" not in controller
    assert "RuntimeFacade" not in controller
    assert "Selection mode" in control
    assert "context.controls.setSelectionMode(selectionMode)" in control
    assert "snapshot.product.value.selection.enabled" in control
    assert "WorkspaceId::kControl, PanelSlot::kTools" in control
    assert 'ImGui::SeparatorText("Selection")' in inspect
    assert "WorkspaceId::kInspect, PanelSlot::kSelection" in inspect
    assert "s.product.value.selection.selectedObjectCount" in inspect
    assert "setSelectionMode(bool enabled)" in router
    assert "kSetSelectionMode" in facade
    assert "selectionMode" in facade
    assert "kToolSelection" in main


def test_selection_click_is_separate_from_brush_pointer_path():
    main = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "selectionMode" in main
    assert "submitSelectionPointer" in main
    assert "selectionPointer" in main
    assert "beginCanvasInput" in main
    assert "selectionMode &&" in main


def test_selection_pointer_state_is_terminally_cleared_and_facade_syncs_tool_state():
    main = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "value.selectionPointers.clear()" in main
    facade_block = main[main.index("class WindowsRuntimeFacade final"):main.index("class WindowsArcDiagnostics final")]
    assert "state_.selectedTool = kToolSelection" in facade_block
    assert "state_.canonicalFrameReady = false" in facade_block
