from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HOST = ROOT / "debug_ui" / "src" / "windows_host.cpp"
RENDERER = ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "imgui_skia_renderer.hpp"
CONTROLLER = ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "controller.hpp"
CONTROLLER_IMPL = ROOT / "debug_ui" / "src" / "controller.cpp"
WORKBENCH = ROOT / "debug_ui" / "src" / "workbench.cpp"
RENDERER_IMPL = ROOT / "debug_ui" / "src" / "imgui_skia_renderer.cpp"
RUNTIME_FACADE = ROOT / "runtime" / "foundation" / "include" / "canvas" / "runtime" / "runtime_facade.hpp"
SURFACE = ROOT / "runtime" / "foundation" / "include" / "canvas" / "runtime" / "surface_debug_control.hpp"
ROUTER = ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "control_router.hpp"
ROUTER_IMPL = ROOT / "debug_ui" / "src" / "control_router.cpp"
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
    header = CONTROLLER.read_text(encoding="utf-8")
    impl = CONTROLLER_IMPL.read_text(encoding="utf-8")
    router = ROUTER.read_text(encoding="utf-8")
    router_impl = ROUTER_IMPL.read_text(encoding="utf-8")
    assert "RuntimeFacade* runtime" in header
    assert "DebugControlRouter*" in header
    assert "DebugControlRouter" in router
    assert "enqueueSurfaceMode" in router_impl
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


def test_overlay_refreshes_snapshot_before_panel_render():
    host = HOST.read_text(encoding="utf-8")
    header = (ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "windows_host.hpp").read_text(encoding="utf-8")
    main = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "std::function<DebugSnapshot()> snapshotRefresh_" in header
    assert "if (snapshotRefresh_)" in host
    assert "snapshot_ = snapshotRefresh_();" in host
    assert "value.debugUi->setSnapshotRefresh" in main
    assert "debugUi->refresh();" in main


def test_windows_uses_common_snapshot_assembler_without_local_mapping():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "DebugSnapshotAssembler" in source
    assert "captureDebugSnapshot" in source
    assert "buildDebugSnapshot(" not in source


def test_overlay_uses_one_imgui_new_frame_per_render_pass():
    source = HOST.read_text(encoding="utf-8")
    render = source.split("void WindowsDebugUiHost::renderFrame()", 1)[1].split(
        "void WindowsDebugUiHost::refresh", 1
    )[0]
    assert render.count("ImGui::NewFrame();") == 1
    assert render.count("ImGui_ImplWin32_NewFrame();") == 1
    assert "const bool submitted = drawPanels();" in render


def test_windows_activity_log_and_router_refresh_before_publish():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    assert "DebugActivityLog" in source
    assert "DebugControlRouter" in source
    capture = source.split("canvas::debug_ui::DebugSnapshot captureDebugSnapshot", 1)[1].split(
        "void paint", 1
    )[0]
    assert "snapshotAssembler.capture" in capture
    assert "controlRouter->beginFrame" in capture
    assert "controlRouter->refreshReceipts" in capture
    assert "snapshot.activity = value.activityLog->snapshot()" in capture


def test_generic_debug_ui_command_queue_is_retired_but_owner_queues_remain():
    debug_dir = ROOT / "debug_ui"
    cmake = (debug_dir / "CMakeLists.txt").read_text(encoding="utf-8")
    assert not (debug_dir / "include" / "canvas" / "debug_ui" / "command.hpp").exists()
    assert not (debug_dir / "src" / "command.cpp").exists()
    assert "src/command.cpp" not in cmake
    assert (debug_dir / "include" / "canvas" / "debug_ui" / "debug_command_queue.hpp").exists()
    assert (debug_dir / "include" / "canvas" / "debug_ui" / "surface_debug_queue.hpp").exists()


def test_router_accepts_semantic_pan_without_legacy_presentation_ids():
    header = (ROOT / "debug_ui" / "include" / "canvas" / "debug_ui" / "control_router.hpp").read_text(encoding="utf-8")
    implementation = (ROOT / "debug_ui" / "src" / "control_router.cpp").read_text(encoding="utf-8")
    assert "CanvasToolKind tool" in header
    assert "4107" not in header
    assert "4107" not in implementation
    assert "request.payload.tool = tool" in implementation


