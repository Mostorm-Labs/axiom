#include "imgui.h"
#include "imgui_internal.h"

#include <cassert>
#include <cstdio>
#include <string_view>
#include <windows.h>

namespace {
POINTER_INFO nativePointer{};
BOOL observedPointer(UINT32 id, POINTER_INFO* out) {
    if (id != nativePointer.pointerId)
        return FALSE;
    *out = nativePointer;
    return TRUE;
}
BOOL noTouchHistory(UINT32, UINT32*, POINTER_TOUCH_INFO*) {
    return FALSE;
}
BOOL noTouchInfo(UINT32, POINTER_TOUCH_INFO*) {
    return FALSE;
}
SHORT observedKeyState(int) {
    return 0;
}
} // namespace

// Hidden, isolated Win32 fixture: no physical pointer/keyboard is synthesized.
// The actual Debug UI, Router, host window procedure and Runtime remain intact.
#define GetPointerInfo observedPointer
#define GetPointerTouchInfoHistory noTouchHistory
#define GetPointerTouchInfo noTouchInfo
#define GetKeyState observedKeyState
#define WinMain AxiomWindowEntryForTextToolTest
#include "../windows_main.cpp"
#undef WinMain
#undef GetPointerInfo
#undef GetPointerTouchInfoHistory
#undef GetPointerTouchInfo
#undef GetKeyState

