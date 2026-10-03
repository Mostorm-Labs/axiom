from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HOST = ROOT / "debug_ui" / "src" / "windows_host.cpp"
RENDERER = ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "controller.hpp"
RENDERER_IMPL = ROOT / "debug_ui" / "src" / "controller.cpp"


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


def test_skia_imgui_renderer_uses_viewer_font_alpha_and_modulate_path():
    host = HOST.read_text(encoding="utf-8")
    renderer = RENDERER_IMPL.read_text(encoding="utf-8")
    assert "GetTexDataAsAlpha8" in host
    assert "SkImageInfo::MakeA8" in host
    assert "GetTexDataAsRGBA32" not in host
    assert "SkBlendMode::kModulate" in renderer


def test_windows_host_sets_viewport_and_frame_time_before_new_frame():
    source = HOST.read_text(encoding="utf-8")
    assert "GetClientRect(overlay_" in source
    assert "io.DisplaySize = ImVec2" in source
    assert "io.DeltaTime" in source


def test_overlay_input_rebuilds_common_imgui_frame_before_blit():
    source = HOST.read_text(encoding="utf-8")
    handler = source.split("bool WindowsDebugUiHost::handleMessage", 1)[1].split(
        "void WindowsDebugUiHost::syncOverlay", 1
    )[0]
    assert "ImGui_ImplWin32_WndProcHandler" in handler
    assert "renderFrame();" in handler


def test_overlay_does_not_reenter_imgui_render_from_paint_messages():
    source = HOST.read_text(encoding="utf-8")
    assert "message == WM_PAINT" in source
    assert "case WM_MOUSEMOVE" in source
    assert "if (!isInputMessage(message)) return false;" in source
    assert "bool rendering" in source


def test_skia_imgui_renderer_uses_white_font_paint_with_a8_atlas():
    renderer = RENDERER_IMPL.read_text(encoding="utf-8")
    assert "paint.setColor(SK_ColorWHITE)" in renderer