def test_windows_timer_does_not_rasterize_debug_overlay_during_active_stroke():
    source = WINDOWS_MAIN.read_text(encoding="utf-8")
    timer = source.split("if (message == WM_TIMER", 1)[1].split(
        "if (message == WM_SIZE", 1
    )[0]
    assert "value->activeKeys.empty()" in timer
    assert "value->resizeInProgress" in timer
    assert "debugUi->refresh();" in source


def test_common_workbench_owns_shell_and_workspace_navigation():
    source = WORKBENCH.read_text(encoding="utf-8")
    for region in ("##header", "##sidebar", "##content", "##footer"):
        assert region in source
    assert "kWorkspaceLabels" in source
    assert "registry.contributions(ui.workspace())" in source
    assert "No panels registered" in source
    assert "snapshot.activity.entries.back()" in source
    assert "io.DisplaySize" in source or "GetIO().DisplaySize" in source
    assert "410.0f" not in source and "560.0f" not in source
    assert "DockSpace" not in source and "DockBuilder" not in source


def test_compatibility_entrypoint_renders_final_builtin_workbench_without_legacy_body():
    source = CONTROLLER_IMPL.read_text(encoding="utf-8")
    assert "workbench.render" in source
    assert "registerBuiltinPanels(registry)" in source
    assert "Transitional legacy controls" not in source
    assert "##debug_tabs" not in source
    assert "buildLegacyCompatibilityPanel" not in source
    assert "context.ui.markControlSubmitted()" not in source
    assert "StateStorage" in source
    assert "ImGui::Begin(\"Axiom Debug UI\"" not in source
    assert "ImGuiSkiaRenderer::render" not in source
    assert "static PanelRegistry" not in source and "static DebugUiSessionState" not in source
    for relative in (
        "debug_ui/src/builtin_panels.cpp",
        "debug_ui/src/panels/dashboard.cpp",
        "debug_ui/src/panels/control.cpp",
        "debug_ui/src/panels/features/ink.cpp",
        "debug_ui/src/panels/inspect.cpp",
        "debug_ui/src/panels/runtime.cpp",
        "debug_ui/src/panels/performance.cpp",
        "debug_ui/src/panels/scenarios.cpp",
    ):
        assert (ROOT / relative).exists(), relative


def test_final_panels_keep_unsupported_boundaries_and_snapshot_readback():
    source = "\n".join(
        (ROOT / relative).read_text(encoding="utf-8")
        for relative in (
            "debug_ui/src/panels/dashboard.cpp",
            "debug_ui/src/panels/control.cpp",
            "debug_ui/src/panels/features/ink.cpp",
            "debug_ui/src/panels/inspect.cpp",
            "debug_ui/src/panels/runtime.cpp",
            "debug_ui/src/panels/performance.cpp",
            "debug_ui/src/panels/scenarios.cpp",
        )
    )
    for token in ("Unsupported", ".product.value", "DebugPanelContext"):
        assert token in source
    assert "Reset View" not in source and "100%" not in source
    assert "Enable Trace" not in source and "Enable GPU Timing" not in source
    assert "RuntimeFacade*" not in source and "PlatformDebugControl*" not in source


def test_controller_single_capture_orchestration_and_retired_taxonomy():
    header = CONTROLLER.read_text(encoding="utf-8")
    source = CONTROLLER_IMPL.read_text(encoding="utf-8")
    frame = source.split("bool DebugController::buildImGuiFrame()", 1)[1].split("bool buildImGuiPanels", 1)[0]
    ordered = ["assembler_.capture", "router_.beginFrame", "router_.refreshReceipts",
               "captured.activity = activity_.snapshot()", "latest_ =", "workbench_.render"]
    offsets = [frame.index(token) for token in ordered]
    assert offsets == sorted(offsets)
    assert frame.count("assembler_.capture") == 1
    assert "DebugPanel" not in header and "PanelState" not in header
    assert not (ROOT / "debug_ui/include/canvas/debug_ui/panels.hpp").exists()
    model = (ROOT / "debug_ui/src/panel_model.cpp").read_text(encoding="utf-8")
    assert "describe(" not in model and "PanelCapability" not in model
    assert (ROOT / "debug_ui/src/panels").is_dir()
