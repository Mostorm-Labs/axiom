struct NativeContact final {
    canvas::input::PointerKey key{};
    POINTER_INFO last{};
};

struct TextFact {
    int kind = 0;
    canvas::runtime::TextInputKind input = canvas::runtime::TextInputKind::kSelection;
    canvas::runtime::TextIntentKind intent = canvas::runtime::TextIntentKind::kFinish;
    std::u16string text;
    std::optional<std::uint32_t> cursor;
    std::optional<canvas::runtime::TextSessionToken> token;
    std::optional<canvas::runtime::TextRange> range;
    float x = 0, y = 0;
    bool afterTerminal = false;
};
struct State final {
    HWND window = nullptr;
    std::deque<TextFact> textFacts;
    std::optional<TextFact> pendingNativeTerminal;
    bool continuationPending = false, ownTerminalAccepted = false;
    bool nativeComposing = false, closeAfterText = false, debugAfterText = false;
    std::uint64_t imeSerial = 0, textRequest = 0, finishEffect = 0, finishSession = 0;
    char16_t highSurrogate = 0;
    canvas::runtime::TextSessionToken highSurrogateToken{};
    canvas::runtime::TextSessionToken nativeTextToken{};
    canvas::runtime::TextRange nativeTextRange{};
    bool textFault = false;
    bool pumping = false;
    bool inCallback = false;
    std::unique_ptr<HostRef> host;
    canvas::runtime::RuntimeFacade* facade = nullptr; // borrowed from host's Composition
#if AXIOM_MINIMAL_HAS_DEBUG_UI
    std::unique_ptr<canvas::debug_ui::DebugController> controller;
    std::unique_ptr<canvas::debug_ui::WindowsDebugUiHost> debugUi;
#endif
    std::uint64_t sequence = 0;
    std::uint64_t timestamp = 0;
    std::uint64_t lastPresentedFrame = 0;
    std::uint64_t lastDebugActivitySequence = 0;
    bool releasingPointerUp = false;
    std::optional<MOUSEMOVEPOINT> lastMousePoint;
    std::optional<canvas::input::PointerKey> mouseKey;
    std::map<std::pair<HANDLE, POINTER_INPUT_TYPE>, canvas::input::InputSourceId> nativeSources;
    canvas::input::InputSourceId nextSource = 4U;
    std::map<UINT32, NativeContact> nativeContacts;
    std::map<std::pair<canvas::input::InputSourceId, UINT32>, POINTER_INFO> nativeWatermarks;
    std::unique_ptr<std::FILE, decltype(&std::fclose)> inputLog{nullptr, &std::fclose};
    std::unique_ptr<canvas::platform::WindowsRuntimeAdapter> platformAdapter;
#if defined(AXIOM_RUNTIME_QUALIFICATION) && AXIOM_RUNTIME_QUALIFICATION
    canvas::minimal_runtime_host::WindowsPresentationCache presentationCache;
    canvas::runtime::RuntimeFrameOpportunity frameOpportunity{};
#endif
};

