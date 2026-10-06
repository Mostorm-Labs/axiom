from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CMAKE = (ROOT / "debug_ui" / "CMakeLists.txt").read_text(encoding="utf-8")


def test_windows_skia_sources_are_selected_only_after_skia_target_detection():
    source = CMAKE
    assert "if(WIN32 AND TARGET CanvasSkia::Skia)" in source
    guarded = source.split("if(WIN32 AND TARGET CanvasSkia::Skia)", 1)[1].split("endif()", 1)[0]
    assert "src/windows_host.cpp" in guarded
    assert "imgui_impl_win32.cpp" in guarded


def test_generic_debug_ui_target_does_not_unconditionally_compile_windows_skia_host():
    source = CMAKE
    before_guard = source.split("if(WIN32 AND TARGET CanvasSkia::Skia)", 1)[0]
    assert "src/windows_host.cpp" not in before_guard
    assert "imgui_impl_win32.cpp" not in before_guard


def test_skia_enabled_path_still_links_skia_and_defines_renderer_capability():
    source = CMAKE
    assert "if(TARGET CanvasSkia::Skia)" in source
    assert "target_link_libraries(canvas_debug_ui PRIVATE CanvasSkia::Skia)" in source
    assert "CANVAS_DEBUG_UI_HAS_SKIA=1" in source
