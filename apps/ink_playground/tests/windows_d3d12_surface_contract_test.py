from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HEADER = ROOT / "apps/ink_playground/platform/windows/windows_d3d12_skia_surface_provider.hpp"
SOURCE = ROOT / "apps/ink_playground/platform/windows/windows_d3d12_skia_surface_provider.cpp"
MAIN = ROOT / "apps/ink_playground/platform/windows/main.cpp"


def test_windows_d3d12_provider_uses_gpu_skia_without_readback():
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class WindowsD3D12SkiaSurfaceProvider" in header
    assert "GrDirectContexts::MakeD3D" in source
    assert "SkSurfaces::WrapBackendRenderTarget" in source
    assert "CreateSwapChainForComposition" in source
    assert "DCompositionCreateDevice" in source
    assert "UpdateLayeredWindow" not in source
    assert "readbackRgba" in source
    assert '"gpu-d3d12"' in source


def test_windows_d3d12_hide_commits_composition_immediately():
    source = SOURCE.read_text(encoding="utf-8")
    assert "ShowWindow(overlay_, SW_HIDE)" in source
    assert "HWND_BOTTOM" not in source
    assert "SetOpacity(visible ? 1.0F : 0.0F)" not in source


def test_windows_d3d12_preview_present_does_not_wait_for_vsync():
    source = SOURCE.read_text(encoding="utf-8")
    assert "swapChain->Present(0, 0)" in source
    assert "swapChain->Present(1, 0)" not in source


def test_windows_d3d12_preview_rebinds_popup_target_around_resize():
    """Resize must fence and detach the provider before creating new resources."""
    source = SOURCE.read_text(encoding="utf-8")
    resize_block = source.split(
        "canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::resize(",
        1,
    )[1].split(
        "canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::readbackRgba",
        1,
    )[0]
    assert "destroyGpuSurface();" in resize_block
    assert "destroyOverlay();" in resize_block
    assert resize_block.index("destroyGpuSurface()") < resize_block.index(
        "createGpuSurface()"
    )
    assert resize_block.index("destroyOverlay()") < resize_block.index(
        "createGpuSurface()"
    )


def test_windows_d3d12_resize_recreates_context_instead_of_resizing_wrapped_buffers():
    """Wrapped Ganesh D3D resources require a full provider teardown on resize."""
    source = SOURCE.read_text(encoding="utf-8")
    resize = source.split(
        "canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::resize(",
        1,
    )[1].split("canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::readbackRgba", 1)[0]
    assert "resizeGpuSurfaceBuffers" not in resize
    assert "destroyGpuSurface();" in resize
    assert "createGpuSurface()" in resize


def test_windows_d3d12_overlay_forwards_mouse_lifecycle_to_owner():
    source = SOURCE.read_text(encoding="utf-8")
    assert "WM_LBUTTONDOWN" in source
    assert "WM_MOUSEMOVE" in source
    assert "WM_LBUTTONUP" in source
    assert "SendMessageW(owner, message, wParam, lParam)" in source
    pointer_block = source.split("if (message == WM_POINTERDOWN", 1)[1].split(
        "if (message == WM_LBUTTONDOWN", 1
    )[0]
    assert "SetCapture(owner)" not in pointer_block
    assert "ReleaseCapture()" not in pointer_block
    assert "MapWindowPoints(window, owner" not in pointer_block


def test_windows_d3d12_forwards_each_pointer_without_global_capture():
    """Forward each contact by ID; Win32 mouse capture is not per pointer."""
    source = SOURCE.read_text(encoding="utf-8")
    pointer_block = source.split("if (message == WM_POINTERDOWN", 1)[1].split(
        "if (message == WM_LBUTTONDOWN", 1
    )[0]
    assert "SetCapture(owner)" not in pointer_block
    assert "SendMessageW(owner, message, wParam, lParam)" in pointer_block


