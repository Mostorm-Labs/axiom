from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent.parent
CONTROLLER = REPO / "debug_ui" / "src" / "controller.cpp"
RUNTIME_FACADE = REPO / "runtime" / "foundation" / "include" / "canvas" / "runtime" / "runtime_facade.hpp"
WINDOWS_MAIN = ROOT / "platform" / "windows" / "main.cpp"


def test_debug_ui_uses_capability_tabs_and_exposes_selection_control():
    controller = CONTROLLER.read_text(encoding="utf-8")
    facade = RUNTIME_FACADE.read_text(encoding="utf-8")
    main = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "ImGui::BeginTabBar" in controller
    assert "Canvas / Selection" in controller
    assert "RuntimeFacade" in controller
    assert "Selection mode" in controller
    assert "kSetSelectionMode" in facade
    assert "selectionMode" in facade
    assert "kToolSelection" in main


def test_selection_click_is_separate_from_brush_pointer_path():
    main = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "selectionMode" in main
    assert "selectAtViewPoint" in main
    assert "beginCanvasInput" in main
    assert "selectionMode &&" in main


def test_selection_pointer_state_is_terminally_cleared_and_facade_syncs_tool_state():
    main = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "value.selectionPointers.clear()" in main
    facade_block = main[main.index("class WindowsRuntimeFacade final"):main.index("class WindowsArcDiagnostics final")]
    assert "state_.selectedTool = kToolSelection" in facade_block
    assert "state_.canonicalFrameReady = false" in facade_block
