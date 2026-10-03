from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HOST = ROOT / "debug_ui" / "src" / "windows_host.cpp"
RENDERER = ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "controller.hpp"
RENDERER_IMPL = ROOT / "debug_ui" / "src" / "controller.cpp"
RUNTIME_FACADE = ROOT / "runtime" / "foundation" / "include" / "canvas" / "runtime" / "runtime_facade.hpp"
SURFACE = ROOT / "runtime" / "foundation" / "include" / "canvas" / "runtime" / "surface_debug_control.hpp"
HOST_HEADER = ROOT / "apps" / "ink_playground" / "common" / "ink_playground_host.hpp"
WINDOWS_MAIN = ROOT / "apps" / "ink_playground" / "platform" / "windows" / "main.cpp"


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


def test_overlay_rebuilds_frame_before_using_capture_state():
    source = HOST.read_text(encoding="utf-8")
    handler = source.split("bool WindowsDebugUiHost::handleMessage", 1)[1].split(
        "void WindowsDebugUiHost::syncOverlay", 1
    )[0]
    render_pos = handler.index("renderFrame();")
    capture_pos = handler.index("const ImGuiIO& io")
    assert render_pos < capture_pos
    assert handler.count("InvalidateRect(overlay_, nullptr, FALSE);") == 1


def test_skia_imgui_renderer_uses_white_font_paint_with_a8_atlas():
    renderer = RENDERER_IMPL.read_text(encoding="utf-8")
    assert "paint.setColor(SK_ColorWHITE)" in renderer


def test_runtime_facade_is_owned_by_runtime_and_has_no_debug_ui_dependency():
    assert RUNTIME_FACADE.exists()
    source = RUNTIME_FACADE.read_text(encoding="utf-8")
    assert "class RuntimeFacade" in source
    assert "submitProductControl" in source
    assert "debug_ui" not in source


def test_surface_control_is_an_owner_interface_with_target_and_expected_generation():
    source = SURFACE.read_text(encoding="utf-8")
    assert "enum class SurfaceRole" in source
    assert "expectedGeneration" in source
    assert "class PlatformDebugControl" in source
    assert "virtual" in source
    assert "generation_" not in source


def test_common_controller_only_submits_to_runtime_and_platform_owner():
    header = RENDERER.read_text(encoding="utf-8")
    impl = RENDERER_IMPL.read_text(encoding="utf-8")
    assert "canvas/runtime/runtime_facade.hpp" in header
    assert "canvas::runtime::RuntimeFacade" in header
    assert "PlatformDebugControl*" in header
    assert "enqueueSurfaceMode" in impl
    assert "platform->requestSurfaceMode" not in impl
    assert "generation_" not in impl


def test_windows_host_does_not_own_surface_mode_truth():
    source = (ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "windows_host.hpp").read_text(encoding="utf-8")
    assert "PlatformDebugControl*" in source
    assert "PlatformDebugControl surfaceControl_" not in source


def test_playground_exposes_generation_fenced_canonical_profile_switch():
    source = HOST_HEADER.read_text(encoding="utf-8")
    assert "selectCanonicalSurfaceProfile" in source
    assert "expectedGeneration" in source


def test_windows_composition_root_implements_platform_owner_and_runtime_facade():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "PlatformDebugControl" in source
    assert "RuntimeFacade" in source
    assert "setRuntimeFacade" in source
    assert "setPlatformDebugControl" in source


def test_windows_composition_root_shares_input_capture_gate_with_debug_host():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    header = (ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "windows_host.hpp").read_text(encoding="utf-8")
    assert "InputCaptureGate inputCapture" in source
    assert "setInputCaptureGate" in header
    assert "setInputCaptureGate(&value.inputCapture)" in source
    assert "DebugInputOwner::kCanvas" in source
