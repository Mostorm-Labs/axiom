#include "host_font_resources.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>

#include <imm.h>

namespace {
std::u16string resultText, composingText;
HIMC observedContext(HWND) {
    return reinterpret_cast<HIMC>(1);
}
BOOL releaseContext(HWND, HIMC) {
    return TRUE;
}
LONG compositionString(HIMC, DWORD kind, void* data, DWORD size) {
    if (kind == GCS_CURSORPOS)
        return static_cast<LONG>(composingText.size());
    const auto& text = kind == GCS_RESULTSTR ? resultText : composingText;
    const auto bytes = static_cast<DWORD>(text.size() * 2);
    if (data && size >= bytes)
        std::memcpy(data, text.data(), bytes);
    return static_cast<LONG>(bytes);
}
BOOL completeComposition(HIMC, DWORD, DWORD, DWORD) {
    return FALSE;
}
BOOL positionComposition(HIMC, LPCOMPOSITIONFORM) {
    return TRUE;
}
BOOL positionCandidate(HIMC, LPCANDIDATEFORM) {
    return TRUE;
}
} // namespace
// Synthetic OS observations; all queue, identity, Facade and Semantic paths
// below are production code. This does not claim a physical IME observation.
#define ImmGetContext observedContext
#define ImmReleaseContext releaseContext
#define ImmGetCompositionStringW compositionString
#define ImmNotifyIME completeComposition
#define ImmSetCompositionWindow positionComposition
#define ImmSetCandidateWindow positionCandidate
#define WinMain AxiomWindowEntryForTextTest
#include "../windows_main.cpp"
#undef WinMain

int main() {
    using namespace canvas::runtime;
    State state;
    std::FILE* log=nullptr;assert(fopen_s(&log,"windows-text-events.log","w")==0);state.inputLog.reset(log);
    state.host = std::make_unique<MinimalRuntimeHost>();
    state.facade = std::make_unique<MinimalRuntimeFacade>(*state.host);
    auto& composition = state.host->composition();
    assert(composition.bindSurface(640, 480));
    assert(canvas::minimal_runtime_host::bindHostTextFont(composition, "axiom-font.otf"));
    auto& facade = *state.facade;
    TextIntent create;
    create.kind = TextIntentKind::kCreate;
    create.clientId = 39;
    create.requestId = 1;
    create.target = facade.mountedCanvasTargets().front();
    assert(facade.submitTextIntent(create).status == TextStatus::kQueued);
    assert(composition.ownerPump());
    (void)windowsTextMessage(state, WM_CHAR, 'A', 0);
    (void)windowsTextMessage(state, WM_CHAR, 'B', 0);
    assert(facade.readTextEditor().projection.empty());
    pumpWindowsText(state);
    assert(facade.readTextEditor().projection == u"AB");
    assert(composition.semanticObjectCount() == 1);
    (void)windowsTextMessage(state, WM_IME_STARTCOMPOSITION, 0, 0);
    composingText = u"preedit";
    (void)windowsTextMessage(state, WM_IME_COMPOSITION, 0, GCS_COMPSTR | GCS_CURSORPOS);
    pumpWindowsText(state);
    assert(facade.readTextEditor().preedit == u"preedit");
    assert(facade.readTextEditor().projection == u"AB");
    resultText = u"C";
    composingText = u"D";
    (void)windowsTextMessage(state, WM_IME_COMPOSITION, 0, GCS_RESULTSTR | GCS_COMPSTR);
    pumpWindowsText(state);
    assert(facade.readTextEditor().projection == u"ABC");
    assert(facade.readTextEditor().preedit == u"D");
    textIntent(state, TextIntentKind::kFinish);
    pumpWindowsText(state);
    pumpWindowsText(state);
    assert(facade.readTextEditor().projection == u"ABC");
    assert(facade.readTextEditor().exitStatus == TextStatus::kRejected);
    assert(facade.readTextEditor().preedit == u"D");
    textIntent(state, TextIntentKind::kEscape);
    pumpWindowsText(state);
    (void)windowsTextMessage(state, WM_IME_ENDCOMPOSITION, 0, 0);
    pumpWindowsText(state);
    (void)windowsTextMessage(state, WM_CHAR, 0xD83D, 0);
    (void)windowsTextMessage(state, WM_CHAR, 0xDE00, 0);
    pumpWindowsText(state);
    assert(facade.readTextEditor().projection == u"ABC\U0001F600");
    std::puts("PASS: queued WM_CHAR, own-projection advancement, preedit isolation, partial result "
              "continuation, native finish failure, UTF16 surrogate pair");
}
