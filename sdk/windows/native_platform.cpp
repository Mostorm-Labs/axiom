#include "native_platform.hpp"

#include <algorithm>
#include <array>
#include <canvas/platform/windows_runtime_adapter.hpp>
#include <cmath>
#include <cstdio>
#include <deque>
#include <imm.h>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <windowsx.h>
#define AXIOM_MINIMAL_HAS_DEBUG_UI 0
namespace canvas::sdk {
namespace {
std::mutex classMutex;
unsigned classUsers = 0;
HMODULE module() {
    HMODULE h = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&module),
                            &h))
        throw std::runtime_error("DLL module unavailable");
    return h;
}
std::wstring className() {
    return L"AxiomSdkCanvasV1_" + std::to_wstring(reinterpret_cast<UINT_PTR>(module()));
}
struct HostRef {
    runtime::RuntimeComposition* owner;
    runtime::RuntimeComposition& publicComposition() {
        return *owner;
    }
    bool registerSource(input::InputSourceId id) {
        return owner->registerSource(id);
    }
    bool unregisterSource(input::InputSourceId id, uint64_t seq, uint64_t time) {
        return owner->unregisterSource(id, seq, time);
    }
};
#include "native_collectors.inl"
void service(State& s) {
    if (!s.facade || s.pumping || s.inCallback)
        return;
    s.pumping = true;
    struct Reset {
        bool& value;
        ~Reset() {
            value = false;
        }
    } reset{s.pumping};
    pumpWindowsText(s);
    if (s.platformAdapter && s.platformAdapter->prepareFrame(false))
        (void)s.platformAdapter->present();
}
LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        s = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);
        s->window = w;
        SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
    }
    if (!s)
        return DefWindowProcW(w, m, a, b);
    // No C++ exception may escape a Windows callback. Failure stays observable.
    try {
        if (m == WM_LBUTTONDBLCLK && isPromotedPointerMouse())
            return 0;
        if (s->inCallback &&
            ((m >= WM_KEYFIRST && m <= WM_KEYLAST) || (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) ||
             (m >= WM_POINTERUPDATE && m <= WM_POINTERUP) ||
             (m >= WM_IME_STARTCOMPOSITION && m <= WM_IME_COMPOSITION) || m==WM_IME_CHAR))
            return 0;
        const bool hostFocus = m == WM_KILLFOCUS && a &&
                               (reinterpret_cast<HWND>(a) == GetParent(w) ||
                                IsChild(GetParent(w), reinterpret_cast<HWND>(a)));
        if (hostFocus && s->facade)
            textIntent(*s, runtime::TextIntentKind::kCompleteComposition);
        else if (auto result = windowsTextMessage(*s, m, a, b))
            return *result;
        switch (m) {
        case WM_TIMER:
            service(*s);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(w, &paint);
            if (s->platformAdapter)
                (void)s->platformAdapter->present();
            EndPaint(w, &paint);
            return 0;
        }
        case WM_SIZE:
            if (s->platformAdapter && LOWORD(b) && HIWORD(b)) {
                cancelHostPointers(*s, "resize");
                (void)s->platformAdapter->resize(
                    LOWORD(b), HIWORD(b), static_cast<float>(GetDpiForWindow(w)) / 96.0F);
            }
            return 0;
        case WM_WINDOWPOSCHANGED:
        case WM_MOVE:
            if (s->platformAdapter)
                s->platformAdapter->reposition();
            break;
        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP:
            if (sendNativePointer(*s, m, GET_POINTERID_WPARAM(a)))
                return 0;
            break;
        case WM_POINTERCAPTURECHANGED:
            cancelNativePointer(*s, GET_POINTERID_WPARAM(a), "capture-lost");
            return 0;
        case WM_POINTERDEVICECHANGE:
            if (a == PDC_REMOVAL)
                removeNativeDevice(*s, reinterpret_cast<HANDLE>(b));
            return 0;
        case WM_LBUTTONDOWN:
            if (isPromotedPointerMouse())
                return 0;
            SetFocus(w);
            SetCapture(w);
            sendPointer(*s, b, input::PointerPhase::kDown, a);
            if (!s->mouseKey)
                releaseHostMouseCapture(*s);
            return 0;
        case WM_MOUSEMOVE:
            if (isPromotedPointerMouse())
                return 0;
            if (s->mouseKey)
                sendPointer(*s, b, input::PointerPhase::kMove, a);
            else if (s->host) {
                auto metrics = s->host->publicComposition().surfaceMetrics();
                if (metrics && metrics->width && metrics->height)
                    (void)s->host->publicComposition().hoverSelectionAtViewPoint(
                        GET_X_LPARAM(b) * metrics->logicalWidth / metrics->width,
                        GET_Y_LPARAM(b) * metrics->logicalHeight / metrics->height);
            }
            return 0;
        case WM_LBUTTONUP:
            if (isPromotedPointerMouse())
                return 0;
            releaseHostMouseCapture(*s);
            if (s->mouseKey)
                sendPointer(*s, b, input::PointerPhase::kUp, a);
            return 0;
        case WM_CAPTURECHANGED:
            if (!s->releasingPointerUp)
                cancelHostPointers(*s, "capture-lost");
            return 0;
        case WM_KILLFOCUS:
        case WM_CANCELMODE:
            cancelHostPointers(*s, "focus-lost");
            break;
        case WM_ACTIVATEAPP:
            if (!a)
                cancelHostPointers(*s, "deactivated");
            break;
        case WM_CLOSE:
            return 0; // embedding application owns controlled close
        case WM_NCDESTROY:
            KillTimer(w, 1);
            SetWindowLongPtrW(w, GWLP_USERDATA, 0);
            s->window = nullptr;
            break;
        }
    } catch (...) {
        s->textFault = true;
        return 0;
    }
    return DefWindowProcW(w, m, a, b);
}
class Platform final : public NativePlatform {
    State state_;
    HWND parent_;
    std::filesystem::path assets_;
    HMODULE module_ = module();
    std::wstring name_ = className();
    bool registered_ = false;
    bool publicationClosing_ = false;

