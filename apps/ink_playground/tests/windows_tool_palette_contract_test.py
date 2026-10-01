"""The Windows host exposes the same brush/eraser choices as the Web playground."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
WINDOWS_MAIN = ROOT / "apps" / "ink_playground" / "platform" / "windows" / "main.cpp"


def test_windows_tool_palette_has_all_web_tools():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    for token in (
        '"vector-solid-v1"',
        '"marker-flat-v1"',
        '"chalk-grain-v1"',
        '"membrane-v1"',
        "Object Eraser",
        "Partial Eraser",
    ):
        assert token in source


def test_windows_tool_palette_routes_selection_through_host():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "selectBrushProfile" in source
    assert "selectTool" in source
    assert "WM_COMMAND" in source
    assert "BS_AUTORADIOBUTTON" in source


def test_windows_tool_palette_reserves_canvas_band():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "kToolbarHeight" in source
    assert "setOverlayOffset" in source
    assert "canvasHeight" in source


def test_windows_paint_preserves_toolbar_and_caches_canonical_frames():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "windows-d3d12-canonical" in source
    assert "WindowsD3D12SkiaSurfaceProvider" in source
    assert "D3D12 swap chain" in source


def test_windows_input_coalesces_preview_paints_to_timer_ticks():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "kRenderTimerId" in source
    assert "kRenderIntervalMs" in source
    assert "SetTimer" in source
    assert "WM_TIMER" in source
