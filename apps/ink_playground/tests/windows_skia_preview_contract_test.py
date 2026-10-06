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
    assert "WindowsD3D12SkiaSurfaceProvider" in source
    assert "registerPreviewSurfaceProvider" in source
    assert "SkiaRenderer" in source
    assert "windows_d3d12_skia_surface_provider.cpp" in cmake


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
    assert source.index("registerPreviewSurfaceProvider(") < source.index("ShowWindow(value.window, effectiveShow)")
    assert source.index("attachPreviewTarget(value,") < source.index("ShowWindow(value.window, effectiveShow)")
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
    assert "value.canonicalFrameReady = false" in source
    assert "value.activeKeys.empty()" in source


def test_windows_hides_preview_presentation_before_canonical_redraw():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "previewPresentationEnabled" in source
    # Overlay lifecycle is routed through the host's provider-locked seam so
    # pointer-up cannot race the preview worker while hiding the surface.
    assert "setPreviewOverlayVisible(false)" in source
    assert "value->previewDirty && value->previewPresentationEnabled" in source
    assert "hidePreviewPresentation(value)" in source
    assert "hidePreviewPresentation(state_)" in source or "hidePreviewPresentation(value)" in source


def test_windows_final_pointer_up_completes_canonical_handoff_immediately():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "value.canonicalFrameReady = false" in source
    assert "value->canonicalFrameReady = false" in source


def test_windows_preview_reasserts_overlay_z_order_after_present():
    source = (ROOT / "apps" / "ink_playground" / "platform" / "windows" /
              "windows_skia_preview_surface_provider.cpp").read_text(encoding="utf-8")
    assert "SetWindowPos(overlay_, HWND_TOPMOST" in source
    assert "WS_EX_TOPMOST" in source
    # A visible top-level layered HWND must let a second touch reach the
    # canonical owner while the first contact's preview is visible.
    assert "WS_EX_TRANSPARENT" in source
    assert "WS_POPUP" in source
    assert "owner_, nullptr, instance, owner_" in source
    assert "ShowWindow(overlay_, SW_SHOWNOACTIVATE)" in source
    assert "WM_POINTERDOWN" in source
    assert "SendMessageW(owner" in source


def test_windows_owner_surface_has_presentation_only_amber_fallback():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "Skia layered" in source
    assert "previewProvider" in source


def test_windows_touch_trace_registers_pointer_key_before_history_trace():
    """A WM_POINTER down must not call unordered_map::at before registration."""
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "value->activeKeys.at(pointerId)" not in source
    history_loop = source.index("for (std::size_t index = 0; index < samples.size(); ++index)")
    registration = source.index("value->activeKeys[pointerId] = *key")
    assert registration < history_loop


def test_windows_amber_fallback_applies_runtime_viewport_transform():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "previewProvider" in source
    assert "translationX" in source or "viewport" in source
    assert "translationY" in source or "viewport" in source


def test_windows_pointer_input_defers_preview_and_canonical_to_render_timer():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    mouse_handler = source.split("bool submitMouseSample", 1)[1].split(
        "void cancelPointer", 1
    )[0]
    pointer_handler = source.split("if (message == WM_POINTERDOWN", 1)[1].split(
        "if (message == WM_POINTERCAPTURECHANGED", 1
    )[0]
    assert "presentBrushPreview()" not in mouse_handler
    assert "presentCanonicalFrame(" not in mouse_handler
    assert "presentBrushPreview()" not in pointer_handler
    assert "presentCanonicalFrame(" not in pointer_handler
    timer = source.split(
        "if ((message == WM_TIMER && value != nullptr &&", 1
    )[1].split("if (message == WM_SIZE", 1)[0]
    assert "requestPreviewRender(*value)" in timer
    assert "renderPreviewPresentation" not in pointer_handler
    assert "renderCanonical(*value)" in timer


def test_windows_pointer_up_reuses_already_presented_canonical_frame():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "canonicalFrameReady" in source
    assert "!value.canonicalFrameReady" in source
    assert "canonicalFrameReady = false" in source


def test_windows_preview_is_three_buffered_and_busy_acquire_is_nonblocking():
    provider = (ROOT / "apps" / "ink_playground" / "platform" / "windows" /
                "windows_d3d12_skia_surface_provider.cpp").read_text(encoding="utf-8")
    assert "std::array<ComPtr<ID3D12Resource>, 3>" in provider
    assert "std::array<sk_sp<SkSurface>, 3>" in provider
    assert "desc.BufferCount = static_cast<UINT>(impl_->buffers.size())" in provider
    assert '"preview backbuffer busy"' in provider


def test_windows_input_only_marks_dirty_and_wakes_single_render_pump():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "kRenderWakeMessage" in source
    assert "PostMessageW(value.window, State::kRenderWakeMessage" in source
    assert "value.previewDirty = true" in source
    mouse_handler = source.split("bool submitMouseSample", 1)[1].split(
        "void cancelPointer", 1
    )[0]
    pointer_handler = source.split(
        "if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP)",
        1,
    )[1].split("if (message == WM_POINTERCAPTURECHANGED", 1)[0]
    assert "presentBrushPreview()" not in mouse_handler
    assert "presentCanonicalFrame(" not in mouse_handler
    assert "presentBrushPreview()" not in pointer_handler
    assert "presentCanonicalFrame(" not in pointer_handler
    assert "setPlatformPresentationDeferred(true)" in source
    assert "pendingCanonicalHandoffCount()" in source


def test_windows_present_keeps_transient_dxgi_busy_as_retryable():
    provider = (ROOT / "apps" / "ink_playground" / "platform" / "windows" /
                "windows_d3d12_skia_surface_provider.cpp").read_text(encoding="utf-8")
    assert "DXGI_ERROR_WAS_STILL_DRAWING" in provider
    assert "DXGI_STATUS_OCCLUDED" in provider
    assert "D3D12 present temporarily unavailable" in provider


def test_windows_multi_pointer_handoff_does_not_hide_other_active_preview():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    canonical_visible = source.split(
        "canvas::ink::HandoffResult canonicalVisible(", 1
    )[1].split("private:", 1)[0]
    render_canonical = source.split("bool renderCanonical(State& value)", 1)[1].split(
        "void paint(", 1
    )[0]
    # A canonical-visible receipt for one session must not hide the shared
    # overlay while another pointer still owns a live preview session.
    assert "hidePreviewPresentation(state_)" not in canonical_visible
    assert "!value.host->previewActive()" in render_canonical
    assert "retireVisiblePreviewPresentation(value)" in render_canonical
