"""Windows preview must consume Runtime geometry and present through Skia.

This is intentionally RED on the common baseline: the old Windows host still
stores ARC primitives and creates the GDI ARC backend as its production
preview renderer.
"""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
WINDOWS_MAIN = ROOT / "apps" / "ink_playground" / "platform" / "windows" / "main.cpp"
WINDOWS_CMAKE = ROOT / "apps" / "ink_playground" / "CMakeLists.txt"


def test_windows_preview_does_not_own_pointer_geometry():
    source = WINDOWS_MAIN.read_text()
    assert "std::vector<arc_preview_primitive_v0> points" not in source
    assert "appendNormalizedPreviewSamples" not in source
    assert "pushArcPreview" not in source


def test_windows_preview_uses_runtime_skia_provider():
    source = WINDOWS_MAIN.read_text()
    cmake = WINDOWS_CMAKE.read_text()
    assert "WindowsSkiaPreviewSurfaceProvider" in source
    assert "registerPreviewSurfaceProvider" in source
    assert "SkiaRenderer" in source
    assert "windows_skia_preview_surface_provider.cpp" in cmake


def test_windows_preview_has_independent_overlay_identity():
    source = WINDOWS_MAIN.read_text()
    assert "previewProvider" in source
    assert "windows-skia-arc-preview" in source
    assert "geometry_source" in source or "BrushPreviewDelta.outline" in source
    assert "CreateWindowsBackend" not in source


def test_windows_resize_uses_host_surface_contract():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "host->resizePreviewSurface" not in source
    assert ".canonicalProviderIdentity" in source
    assert ".previewProviderIdentity" in source


def test_windows_initializes_render_surfaces_before_first_paint():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert source.index("registerPreviewSurfaceProvider(") < source.index("ShowWindow(value.window, show)")
    assert source.index("attachPreviewTarget(value,") < source.index("ShowWindow(value.window, show)")
    paint = source.split("void paint(HWND window, State& value) {", 1)[1].split(
        "LRESULT CALLBACK WindowProc", 1
    )[0]
    assert "canonicalProvider->resize(" not in paint


def test_windows_mouse_samples_use_process_monotonic_sequences():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "mouseSampleSequence" in source
    assert "sample.sample_sequence = ++value.mouseSampleSequence" in source


def test_windows_keeps_preview_until_mouse_release_canonical_handoff():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "presentCanonicalFrame(value.host->canonicalFrameCount() + 1U, 0.0," in source
    assert "value.activeKeys.empty()" in source


def test_windows_preview_reasserts_overlay_z_order_after_present():
    source = (ROOT / "apps" / "ink_playground" / "platform" / "windows" /
              "windows_skia_preview_surface_provider.cpp").read_text(encoding="utf-8")
    assert "SetWindowPos(overlay_, HWND_TOPMOST" in source
    assert "WS_EX_TOPMOST" in source
    assert "WS_EX_TRANSPARENT" not in source
    assert "WS_POPUP" in source
    assert "nullptr, nullptr, instance, nullptr" in source
    assert "ShowWindow(overlay_, SW_SHOWNOACTIVATE)" in source


def test_windows_owner_surface_has_presentation_only_amber_fallback():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "presentation-only Amber fallback" in source
    assert "value.host->brushPreviewOutline()" in source
    assert "RGB(255, 170, 0)" in source
    assert "brushPreviewOutline()" in source
    assert "CreateSolidBrush(RGB(255, 170, 0))" in source
    assert "Polygon(bufferDc" in source
