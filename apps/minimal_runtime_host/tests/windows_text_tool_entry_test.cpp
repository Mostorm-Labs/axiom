#include "host_font_resources.hpp"
#include "canvas/debug_ui/snapshot_assembler.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <cassert>
#include <cstdio>
#include <string_view>
#include <windows.h>

namespace {
POINTER_INFO nativePointer{};
bool syntheticPointerClock = false;
ULONGLONG pointerClockMs = 0;
ULONGLONG observedTickCount64() {
    return syntheticPointerClock ? pointerClockMs : ::GetTickCount64();
}
DWORD observedTickCount() {
    return syntheticPointerClock ? static_cast<DWORD>(pointerClockMs) : ::GetTickCount();
}
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
#define GetTickCount64 observedTickCount64
#define GetTickCount observedTickCount
#define WinMain AxiomWindowEntryForTextToolTest
#include "../windows_main.cpp"
#undef WinMain
#undef GetPointerInfo
#undef GetPointerTouchInfoHistory
#undef GetPointerTouchInfo
#undef GetKeyState
#undef GetTickCount64
#undef GetTickCount

int main(int argc, char** argv) {
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    using namespace canvas;
    const std::string_view mode = argc > 1 ? argv[1] : "entry";
    const bool checkFocus = mode != "entry";
    State state;
    state.host = std::make_unique<MinimalRuntimeHost>();
    state.facade = &state.host->publicComposition().controlFacade();
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
    if (mode == "public-adapter") {
        state.platformAdapter = std::make_unique<platform::WindowsRuntimeAdapter>(state.host->publicComposition(), owner);
        if (!state.platformAdapter->bind(800, 600, 1.0F)) return 77;
        assert(&state.host->publicComposition().controlFacade() == &composition.controlFacade());
        debug_ui::DebugSnapshotAssembler assembler;
        const auto snapshot = assembler.capture({state.facade, nullptr, nullptr, state.platformAdapter.get()});
        assert(snapshot.platform.availability == debug_ui::DebugAvailability::kAvailable);
        assert(snapshot.platform.value.width == 800 && snapshot.platform.value.height == 600);
        assert(!state.platformAdapter->loadHostFonts(L"missing-axiom-font.otf", L"fonts"));
        assert(state.platformAdapter->loadHostFonts(L"axiom-font.otf", L"fonts"));
        assert(!state.host->publicComposition().controlFacade().readFontCatalog().entries.empty());
        assert(!state.platformAdapter->loadHostFonts(L"axiom-font.otf", L"fonts"));
        SendMessageW(owner, WM_TIMER, 1, 0);
        assert(state.platformAdapter->frameStatus().canonicalWorkId != 0);
        ShowWindow(owner, SW_SHOWNOACTIVATE);
        InvalidateRect(owner, nullptr, FALSE);
        UpdateWindow(owner);
        assert(state.platformAdapter->frameStatus().canonicalWorkId == 0);
        SendMessageW(owner, WM_SIZE, 0, MAKELPARAM(640, 480));
        assert(composition.surfaceSnapshot().width == 640);
        assert(state.platformAdapter->shutdown());
        assert(composition.close());
        DestroyWindow(owner);
        std::puts("PASS Windows Host public adapter same-owner surface and resize");
        return 0;
    }
    assert(composition.bindSurface(800, 600));
    assert(state.host->registerSource(1));
    assert(minimal_runtime_host::bindHostTextFont(composition, "axiom-font.otf"));
    if (mode == "group-dblclick" || mode == "group-touch") {
        const auto a=foundation::ObjectId::fromUint64(9101),b=foundation::ObjectId::fromUint64(9102);
        const auto group=foundation::ObjectId::fromUint64(9103);
        semantic::InsertObjectsOp insert;
        for(const auto id:{a,b}) {
            semantic::ObjectRecord shape;shape.id=id;shape.kind=semantic::ObjectKind::kShape;shape.kind_version=1;
            shape.content=semantic::ShapeContent{1,80,60};shape.transform.tx=id==a?100:260;shape.transform.ty=100;
            shape.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(id==a?1:2)});insert.objects.push_back(shape);
        }
        semantic::Operation seed{semantic::OperationId{foundation::ObjectId::fromUint64(9104)},
            semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,std::move(insert)};
        assert(composition.applyExternalOperation(seed,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied);
        auto& facade=composition.controlFacade();
        assert(facade.groupObjects({a,b},group,1,9100).state==runtime::ProductControlState::kQueued);
        assert(composition.ownerPump());
        assert(facade.setSelectionMode(true,2,facade.readRuntimeState().identity.runtimeGeneration,9100).state==runtime::ProductControlState::kQueued);
        assert(composition.ownerPump());
        if (mode == "group-touch") {
            const auto touch=[&](UINT message) {
                Sleep(40);nativePointer={};nativePointer.pointerType=PT_TOUCH;nativePointer.pointerId=7;
                nativePointer.sourceDevice=reinterpret_cast<HANDLE>(1);nativePointer.hwndTarget=owner;
                nativePointer.ptPixelLocation={120,120};assert(ClientToScreen(owner,&nativePointer.ptPixelLocation));
                nativePointer.dwTime=GetTickCount();nativePointer.pointerFlags=message==WM_POINTERUP?POINTER_FLAG_UP:
                    POINTER_FLAG_DOWN|POINTER_FLAG_INCONTACT;
                SendMessageW(owner,message,7,0);pumpWindowsText(state);
            };
            touch(WM_POINTERDOWN);touch(WM_POINTERUP);touch(WM_POINTERDOWN);touch(WM_POINTERUP);
            assert(facade.readSelectionSummary().groupScope==group);
            assert(facade.readSelectionSummary().primary==a);
            assert(facade.readTextEditor().phase==runtime::TextPhase::kClosed);
            touch(WM_POINTERDOWN);touch(WM_POINTERUP);touch(WM_POINTERDOWN);touch(WM_POINTERUP);
            assert(facade.readTextEditor().phase==runtime::TextPhase::kEditing);
            assert(facade.readTextEditor().token.object==a);
            assert(composition.close());DestroyWindow(owner);
            std::puts("PASS Win32 touch double-tap enters Group before child text editing");return 0;
        }
        const auto doubleClick=[&] {
            SendMessageW(owner,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(120,120));
            SendMessageW(owner,WM_LBUTTONUP,0,MAKELPARAM(120,120));
            SendMessageW(owner,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(120,120));
            SendMessageW(owner,WM_LBUTTONUP,0,MAKELPARAM(120,120));
            pumpWindowsText(state);
        };
        doubleClick();
        assert(facade.readSelectionSummary().groupScope==group);
        assert(facade.readSelectionSummary().primary==a);
        assert(facade.readTextEditor().phase==runtime::TextPhase::kClosed);
        doubleClick();
        assert(facade.readTextEditor().phase==runtime::TextPhase::kEditing);
        assert(facade.readTextEditor().token.object==a);
        assert(composition.close());DestroyWindow(owner);
        std::puts("PASS Win32 double-click enters Group before child text editing");
        return 0;
    }
    if (mode == "touch-shape-label") {
        runtime::CanvasControlRequest control;
        control.clientId = 0xFC027;
        control.target = state.facade->mountedCanvasTargets().front();
        const auto choose = [&](runtime::CanvasToolKind tool) {
            control.requestId++;
            control.payload.kind = runtime::CanvasControlPayloadKind::kSelectTool;
            control.payload.tool = tool;
            assert(state.facade->submitCanvasControl(control).state == runtime::ProductControlState::kQueued);
            assert(composition.ownerPump());
        };
        // These are synthetic native facts, not hardware delivery timings.
        // Synchronous rendering must not turn a 120 ms double tap into a
        // >500 ms pair or turn a 40 ms tap into a long press under CPU load.
        pointerClockMs = GetTickCount64();
        syntheticPointerClock = true;
        const auto touch = [&](UINT message, UINT32 id, int x, int y, DWORD elapsed = 40) {
            pointerClockMs += elapsed;
            nativePointer = {};
            nativePointer.pointerType = PT_TOUCH;
            nativePointer.pointerId = id;
            nativePointer.sourceDevice = reinterpret_cast<HANDLE>(1);
            nativePointer.hwndTarget = owner;
            nativePointer.ptPixelLocation = {x, y};
            assert(ClientToScreen(owner, &nativePointer.ptPixelLocation));
            nativePointer.dwTime = static_cast<DWORD>(pointerClockMs);
            nativePointer.pointerFlags = message == WM_POINTERUP ? POINTER_FLAG_UP :
                message == WM_POINTERDOWN ? POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT :
                                            POINTER_FLAG_UPDATE | POINTER_FLAG_INCONTACT;
            SendMessageW(owner, message, id, 0);
            pumpWindowsText(state);
        };
        const auto tap = [&](int x, int y, DWORD gap = 80) {
            touch(WM_POINTERDOWN, 7, x, y, gap);
            touch(WM_POINTERUP, 7, x, y);
        };
        const auto drag = [&](int x, int y) {
            touch(WM_POINTERDOWN, 7, x, y, 80);
            touch(WM_POINTERUPDATE, 7, x + 50, y);
            touch(WM_POINTERUP, 7, x + 50, y);
        };
        for (unsigned preset = 0; preset < 3; ++preset) {
            control.requestId++;
            control.payload.kind = runtime::CanvasControlPayloadKind::kSelectCreationPreset;
            control.payload.creationPreset = static_cast<runtime::CanvasCreationPreset>(preset);
            assert(state.facade->submitCanvasControl(control).state == runtime::ProductControlState::kQueued);
            assert(composition.ownerPump());
            const int y = 100 + static_cast<int>(preset) * 130;
            SendMessageW(owner, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(430, y));
            SendMessageW(owner, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(630, y + 80));
            SendMessageW(owner, WM_LBUTTONUP, 0, MAKELPARAM(630, y + 80));
            choose(runtime::CanvasToolKind::kSelect);
            int centerX = 530;
            tap(centerX, y + 40, 600);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            const auto selected = composition.qualificationRuntime().selectedPrimaryObject();
            assert(!selected.isZero());
            const auto generation = composition.controlFacade().readRuntimeState().identity.documentRevision;
            // The tap that selected the Shape plus the next rapid tap enters
            // its label editor; touch must not depend on promoted mouse clicks.
            if (preset == 0) Sleep(600); // Processing delay does not alter source event facts.
            tap(centerX, y + 40, 120);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
            assert(state.facade->readTextEditor().token.object == selected);
            assert(GetFocus() == owner);
            assert(composition.semanticObjectCount() == preset + 1);
            assert(composition.controlFacade().readRuntimeState().identity.documentRevision == generation);
            SendMessageW(owner, WM_CHAR, 'A', 0);
            SendMessageW(owner, WM_CHAR, 'B', 0);
            pumpWindowsText(state);
            textIntent(state, runtime::TextIntentKind::kFinish);
            pumpWindowsText(state);
            tap(centerX, y + 40, 600);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            tap(centerX, y + 40, 120);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
            textIntent(state, runtime::TextIntentKind::kFinish);
            pumpWindowsText(state);
            tap(centerX, y + 40, 600);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            tap(centerX, y + 40, 600);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            touch(WM_POINTERDOWN, 7, centerX, y + 40, 600);
            touch(WM_POINTERDOWN, 8, centerX + 5, y + 40);
            touch(WM_POINTERUP, 7, centerX, y + 40);
            touch(WM_POINTERUP, 8, centerX + 5, y + 40);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            tap(centerX, y + 40, 600);
            touch(WM_POINTERDOWN, 7, centerX, y + 40, 80);
            SendMessageW(owner, WM_POINTERCAPTURECHANGED, 7, 0);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            tap(centerX, y + 40, 600);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            drag(centerX, y + 40);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            centerX += 50;
            tap(centerX, y + 40, 600);
            tap(centerX + 2, y + 41, 120);
            const auto reedit = state.facade->readTextEditor();
            assert(reedit.phase == runtime::TextPhase::kEditing);
            assert(state.facade->readTextEditor().projection == u"AB");
            assert(state.facade->readTextGeometry().available);
            textIntent(state, runtime::TextIntentKind::kFinish);
            pumpWindowsText(state);

            tap(centerX, y + 40, 600);
            drag(centerX, y + 40);
            centerX += 50;
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            assert(composition.qualificationRuntime().selectedPrimaryObject() == selected);
            tap(centerX, y + 40, 600);
            tap(centerX + 2, y + 41, 120);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
            textIntent(state, runtime::TextIntentKind::kFinish);
            pumpWindowsText(state);

            tap(centerX, y + 40, 600);
            tap(centerX, y + 40, 600);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            touch(WM_POINTERDOWN, 7, centerX, y + 40, 80);
            touch(WM_POINTERDOWN, 8, centerX + 5, y + 40);
            touch(WM_POINTERUP, 7, centerX, y + 40);
            touch(WM_POINTERUP, 8, centerX + 5, y + 40);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            tap(centerX, y + 40, 600);
            tap(centerX + 2, y + 41, 120);
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kEditing);
            textIntent(state, runtime::TextIntentKind::kFinish);
            pumpWindowsText(state);
            std::printf("Touch double-tap Shape %u: empty entry, reentry, focus and no extra commit pass\n", preset);
        }
        DestroyWindow(owner);
        UnregisterClassW(klass.lpszClassName, instance);
        return 0;
    }
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
            SendMessageW(owner, WM_CHAR, 'C', 0);
            pumpWindowsText(state);
            assert(state.facade->readTextEditor().projection.size() == 3);
            runtime::RuntimeFrameOpportunity frame;
            assert(composition.serviceFrameOpportunity(false, frame));
            const auto geometry = state.facade->readTextGeometry();
            assert(geometry.available);
            const auto centerY = y + (preset == 2 ? 60 : 40);
            assert(geometry.caret[1] > centerY - 18 && geometry.caret[5] < centerY + 18);
            std::vector<std::uint8_t> previewPixels(800 * 600 * 4);
            assert(composition.readPreviewRgba(previewPixels));
            for (int py = y + 20; py < y + 60; ++py)
                for (int px = 500; px < 560; ++px)
                    assert(previewPixels[(py * 800 + px) * 4 + 3] == 0);
            textIntent(state, runtime::TextIntentKind::kFinish);
            pumpWindowsText(state);
            assert(state.facade->readTextEditor().formatObject == shape.id);
            runtime::TextIntent format;
            format.clientId = 0xFC026;
            format.requestId = preset + 1;
            format.target = control.target;
            format.kind = runtime::TextIntentKind::kFormat;
            format.object = shape.id;
            format.format.bold = true;
            format.format.italic = true;
            format.format.fontSize = 24;
            assert(state.facade->submitTextIntent(format).status == runtime::TextStatus::kQueued);
            assert(composition.ownerPump());
            assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
            assert(state.facade->readTextEditor().format.bold.value_or(false));
            assert(state.facade->readTextEditor().format.italic.value_or(false));
            assert(state.facade->readTextEditor().format.fontSize == 24);
            std::printf("Shape preset %u: live reedit pixels/caret and whole-label Facade format pass\n", preset);
        }
        state.debugUi = std::make_unique<debug_ui::WindowsDebugUiHost>();
        assert(state.debugUi->initialize(owner));
        state.controller = std::make_unique<debug_ui::DebugController>(
            debug_ui::DebugControllerContext{state.facade});
        state.controller->uiState().setWorkspace(debug_ui::WorkspaceId::kControl);
        state.debugUi->setController(state.controller.get());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(410, 560);
        io.DeltaTime = 1.0F / 60.0F;
        for (int i = 0; i < 2; ++i) {
            ImGui::NewFrame();
            (void)state.controller->buildImGuiFrame();
            ImGui::Render();
        }
        auto& gui = *ImGui::GetCurrentContext();
        ImGuiWindow* content = nullptr;
        for (auto* window : gui.Windows)
            if (std::string_view(window->Name).find("/##content_") != std::string_view::npos)
                content = window;
        assert(content);
        // Exercise the real widget -> Router -> Facade -> selected Shape path,
        // with the text editor closed and Italic as an unchanged sibling leaf.
        const auto beforeWidget = state.facade->readTextEditor();
        assert(!beforeWidget.formatObject.isZero());
        assert(beforeWidget.format.bold.value_or(false));
        ImGui::NewFrame();
        gui.NavActivateId = gui.NavActivateDownId = content->GetID("Bold");
        gui.NavInputSource = ImGuiInputSource_Keyboard;
        assert(state.controller->buildImGuiFrame());
        ImGui::Render();
        assert(composition.ownerPump());
        assert(state.facade->readTextEditor().phase == runtime::TextPhase::kClosed);
        assert(!state.facade->readTextEditor().format.bold.value_or(true));
        assert(state.facade->readTextEditor().format.italic.value_or(false));
        std::puts("Real Debug UI Bold widget: selected Shape label updated without an editor session");
        state.debugUi->shutdown();
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
        debug_ui::DebugControllerContext{state.facade});
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