  public:
    Platform(HWND parent, const std::filesystem::path& assets) : parent_(parent), assets_(assets) {}
    ~Platform() override {
        release();
    }
    HWND window() const noexcept override {
        return state_.window;
    }
    void inCallback(bool value) override {
        state_.inCallback = value || publicationClosing_;
    }
    void pauseInputForClose() override { publicationClosing_=true;state_.inCallback=true; }
    Json open(runtime::RuntimeComposition& owner, const Json&) override {
        std::unique_lock lock(classMutex);
        WNDCLASSW cls{};
        cls.style = CS_DBLCLKS;
        cls.lpfnWndProc = proc;
        cls.hInstance = module_;
        cls.lpszClassName = name_.c_str();
        cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return {{"ok", false}, {"code", "WINDOW_CLASS_FAILED"}};
        ++classUsers;
        registered_ = true;
        state_.host = std::make_unique<HostRef>(HostRef{&owner});
        RECT r{};
        GetClientRect(parent_, &r);
        auto w = CreateWindowExW(0,
                                 cls.lpszClassName,
                                 L"",
                                 WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                 0,
                                 0,
                                 r.right,
                                 r.bottom,
                                 parent_,
                                 nullptr,
                                 cls.hInstance,
                                 &state_);
        lock.unlock();
        if (!w)
            return {{"ok", false}, {"code", "WINDOW_FAILED"}};
        state_.facade = &owner.controlFacade();
        state_.platformAdapter = std::make_unique<platform::WindowsRuntimeAdapter>(owner, w);
        auto dpi = GetDpiForWindow(w);
        auto scale = dpi ? static_cast<float>(dpi) / 96.0F : 1.0F;
        if (r.right <= 0 || r.bottom <= 0 ||
            !state_.platformAdapter->bind(
                static_cast<uint32_t>(r.right), static_cast<uint32_t>(r.bottom), scale))
            return {{"ok", false}, {"code", "SURFACE_FAILED"}};
        SetTimer(w, 1, 16, nullptr);
        return {{"ok", true}};
    }
    bool loadFonts() override {
        return state_.platformAdapter &&
               state_.platformAdapter->loadHostFonts((assets_ / L"axiom-font.otf").c_str(),
                                                     (assets_ / L"fonts").c_str());
    }
    bool resize(float w, float h, float scale) override {
        if (!state_.window || !state_.platformAdapter)
            return false;
        auto width = static_cast<int>(std::round(w * scale)),
             height = static_cast<int>(std::round(h * scale));
        if (!SetWindowPos(
                state_.window, nullptr, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOZORDER))
            return false;
        const auto metrics = state_.host->publicComposition().surfaceMetrics();
        return metrics && metrics->available
                   ? state_.platformAdapter->resize(
                         static_cast<uint32_t>(width), static_cast<uint32_t>(height), scale)
                   : state_.platformAdapter->recoverSurface(
                         static_cast<uint32_t>(width), static_cast<uint32_t>(height), scale);
    }
    bool lose() override {
        cancelHostPointers(state_, "surface-lost");
        return state_.platformAdapter && state_.platformAdapter->loseSurface();
    }
    Json frame(bool force) override {
        if (!state_.window || !state_.platformAdapter)
            return {{"ok", false}, {"code", "WINDOW_DESTROYED"}};
        pumpWindowsText(state_);
        bool ok = state_.platformAdapter->prepareFrame(force);
        const auto status = state_.platformAdapter->frameStatus();
        if (ok)
            ok = state_.platformAdapter->present();
        return {{"ok", ok && !state_.textFault},
                {"code", state_.textFault ? "NATIVE_INPUT_FAILED" : "FRAME_REJECTED"},
                {"pending", status.needsPaint},
                {"canonicalWork", std::to_string(status.canonicalWorkId)}};
    }
    std::size_t activeContacts() const noexcept override {
        return state_.nativeContacts.size() + (state_.mouseKey ? 1 : 0);
    }
    bool finishText(bool keep) override {
        if (!state_.facade)
            return true;
        pumpWindowsText(state_);
        auto s = state_.facade->readTextEditor();
        if (keep ? s.composition == 0 : s.phase == runtime::TextPhase::kClosed)
            return true;
        textIntent(state_,
                   keep ? runtime::TextIntentKind::kCompleteComposition
                        : runtime::TextIntentKind::kFinish);
        pumpWindowsText(state_);
        s = state_.facade->readTextEditor();
        return keep ? s.composition == 0 : s.phase == runtime::TextPhase::kClosed;
    }
    bool shutdown() override {
        return state_.platformAdapter && state_.platformAdapter->shutdown();
    }
    void release() noexcept override {
        if (state_.window)
            KillTimer(state_.window, 1);
        if (state_.host)
            cancelHostPointers(state_, "release");
        state_.facade = nullptr;
        state_.platformAdapter.reset();
        if (state_.window)
            DestroyWindow(state_.window);
        state_.host.reset();
        if (registered_) {
            std::lock_guard lock(classMutex);
            registered_ = false;
            if (--classUsers == 0)
                UnregisterClassW(name_.c_str(), module_);
        }
    }
    uint64_t nowNs() const noexcept override {
        return GetTickCount64() * 1000000;
    }
};
} // namespace
std::unique_ptr<NativePlatform> makeNativePlatform(HWND p, const std::filesystem::path& a) {
    return std::make_unique<Platform>(p, a);
}
} // namespace canvas::sdk