def test_windows_d3d12_preview_overlay_is_display_only_and_touch_transparent():
    source = SOURCE.read_text(encoding="utf-8")
    create = source.split("overlay_ = CreateWindowExW", 1)[1].split(
        "if (!overlay_)", 1
    )[0]
    create_pos = source.index("overlay_ = CreateWindowExW")
    post_create = source[create_pos:].split("if (!overlay_)", 1)[1].split(
        "if (FAILED(DCompositionCreateDevice", 1
    )[0]
    assert "WS_EX_NOACTIVATE" in create
    assert "WS_EX_TRANSPARENT" in create
    # Layered alpha transparency is required for Windows touch hit testing;
    # HTTRANSPARENT alone is insufficient once the preview popup is visible.
    assert "WS_EX_LAYERED" in create
    assert "WS_EX_NOREDIRECTIONBITMAP" not in create
    assert "EnableWindow(overlay_, FALSE)" not in post_create
    assert "SetLayeredWindowAttributes(overlay_, 0, 255, LWA_ALPHA)" in post_create
    assert "HTTRANSPARENT" in source


def test_windows_d3d12_preview_overlay_never_disables_owner_input_after_resize():
    source = SOURCE.read_text(encoding="utf-8")
    assert "EnableWindow(overlay_, FALSE)" not in source
    assert "return HTTRANSPARENT;" in source
    assert "return MA_NOACTIVATE;" in source


def test_windows_d3d12_preview_overlay_initializes_layered_alpha_for_touch_passthrough():
    source = SOURCE.read_text(encoding="utf-8")
    assert "WS_EX_LAYERED" in source
    assert "SetLayeredWindowAttributes(overlay_, 0, 255, LWA_ALPHA)" in source


def test_windows_canonical_is_owner_attached_gpu_surface_without_cpu_readback():
    source = MAIN.read_text(encoding="utf-8")
    assert 'registerSurfaceProvider("windows-d3d12-canonical"' in source
    assert "value.window, true);" in source
    assert "canonicalProvider->readbackRgba" not in source
    assert "SetDIBitsToDevice" not in source
    assert "canonicalBgra" not in source


def test_owner_attached_surface_has_independent_composition_target():
    source = SOURCE.read_text(encoding="utf-8")
    assert "CreateTargetForHwnd(owner_, TRUE" in source
    assert "DCompositionCreateDevice" in source
    assert "if (attachToOwner_)" in source


def test_matching_canonical_visible_only_acknowledges_session_handoff():
    source = MAIN.read_text(encoding="utf-8")
    callback = source.split("canvas::ink::HandoffResult canonicalVisible(", 1)[1].split(
        "private:", 1
    )[0]
    assert "previewBridge->CanonicalVisible(visible)" in callback
    assert "hidePreviewPresentation(state_)" not in callback
    render = source.split("bool renderCanonical(State& value)", 1)[1].split(
        "void paint(", 1
    )[0]
    assert "!value.host->previewActive()" in render
    assert "retireVisiblePreviewPresentation(value)" in render


def test_pointer_down_reenables_preview_after_previous_stroke_retired():
    source = MAIN.read_text(encoding="utf-8")
    pointer_handler = source.split(
        "if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP)",
        1,
    )[1].split("if (message == WM_POINTERCAPTURECHANGED", 1)[0]
    begin_block = pointer_handler.split("if (begin)", 1)[1].split(
        "std::vector<POINTER_INFO>", 1
    )[0]
    assert "enablePreviewPresentation(*value)" in begin_block


def test_owner_pointer_down_does_not_take_global_mouse_capture():
    source = MAIN.read_text(encoding="utf-8")
    pointer_handler = source.split(
        "if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP)",
        1,
    )[1].split("if (message == WM_POINTERCAPTURECHANGED", 1)[0]
    begin_block = pointer_handler.split("if (begin)", 1)[1].split(
        "std::vector<POINTER_INFO>", 1
    )[0]
    assert "SetCapture(window)" not in begin_block


def test_windows_pointer_capture_change_does_not_cancel_active_ink():
    source = MAIN.read_text(encoding="utf-8")
    block = source.split("if (message == WM_POINTERCAPTURECHANGED", 1)[1].split(
        "if (canvas::ink_playground::windows_input::cancelsAllActivePointers", 1
    )[0]
    assert "cancelPointer" not in block
    assert "capture-changed" in block


def test_windows_pointer_mouse_is_routed_through_mouse_submission_path():
    source = MAIN.read_text(encoding="utf-8")
    assert "if (info.pointerType == PT_MOUSE) return 0;" not in source
    mouse_pointer_block = source.split("if (info.pointerType == PT_MOUSE)", 1)[1].split(
        "const auto phase", 1
    )[0]
    assert "submitMouseSample" in mouse_pointer_block
    assert "ScreenToClient(window" in mouse_pointer_block