int main(int argc, char** argv) {
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    using namespace canvas;
    const std::string_view mode = argc > 1 ? argv[1] : "entry";
    const bool checkFocus = mode != "entry";
    State state;
    state.host = std::make_unique<MinimalRuntimeHost>();
    state.facade = std::make_unique<MinimalRuntimeFacade>(*state.host);
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW klass{};
    klass.hInstance = instance;
    klass.lpfnWndProc = windowProc;
    klass.lpszClassName = L"AxiomTextToolEntryFixture";
    assert(RegisterClassW(&klass));
    const auto owner = CreateWindowExW(0,
                                       klass.lpszClassName,
                                       L"text fixture",
                                       WS_OVERLAPPEDWINDOW,
                                       0,
                                       0,
                                       800,
                                       600,
                                       nullptr,
                                       nullptr,
                                       instance,
                                       &state);
    assert(owner);
    auto& composition = state.host->composition();
    assert(composition.bindSurface(800, 600));
    assert(state.host->registerSource(1));
    assert(minimal_runtime_host::bindHostTextFont(composition, "axiom-font.otf"));
    if (mode == "shape-label") {
        runtime::CanvasControlRequest control;
        control.clientId = 0xFC025;
        control.target = state.facade->mountedCanvasTargets().front();
        for (unsigned preset = 0; preset < 3; ++preset) {
            control.requestId++;
            control.payload.kind = runtime::CanvasControlPayloadKind::kSelectCreationPreset;
            control.payload.creationPreset = static_cast<runtime::CanvasCreationPreset>(preset);
            assert(state.facade->submitCanvasControl(control).state == runtime::ProductControlState::kQueued);
            assert(composition.ownerPump());
            const auto y = 100 + static_cast<int>(preset) * 130;
            SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(430, y));
            SendMessageW(owner, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(630, y + 80));
            SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(630, y + 80));
            assert(composition.semanticObjectCount() == preset + 1);
            control.requestId++;
            control.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
            control.payload.tool = runtime::CanvasToolKind::kText;
            assert(state.facade->submitCanvasControl(control).state == runtime::ProductControlState::kQueued);
            assert(composition.ownerPump());
            // A real double-click includes the initial single-click draft.
            SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(530, y + 40));
            SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(530, y + 40));
            pumpWindowsText(state);
            SendMessageW(owner, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(530, y + 40));
            SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(530, y + 40));
            pumpWindowsText(state);
            const auto editor = state.facade->readTextEditor();
            assert(editor.phase == runtime::TextPhase::kEditing);
            assert(state.facade->readTextGeometry().available);
            const auto* before = composition.qualificationRuntime().qualificationObjectRecord(editor.token.object);
            assert(before && before->kind == semantic::ObjectKind::kShape);
            const auto shape = *before;
            assert(!std::get<semantic::ShapeContent>(shape.content).label);
            SendMessageW(owner, WM_CHAR, 'A', 0);
            SendMessageW(owner, WM_CHAR, 'B', 0);
            pumpWindowsText(state);
            assert(state.facade->readTextEditor().projection == u"AB");
            textIntent(state, runtime::TextIntentKind::kFinish);
            pumpWindowsText(state);
            const auto* after = composition.qualificationRuntime().qualificationObjectRecord(shape.id);
            assert(after && after->kind_version == 3);
            assert(after->transform == shape.transform);
            assert(std::get<semantic::ShapeContent>(after->content).label);
            assert(composition.semanticObjectCount() == preset + 1);
            control.requestId++;
            control.payload.tool = runtime::CanvasToolKind::kSelect;
            assert(state.facade->submitCanvasControl(control).state == runtime::ProductControlState::kQueued);
            assert(composition.ownerPump());
            SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(530, y + 40));
            SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(530, y + 40));
            SendMessageW(owner, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(530, y + 40));
            SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(530, y + 40));
            pumpWindowsText(state);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
            assert(state.facade->readTextEditor().token.object == shape.id);
            assert(state.facade->readTextEditor().projection == u"AB");
            textIntent(state, runtime::TextIntentKind::kFinish);
            pumpWindowsText(state);
            std::printf("Shape preset %u: first double-click + native characters retained one Shape\n", preset);
        }
        DestroyWindow(owner);
        UnregisterClassW(klass.lpszClassName, instance);
        return 0;
    }
    if (mode == "ink-double-click") {
        runtime::TextIntent create;
        create.clientId = 0xFA02;
        create.requestId = 1;
        create.target = state.facade->mountedCanvasTargets().front();
        create.kind = runtime::TextIntentKind::kCreate;
        create.viewX = 450;
        create.viewY = 100;
        assert(state.facade->submitTextIntent(create).status == runtime::TextStatus::kQueued);
        assert(composition.ownerPump());
        SendMessageW(owner, WM_CHAR, 'A', 0);
        SendMessageW(owner, WM_CHAR, 'B', 0);
        pumpWindowsText(state);
        textIntent(state, runtime::TextIntentKind::kFinish);
        pumpWindowsText(state);
        runtime::CanvasControlRequest ink;
        ink.clientId = 0xFA02;
        ink.requestId = 2;
        ink.target = state.facade->mountedCanvasTargets().front();
        ink.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
        ink.payload.tool = runtime::CanvasToolKind::kInk;
        assert(state.facade->submitCanvasControl(ink).state == runtime::ProductControlState::kQueued);
        assert(composition.ownerPump());
        SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(452, 103));
        SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(452, 103));
        SendMessageW(owner, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(452, 103));
        SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(452, 103));
        pumpWindowsText(state);
        std::printf("Ink double-click: text_phase=%u objects=%zu\n",
                    static_cast<unsigned>(state.facade->readTextEditor().phase),
                    composition.semanticObjectCount());
        std::fflush(stdout);
        assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
        assert(state.facade->readCanvasControlSnapshot(ink.target).tool == runtime::CanvasToolKind::kInk);
        DestroyWindow(owner);
        UnregisterClassW(klass.lpszClassName, instance);
        return 0;
    }
    if (mode == "after-ink") {
        for (int i = 0; i < 10; ++i) {
            SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(450, 100));
            SendMessageW(owner, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(500, 120));
            SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(500, 120));
        }
        assert(composition.semanticObjectCount() == 10);
    }
    const auto objectsBefore = composition.semanticObjectCount();
    state.debugUi = std::make_unique<debug_ui::WindowsDebugUiHost>();
    assert(state.debugUi->initialize(owner));
    state.controller = std::make_unique<debug_ui::DebugController>(
        debug_ui::DebugControllerContext{state.facade.get()});
    state.controller->uiState().setWorkspace(debug_ui::WorkspaceId::kControl);
    state.debugUi->setController(state.controller.get());
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(410, 560);
    io.DeltaTime = 1.0F / 60.0F;
    // Materialize real workbench child IDs, then activate the real Text widget.
    for (int i = 0; i < 2; ++i) {
        ImGui::NewFrame();
        (void)state.controller->buildImGuiFrame();
        ImGui::Render();
    }
    ImGuiWindow* content = nullptr;
    auto& g = *ImGui::GetCurrentContext();
    for (auto* window : g.Windows)
        if (std::string_view(window->Name).find("/##content_") != std::string_view::npos)
            content = window;
    assert(content);
    ImGui::NewFrame();
    g.NavActivateId = g.NavActivateDownId = content->GetID("Text tool");
    g.NavInputSource = ImGuiInputSource_Keyboard;
    assert(state.controller->buildImGuiFrame());
    ImGui::Render();
    assert(composition.ownerPump());
    const auto tool =
        state.facade->readCanvasControlSnapshot(state.facade->mountedCanvasTargets().front()).tool;
    std::printf("after Text widget: tool=%u phase=%u objects=%zu\n",
                static_cast<unsigned>(tool),
                static_cast<unsigned>(state.facade->readTextEditor().phase),
                composition.semanticObjectCount());
    std::fflush(stdout);
    assert(tool == runtime::CanvasToolKind::kText);
    SetFocus(state.debugUi->nativeWindow());
    assert(GetFocus() == state.debugUi->nativeWindow());
    if (mode == "touch") {
        POINT point{550, 200};
        assert(ClientToScreen(owner, &point));
        nativePointer.pointerType = PT_TOUCH;
        nativePointer.pointerId = 7;
        nativePointer.sourceDevice = reinterpret_cast<HANDLE>(1);
        nativePointer.hwndTarget = owner;
        nativePointer.ptPixelLocation = point;
        nativePointer.dwTime = 1;
        nativePointer.pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT;
        SendMessageW(owner, WM_POINTERDOWN, 7, 0);
        nativePointer.dwTime = 2;
        nativePointer.pointerFlags = POINTER_FLAG_UP;
        SendMessageW(owner, WM_POINTERUP, 7, 0);
        assert(GetCapture() != owner); // Touch does not take global mouse capture.
    } else {
        SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(550, 200));
        SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(550, 200));
    }
    std::printf(
        "after canvas click: tool=%u phase=%u canvas_focus=%d objects=%zu\n",
        static_cast<unsigned>(
            state.facade->readCanvasControlSnapshot(state.facade->mountedCanvasTargets().front())
                .tool),
        static_cast<unsigned>(state.facade->readTextEditor().phase),
        GetFocus() == owner,
        composition.semanticObjectCount());
    std::fflush(stdout);
    assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
    assert(composition.semanticObjectCount() == objectsBefore);
    if (checkFocus)
        assert(GetFocus() == owner);
    // Dispatch where Win32 would send typing, rather than bypassing the HWND focus.
    SendMessageW(GetFocus(), WM_CHAR, 'A', 0);
    SendMessageW(GetFocus(), WM_CHAR, 0x4F60, 0);
    SendMessageW(GetFocus(), WM_CHAR, 0x597D, 0);
    pumpWindowsText(state);
    if (checkFocus) {
        assert(state.facade->readTextEditor().projection == u"A\u4f60\u597d");
        assert(composition.semanticObjectCount() == objectsBefore + 1);
        if (mode == "format") {
            textIntent(state, runtime::TextIntentKind::kSelectAll);
            pumpWindowsText(state);
            const auto readFlag = [&](std::string_view name) {
                const auto f = state.facade->readTextEditor().format;
                if (name == "Bold") return f.bold.value_or(false);
                if (name == "Italic") return f.italic.value_or(false);
                if (name == "Underline") return f.underline.value_or(false);
                if (name == "Strike") return f.strikethrough.value_or(false);
                return f.bulleted.value_or(false);
            };
            for (const char* label : {"Bold", "Italic", "Underline", "Strike", "Bulleted list"}) {
                for (const bool enabled : {true, false, true}) {
                    SetFocus(state.debugUi->nativeWindow());
                    ImGui::NewFrame();
                    g.NavActivateId = g.NavActivateDownId = content->GetID(label);
                    g.NavInputSource = ImGuiInputSource_Keyboard;
                    (void)state.controller->buildImGuiFrame();
                    ImGui::Render();
                    assert(composition.ownerPump());
                    for (int frame = 0; frame < 3; ++frame) {
                        ImGui::NewFrame();
                        (void)state.controller->buildImGuiFrame();
                        ImGui::Render();
                        assert(readFlag(label) == enabled);
                        assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
                    }
                    std::printf("format %s=%d selection=%u\n", label, enabled,
                                state.facade->readTextEditor().selection.length);
                }
            }
            state.debugUi->shutdown();
            DestroyWindow(owner);
            UnregisterClassW(klass.lpszClassName, instance);
            std::puts("PASS: five real format widgets keep Runtime readback across frames");
            return 0;
        }
        SendMessageW(GetFocus(), WM_KEYDOWN, VK_RETURN, 0);
        pumpWindowsText(state);
        assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
        assert(state.facade->readCanvasControlSnapshot(state.facade->mountedCanvasTargets().front())
                   .tool == runtime::CanvasToolKind::kText);
        if (mode == "reedit") {
            const auto originalObject = state.facade->readTextEditor().token.object;
            for (int attempt = 0; attempt < 3; ++attempt) {
                // The first Down creates an empty Text draft. The subsequent
                // Win32 double-click must replace it with the existing text.
                SetFocus(state.debugUi->nativeWindow());
                SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(554, 207));
                SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(554, 207));
                SendMessageW(owner, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(554, 207));
                SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(554, 207));
                pumpWindowsText(state);
                const auto editing = state.facade->readTextEditor();
                std::printf("reedit %d: phase=%u object_match=%d projection_units=%zu\n",
                            attempt, static_cast<unsigned>(editing.phase),
                            editing.token.object == originalObject, editing.projection.size());
                std::fflush(stdout);
                assert(editing.phase == runtime::TextPhase::kEditing);
                assert(editing.token.object == originalObject);
                assert(editing.projection == u"A\u4f60\u597d");
                assert(GetFocus() == owner);
                SetFocus(state.debugUi->nativeWindow());
                pumpWindowsText(state);
                assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
                assert(state.facade->readTextEditor().token == editing.token);
                textIntent(state, runtime::TextIntentKind::kFinish);
                pumpWindowsText(state);
                assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
                assert(composition.semanticObjectCount() == objectsBefore + 1);
            }
        }
        SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(550, 300));
        SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(550, 300));
        assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
        assert(composition.semanticObjectCount() == objectsBefore + 1);
    }
    state.debugUi->shutdown();
    DestroyWindow(owner);
    UnregisterClassW(klass.lpszClassName, instance);
    std::puts("PASS: real Text widget / host canvas entry");
}
