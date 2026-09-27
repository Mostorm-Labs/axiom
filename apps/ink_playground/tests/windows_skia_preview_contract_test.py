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