def test_windows_mouse_submission_ignores_duplicate_lifecycle_messages():
    source = MAIN.read_text(encoding="utf-8")
    mouse_handler = source.split("bool submitMouseSample", 1)[1].split(
        "void cancelPointer", 1
    )[0]
    assert "if (value.activeKeys.contains(kMousePointerId)) return false;" in mouse_handler
    assert "if (!value.activeKeys.contains(kMousePointerId)) return false;" in mouse_handler


def test_windows_submits_compatibility_mouse_when_device_exposes_no_touch_channel():
    source = MAIN.read_text(encoding="utf-8")
    mouse_branch = source.split(
        "if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_MOUSEMOVE)",
        1,
    )[1].split("if (message == WM_COMMAND", 1)[0]
    assert "submitMouseSample" in mouse_branch
    assert "nativeTouchChannelSeen" in mouse_branch
    assert "kMousePointerId = 0xFFFFFFFFU" in source


def test_windows_mouse_fallback_uses_nonzero_monotonic_timestamp():
    source = MAIN.read_text(encoding="utf-8")
    mouse = source.split("bool submitMouseSample", 1)[1].split(
        "bool submitTouchInput", 1
    )[0]
    assert "GetTickCount64()" in mouse
    assert "lastMouseTimestampNs" in mouse
    assert "GetMessageTime()" not in mouse
    assert "mouse accepted=" in mouse


def test_windows_pointer_down_falls_back_to_current_sample_when_history_is_unavailable():
    source = MAIN.read_text(encoding="utf-8")
    pointer_block = source.split(
        "if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP)",
        1,
    )[1].split("if (message == WM_POINTERCAPTURECHANGED", 1)[0]
    history_block = pointer_block.split("UINT32 pointerHistoryCount = 0;", 1)[1].split(
        "if (pointerHistory.empty()) pointerHistory.push_back(info);", 1
    )[0]
    assert "GetPointerInfoHistory(pointerId, &pointerHistoryCount, nullptr)" in history_block
    assert "return 0;" not in history_block
    assert "pointerHistory.push_back(info)" in pointer_block


def test_windows_pointer_samples_use_independent_nonzero_sequence_clock():
    source = MAIN.read_text(encoding="utf-8")
    assert "pointerSampleSequence" in source
    assert "sample.sample_sequence = ++value->pointerSampleSequence" in source
    assert "history.dwTime) << 16U" not in source
    assert "lastPointerTimestampNs" in source


def test_windows_pointer_diagnostic_records_startup_and_legacy_touch_channel():
    source = MAIN.read_text(encoding="utf-8")
    assert "startup pid=" in source
    assert "window-created hwnd=" in source
    assert "message == WM_TOUCH" in source


def test_windows_registers_touch_window_and_submits_legacy_touch_packets():
    source = MAIN.read_text(encoding="utf-8")
    assert "RegisterTouchWindow(value.window" in source
    assert "GetTouchInputInfo" in source
    assert "CloseTouchInputHandle" in source
    assert "TOUCHEVENTF_DOWN" in source
    assert "TOUCHEVENTF_UP" in source


def test_windows_d3d12_overlay_forwards_legacy_touch_packets():
    source = SOURCE.read_text(encoding="utf-8")
    assert "if (message == WM_TOUCH)" in source
    assert "SendMessageW(owner, message, wParam, lParam)" in source
    assert "RegisterTouchWindow(overlay_" not in source


def test_windows_logs_runtime_contact_disposition_for_multitouch_diagnosis():
    source = MAIN.read_text(encoding="utf-8")
    assert "pointerDisposition(*key)" in source
    assert '"post-route id="' in source
    assert "multiContactPolicy()" in source



def test_windows_d3d12_preview_overlay_does_not_register_touch_targets():
    source = SOURCE.read_text(encoding="utf-8")
    create = source.split("overlay_ = CreateWindowExW", 1)[1].split(
        "if (FAILED(DCompositionCreateDevice", 1
    )[0]
    assert "RegisterTouchWindow(overlay_" not in create
    assert "RegisterPointerInputTarget(overlay_" not in create
