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
        SendMessageW(GetFocus(), WM_KEYDOWN, VK_RETURN, 0);
        pumpWindowsText(state);
        assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
        assert(state.facade->readCanvasControlSnapshot(state.facade->mountedCanvasTargets().front())
                   .tool == runtime::CanvasToolKind::kText);
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
