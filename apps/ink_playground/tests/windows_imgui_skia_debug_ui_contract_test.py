from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HOST = ROOT / "debug_ui" / "src" / "windows_host.cpp"
RENDERER = ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "controller.hpp"


def test_windows_host_uses_common_imgui_frame_pipeline():
    source = HOST.read_text(encoding="utf-8")
    assert "ImGui::CreateContext" in source
    assert "ImGui::NewFrame" in source
    assert "ImGui::Render" in source
    assert "ImGui_ImplWin32_WndProcHandler" in source
    assert "FillRect" not in source
    assert "TextOutW" not in source
    assert "CreateSolidBrush" not in source


def test_common_renderer_consumes_draw_data_and_skia_surface():
    source = RENDERER.read_text(encoding="utf-8")
    assert "ImDrawData" in source
    assert "SkSurface" in source
    assert "render(ImDrawData" in source
