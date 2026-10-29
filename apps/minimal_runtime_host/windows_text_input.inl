// Native callbacks enqueue observations; this owner pump alone enters RuntimeFacade.
void logTextInput(State& state,
                  int kind,
                  const canvas::runtime::TextSessionToken& token,
                  std::uint64_t sequence,
                  canvas::runtime::TextStatus status) {
    if (!state.inputLog)
        return;
    const auto id = [](canvas::foundation::ObjectId value) {
        std::string out;
        constexpr char digits[] = "0123456789abcdef";
        for (auto b : value.bytes) {
            out += digits[b >> 4];
            out += digits[b & 15];
        }
        return out;
    };
    const auto current = state.facade->readTextEditor();
    std::fprintf(state.inputLog.get(),
                 "text kind=%d status=%u sequence=%llu runtime=%llu attachment=%llu view=%llu "
                 "adapter=%llu session=%llu projection=%llu document=%s object=%s phase=%u "
                 "composition=%llu preedit_units=%zu confirmed_units=%zu effect=%llu\n",
                 kind,
                 static_cast<unsigned>(status),
                 static_cast<unsigned long long>(sequence),
                 static_cast<unsigned long long>(token.runtimeInstance),
                 static_cast<unsigned long long>(token.attachment),
                 static_cast<unsigned long long>(token.view),
                 static_cast<unsigned long long>(token.adapter),
                 static_cast<unsigned long long>(token.session),
                 static_cast<unsigned long long>(token.projection),
                 id(token.document).c_str(),
                 id(token.object).c_str(),
                 static_cast<unsigned>(current.phase),
                 static_cast<unsigned long long>(current.composition),
                 current.preedit.size(),
                 current.projection.size(),
                 static_cast<unsigned long long>(current.finishEffect));
    logInput(state,
             "text-owner",
             status == canvas::runtime::TextStatus::kApplied ||
                 status == canvas::runtime::TextStatus::kObserved,
             {});
}