#include "windows_pointer_input.inl"
#include "windows_text_input.inl"
void sendPointer(State& state,
                 LPARAM lParam,
                 canvas::input::PointerPhase phase,
                 WPARAM keyStates = 0) {
    (void)state.host->publicComposition().ownerPump();
    const auto surface = state.host->publicComposition().surfaceMetrics().value_or(
        canvas::runtime::RuntimeSurfaceMetrics{});
    if (!surface.available || surface.width == 0U || surface.height == 0U ||
        !std::isfinite(surface.logicalWidth) || !std::isfinite(surface.logicalHeight) ||
        surface.logicalWidth <= 0.0F || surface.logicalHeight <= 0.0F) {
        return;
    }
    // WM_MOUSE client coordinates are physical pixels under per-monitor DPI
    // awareness. Runtime consumes logical view coordinates exactly once.
    const auto x = static_cast<float>(GET_X_LPARAM(lParam)) * surface.logicalWidth / surface.width;
    const auto y =
        static_cast<float>(GET_Y_LPARAM(lParam)) * surface.logicalHeight / surface.height;
    canvas::input::PlatformPointerBatch batch;
    const auto messageTime = static_cast<DWORD>(GetMessageTime());
    const auto append = [&](float sampleX,
                            float sampleY,
                            DWORD time,
                            canvas::input::PointerPhase samplePhase,
                            canvas::input::SampleProvenance provenance) {
        // Match Windows' millisecond clock, including its 32-bit wrap, rather
        // than treating a callback counter as elapsed nanoseconds.
        const auto now = GetTickCount64();
        const auto age = static_cast<DWORD>(static_cast<DWORD>(now) - time);
        const auto sampledAt = now >= age ? (now - age) * 1'000'000U : 0U;
        state.timestamp = (std::max)(state.timestamp + 1U, sampledAt);
        batch.samples.push_back({1U,
                                 1U,
                                 ++state.sequence,
                                 state.timestamp,
                                 sampleX,
                                 sampleY,
                                 0.0F,
                                 0.0F,
                                 0.0F,
                                 {},
                                 {},
                                 provenance,
                                 samplePhase});
        if (provenance != canvas::input::SampleProvenance::kCoalescedHistory)
            batch.samples.back().modifiers = (keyStates & MK_SHIFT)
                                                 ? canvas::input::PointerModifiers::kShift
                                                 : canvas::input::PointerModifiers::kNone;
    };
    POINT screen{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const bool screenAvailable = ClientToScreen(state.window, &screen) != FALSE;
    bool currentIncluded = false;
    if (phase == canvas::input::PointerPhase::kDown) {
        state.lastMousePoint.reset();
    } else if (screenAvailable && state.lastMousePoint.has_value()) {
        // WM_MOUSEMOVE coalesces while the UI thread draws. Recover the actual
        // OS observations instead of connecting only the surviving messages.
        std::array<MOUSEMOVEPOINT, 64> history{};
        MOUSEMOVEPOINT query{screen.x & 0xFFFF, screen.y & 0xFFFF, messageTime, 0U};
        const int count = GetMouseMovePointsEx(sizeof(MOUSEMOVEPOINT),
                                               &query,
                                               history.data(),
                                               static_cast<int>(history.size()),
                                               GMMP_USE_DISPLAY_POINTS);
        if (count > 0) {
            const auto signedPoint = [](MOUSEMOVEPOINT point) {
                point.x = static_cast<short>(point.x);
                point.y = static_cast<short>(point.y);
                return point;
            };
            // Locate the last delivered observation before reversing the native
            // newest-first history. This also preserves distinct same-ms samples.
            int end = count;
            for (int i = 0; i < count; ++i) {
                const auto point = signedPoint(history[static_cast<std::size_t>(i)]);
                if (point.time == state.lastMousePoint->time &&
                    point.x == state.lastMousePoint->x && point.y == state.lastMousePoint->y) {
                    end = i;
                    break;
                }
            }
            for (int i = end - 1; i >= 0; --i) {
                const auto point = signedPoint(history[static_cast<std::size_t>(i)]);
                if (static_cast<LONG>(point.time - messageTime) > 0 ||
                    (end == count &&
                     static_cast<LONG>(point.time - state.lastMousePoint->time) <= 0))
                    continue;
                const bool isCurrent =
                    point.time == messageTime && point.x == screen.x && point.y == screen.y;
                // Up is a single terminal observation. Its preceding history is Move.
                if (isCurrent && phase == canvas::input::PointerPhase::kUp)
                    continue;
                POINT client{point.x, point.y};
                if (!ScreenToClient(state.window, &client))
                    continue;
                append(static_cast<float>(client.x) * surface.logicalWidth / surface.width,
                       static_cast<float>(client.y) * surface.logicalHeight / surface.height,
                       point.time,
                       canvas::input::PointerPhase::kMove,
                       isCurrent ? canvas::input::SampleProvenance::kConfirmedCurrent
                                 : canvas::input::SampleProvenance::kCoalescedHistory);
                currentIncluded = currentIncluded || isCurrent;
            }
        }
    }
    if (!currentIncluded &&
        (phase != canvas::input::PointerPhase::kMove || !state.lastMousePoint ||
         state.lastMousePoint->time != messageTime || state.lastMousePoint->x != screen.x ||
         state.lastMousePoint->y != screen.y)) {
        append(x, y, messageTime, phase, canvas::input::SampleProvenance::kConfirmedCurrent);
    }
    if (screenAvailable)
        state.lastMousePoint = MOUSEMOVEPOINT{screen.x, screen.y, messageTime, 0U};
    if (phase == canvas::input::PointerPhase::kUp || phase == canvas::input::PointerPhase::kCancel)
        state.lastMousePoint.reset();
    if (batch.samples.empty())
        return;
    const auto before = state.mouseKey;
    if (phase == canvas::input::PointerPhase::kDown)
        state.mouseKey = state.host->publicComposition().beginContact(batch);
    const bool accepted = phase == canvas::input::PointerPhase::kDown
                              ? state.mouseKey.has_value()
                              : state.mouseKey && state.host->publicComposition().submitContact(
                                                      *state.mouseKey, batch);
    logInput(state,
             "mouse",
             accepted,
             state.mouseKey ? *state.mouseKey : before.value_or(canvas::input::PointerKey{}),
             &batch);
    if (!accepted && state.mouseKey) {
        cancelKey(state, *state.mouseKey, "mouse-rejected");
        state.lastMousePoint.reset();
    }
    if (!accepted || phase == canvas::input::PointerPhase::kUp ||
        phase == canvas::input::PointerPhase::kCancel)
        state.mouseKey.reset();
    InvalidateRect(state.window, nullptr, FALSE);
}