void textFact(State& state, TextFact fact) {
    if (fact.kind != 1 && state.continuationPending)
        fact.afterTerminal = true;
    if (!fact.token && state.facade) {
        const auto s = state.facade->readTextEditor();
        fact.token = state.nativeComposing ? state.nativeTextToken : s.token;
    }
    if (state.textFacts.size() >= 256 && fact.input == canvas::runtime::TextInputKind::kUpdate &&
        !state.textFacts.empty() && state.textFacts.back().input == fact.input) {
        state.textFacts.back() = std::move(fact);
        return;
    }
    if (state.textFacts.size() >= 257) {
        state.textFault = true;
        if ((fact.input == canvas::runtime::TextInputKind::kCommit ||
             fact.input == canvas::runtime::TextInputKind::kCancel || fact.kind == 2) &&
            !state.pendingNativeTerminal)
            state.pendingNativeTerminal = std::move(fact);
        return;
    }
    state.textFacts.push_back(std::move(fact));
}
void textIntent(State& state, canvas::runtime::TextIntentKind kind) {
    TextFact fact;
    fact.kind = 1;
    fact.intent = kind;
    textFact(state, std::move(fact));
}
#if AXIOM_MINIMAL_HAS_DEBUG_UI
bool admitWindowsDebugFocus(State& state) {
    const auto s = state.facade->readTextEditor();
    if (!s.composition)
        return true;
    textIntent(state, canvas::runtime::TextIntentKind::kCompleteComposition);
    return false;
}
#endif
bool finishWindowsText(State& state) {
    const auto value = state.facade->readTextEditor();
    if (value.phase == canvas::runtime::TextPhase::kClosed)
        return true;
    textIntent(state, canvas::runtime::TextIntentKind::kFinish);
    return false;
}
void pumpWindowsText(State& state) {
    using namespace canvas::runtime;
    if (!state.facade)
        return;
    auto& facade = *state.facade;
    auto& composition = state.host->publicComposition();
    std::vector<std::pair<TextSessionToken, TextSessionToken>> accepted;
    for (std::size_t count = 0; count < 258 && !state.textFacts.empty(); ++count) {
        auto fact = state.textFacts.front();
        if (fact.afterTerminal) {
            if (!state.ownTerminalAccepted) {
                state.textFault = true;
                state.textFacts.pop_front();
                continue;
            }
            fact.token = state.nativeTextToken;
            fact.range = state.nativeTextRange;
        }
        const auto snapshot = facade.readTextEditor();
        if (fact.token && fact.input == TextInputKind::kReplace)
            for (const auto& alias : accepted)
                if (*fact.token == alias.first && snapshot.token == alias.second)
                    fact.token = alias.second;
        if (snapshot.phase == TextPhase::kClosed &&
            !(fact.kind == 1 && fact.intent == TextIntentKind::kEditAtPoint)) {
            state.textFacts.pop_front();
            continue;
        }
        if (fact.kind == 1) {
            TextIntent intent;
            intent.kind = fact.intent;
            intent.viewX = fact.x;
            intent.viewY = fact.y;
            intent.clientId = 0xFC0202;
            intent.requestId = ++state.textRequest;
            intent.target = facade.mountedCanvasTargets().front();
            intent.token = snapshot.token;
            const auto queued = facade.submitTextIntent(intent);
            if (queued.status == TextStatus::kBackpressure)
                break;
            (void)composition.ownerPump();
            const auto receipt = facade.textIntentReceipt(intent.clientId, intent.requestId);
            logTextInput(state,
                         100 + static_cast<int>(intent.kind),
                         intent.token,
                         intent.requestId,
                         receipt ? receipt->status : queued.status);
        } else {
            if (fact.kind == 2 && !snapshot.composition) {
                state.textFacts.pop_front();
                continue;
            }
            TextInputEvent event;
            event.token = fact.token.value_or(snapshot.token);
            event.sequence = snapshot.nextInputSequence;
            event.kind = fact.kind == 2 ? TextInputKind::kCancel : fact.input;
            event.text = std::move(fact.text);
            event.range = fact.range.value_or(snapshot.composition ? snapshot.replacement
                                                                   : snapshot.selection);
            event.composition =
                event.kind == TextInputKind::kBegin ? ++state.imeSerial : snapshot.composition;
            if (event.kind == TextInputKind::kCommit || event.kind == TextInputKind::kFinishFailed)
                event.finishEffect = snapshot.finishEffect;
            if (event.kind == TextInputKind::kBegin)
                state.continuationPending = false;
            if (fact.cursor)
                event.preeditSelection = TextRange{*fact.cursor, 0};
            const auto queued = facade.submitTextInput(event);
            if (queued.status == TextStatus::kBackpressure)
                break;
            (void)composition.ownerPump();
            const auto receipt = facade.textInputReceipt(event.token, event.sequence);
            logTextInput(state,
                         static_cast<int>(event.kind),
                         event.token,
                         event.sequence,
                         receipt ? receipt->status : queued.status);
            if (receipt && receipt->status == TextStatus::kApplied &&
                event.kind == TextInputKind::kReplace) {
                for (auto& alias : accepted)
                    if (alias.second == event.token)
                        alias.second = receipt->token;
                accepted.emplace_back(event.token, receipt->token);
            }
            if (receipt &&
                (receipt->status == TextStatus::kApplied ||
                 receipt->status == TextStatus::kObserved) &&
                event.kind == TextInputKind::kCommit) {
                state.ownTerminalAccepted = true;
                state.nativeTextToken = receipt->token;
                state.nativeTextRange = facade.readTextEditor().selection;
            } else if (receipt && receipt->status == TextStatus::kRejected) {
                state.textFault = true;
                if (event.kind == TextInputKind::kCommit)
                    state.ownTerminalAccepted = false;
            }
            if (receipt && receipt->status != TextStatus::kQueued &&
                receipt->status != TextStatus::kRejected)
                (void)facade.acknowledgeTextInput(event.token, event.sequence);
        }
        state.textFacts.pop_front();
        if (state.textFacts.empty() && state.pendingNativeTerminal) {
            state.textFacts.push_back(std::move(*state.pendingNativeTerminal));
            state.pendingNativeTerminal.reset();
        }
    }
    const auto snapshot = facade.readTextEditor();
    if (snapshot.finishEffect && (snapshot.finishEffect != state.finishEffect ||
                                  snapshot.token.session != state.finishSession)) {
        state.finishEffect = snapshot.finishEffect;
        state.finishSession = snapshot.token.session;
        const auto context = ImmGetContext(state.window);
        const bool completed = context && ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
        if (context)
            ImmReleaseContext(state.window, context);
        if (!completed) {
            TextFact fact;
            fact.input = TextInputKind::kFinishFailed;
            textFact(state, std::move(fact));
        }
    }
    if (snapshot.phase != TextPhase::kClosed) {
        const auto geometry = facade.readTextGeometry();
        if (geometry.available && geometry.token.session == snapshot.token) {
            const auto dpi = static_cast<float>(GetDpiForWindow(state.window)) / 96.0F;
            const auto context = ImmGetContext(state.window);
            if (context) {
                COMPOSITIONFORM form{};
                form.dwStyle = CFS_POINT;
                form.ptCurrentPos = {static_cast<LONG>(geometry.caret[0] * dpi),
                                     static_cast<LONG>(geometry.caret[1] * dpi)};
                ImmSetCompositionWindow(context, &form);
                CANDIDATEFORM candidate{};
                candidate.dwStyle = CFS_CANDIDATEPOS;
                candidate.ptCurrentPos = {static_cast<LONG>(geometry.caret[6] * dpi),
                                          static_cast<LONG>(geometry.caret[7] * dpi)};
                ImmSetCandidateWindow(context, &candidate);
                ImmReleaseContext(state.window, context);
            }
        }
    } else {
        if (state.closeAfterText) {
            state.closeAfterText = false;
            PostMessageW(state.window, WM_CLOSE, 0, 0);
        }
#if AXIOM_MINIMAL_HAS_DEBUG_UI
        if (state.debugAfterText && state.debugUi) {
            state.debugAfterText = false;
            state.debugUi->toggle();
        }
#endif
    }
#if AXIOM_MINIMAL_HAS_DEBUG_UI
    if (state.debugAfterText && !snapshot.composition &&
        snapshot.exitStatus != TextStatus::kRejected && state.debugUi) {
        state.debugAfterText = false;
        state.debugUi->toggle();
    }
#endif
    if (!facade.readTextGeometry().diagnostics.empty())
        SetWindowTextW(state.window,
                       L"Axiom - exact font glyph unavailable; text retained, display blocked");
    else if (snapshot.phase == TextPhase::kCommitFailed ||
             snapshot.exitStatus == TextStatus::kRejected || state.textFault)
        SetWindowTextW(state.window,
                       L"Axiom Minimal Runtime Host - text completion rejected / retry or Esc");
    else
        SetWindowTextW(
            state.window,
            L"Axiom Minimal Runtime Host - T: Text / Enter: Finish / Ctrl+Enter: Paragraph");
}
std::optional<LRESULT> windowsTextMessage(State& state, UINT message, WPARAM key, LPARAM flags) {
    using namespace canvas::runtime;
    if (!state.facade)
        return {};
    const auto snapshot = state.facade->readTextEditor();
    const bool active = snapshot.phase != TextPhase::kClosed;
    if (message == WM_LBUTTONDBLCLK) {
        const auto targets=state.facade->mountedCanvasTargets();
        if(targets.empty())return 0;
        const auto tool=state.facade->readCanvasControlSnapshot(targets.front()).tool;
        if(tool!=CanvasToolKind::kText && tool!=CanvasToolKind::kSelect)return 0;
        if (active) {
            textIntent(state, TextIntentKind::kFinish);
            if (snapshot.composition)
                return 0;
        }
        const auto metrics = state.host->composition().surfaceSnapshot();
        if (!metrics.width || !metrics.height)
            return 0;
        TextFact fact;
        fact.kind = 1;
        fact.intent = TextIntentKind::kEditAtPoint;
        fact.x = static_cast<float>(GET_X_LPARAM(flags)) * metrics.logicalWidth / metrics.width;
        fact.y = static_cast<float>(GET_Y_LPARAM(flags)) * metrics.logicalHeight / metrics.height;
        textFact(state, std::move(fact));
        return 0;
    }
    if (message == WM_KEYDOWN && key == 'T' && !active && !(GetKeyState(VK_CONTROL) & 0x8000)) {
        CanvasControlRequest request;
        request.clientId = 0xFC0203;
        request.requestId = ++state.textRequest;
        request.target = state.facade->mountedCanvasTargets().front();
        request.payload.kind = CanvasControlPayloadKind::kSelectTool;
        request.payload.tool = CanvasToolKind::kText;
        (void)state.facade->submitCanvasControl(request);
        return 0;
    }
    if (!active)
        return {};
    if (message == WM_IME_SETCONTEXT)
        return DefWindowProcW(state.window, message, key, flags & ~ISC_SHOWUICOMPOSITIONWINDOW);
    if (message == WM_IME_STARTCOMPOSITION) {
        state.nativeComposing = true;
        state.nativeTextToken = snapshot.token;
        state.nativeTextRange = snapshot.selection;
        TextFact fact;
        fact.range = state.nativeTextRange;
        fact.input = TextInputKind::kBegin;
        textFact(state, std::move(fact));
        return 0;
    }
    if (message == WM_IME_COMPOSITION) {
        const auto context = ImmGetContext(state.window);
        if (!context)
            return 0;
        const auto read = [&](DWORD what) -> std::optional<std::u16string> {
            const auto bytes = ImmGetCompositionStringW(context, what, nullptr, 0);
            if (bytes < 0 || bytes % 2 || bytes > 131072)
                return {};
            std::u16string value(static_cast<std::size_t>(bytes) / 2, u'\0');
            if (bytes && ImmGetCompositionStringW(
                             context, what, value.data(), static_cast<DWORD>(bytes)) != bytes)
                return {};
            return value;
        };
        if (flags & GCS_RESULTSTR) {
            if (auto result = read(GCS_RESULTSTR)) {
                TextFact fact;
                fact.input = TextInputKind::kCommit;
                fact.range = state.nativeTextRange;
                fact.text = std::move(*result);
                textFact(state, std::move(fact));
            }
        }
        if (flags & GCS_COMPSTR) {
            if (auto value = read(GCS_COMPSTR)) {
                if ((flags & GCS_RESULTSTR) && !value->empty()) {
                    state.continuationPending = true;
                    TextFact begin;
                    begin.afterTerminal = true;
                    begin.input = TextInputKind::kBegin;
                    textFact(state, std::move(begin));
                }
                TextFact fact;
                fact.input = TextInputKind::kUpdate;
                fact.range = state.nativeTextRange;
                fact.text = std::move(*value);
                const auto cursor = ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0);
                if (cursor >= 0 && static_cast<std::size_t>(cursor) <= fact.text.size())
                    fact.cursor = static_cast<std::uint32_t>(cursor);
                textFact(state, std::move(fact));
            }
        }
        if (!flags) {
            TextFact cancel;
            cancel.kind = 2;
            textFact(state, std::move(cancel));
        }
        ImmReleaseContext(state.window, context);
        return 0;
    }
    if (message == WM_IME_ENDCOMPOSITION) {
        state.nativeComposing = false;
        TextFact end;
        end.kind = 2;
        textFact(state, std::move(end));
        return 0;
    }
    if (message == WM_IME_CHAR)
        return 0;
    if (message == WM_CHAR) {
        if (state.nativeComposing)
            return 0;
        char16_t value = static_cast<char16_t>(key);
        if (value < 32)
            return 0;
        if (value >= 0xD800 && value <= 0xDBFF) {
            state.highSurrogate = value;
            state.highSurrogateToken=snapshot.token;
            return 0;
        }
        TextFact fact;
        fact.input = TextInputKind::kReplace;
        if (state.highSurrogate) {
            if(state.highSurrogateToken==snapshot.token)fact.text.push_back(state.highSurrogate);
            state.highSurrogate = 0;
        }
        fact.text.push_back(value);
        textFact(state, std::move(fact));
        return 0;
    }
    if (message == WM_KEYDOWN && !state.nativeComposing && key != VK_PROCESSKEY) {
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (key == VK_RETURN) {
            textIntent(state, control ? TextIntentKind::kInsertParagraph : TextIntentKind::kFinish);
            return 0;
        }
        if (key == VK_ESCAPE) {
            textIntent(state, TextIntentKind::kEscape);
            return 0;
        }
        if (key == VK_BACK) {
            textIntent(state, TextIntentKind::kBackspace);
            return 0;
        }
        if (key == VK_DELETE) {
            textIntent(state, TextIntentKind::kDelete);
            return 0;
        }
        if (key == VK_LEFT || key == VK_RIGHT) {
            textIntent(state, key == VK_LEFT ? TextIntentKind::kLeft : TextIntentKind::kRight);
            return 0;
        }
        if (control && key == 'A') {
            textIntent(state, TextIntentKind::kSelectAll);
            return 0;
        }
    }
    if (message == WM_KILLFOCUS) {
#if AXIOM_MINIMAL_HAS_DEBUG_UI
        if (state.debugUi && reinterpret_cast<HWND>(key) == state.debugUi->nativeWindow())
            textIntent(state, TextIntentKind::kCompleteComposition);
        else
#endif
            (void)finishWindowsText(state);
    }
    return {};
}
