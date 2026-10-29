#include "minimal_runtime_host.hpp"
#include "minimal_runtime_facade.hpp"
#include "windows_presentation_cache.hpp"
#if AXIOM_MINIMAL_HAS_DEBUG_UI
#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/windows_host.hpp"
#include "windows_image_file.hpp"
#endif
#include "canvas/platform/windows_runtime_adapter.hpp"

#include <windows.h>
#include <windowsx.h>
#include <imm.h>
#include <deque>
#include <filesystem>
#include <share.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using canvas::minimal_runtime_host::MinimalRuntimeHost;

constexpr wchar_t kWindowClass[] = L"AxiomMinimalRuntimeHost";
constexpr UINT_PTR kTimer = 1U;
constexpr UINT kTimerMs = 16U;
constexpr UINT kOverlaySyncMessage = WM_APP + 1U;

using canvas::minimal_runtime_host::MinimalRuntimeFacade;

struct NativeContact final {
  canvas::input::PointerKey key{};
  POINTER_INFO last{};
};

struct TextFact { int kind=0; canvas::runtime::TextInputKind input=canvas::runtime::TextInputKind::kSelection; canvas::runtime::TextIntentKind intent=canvas::runtime::TextIntentKind::kFinish; std::u16string text; std::optional<std::uint32_t> cursor; std::optional<canvas::runtime::TextSessionToken> token; std::optional<canvas::runtime::TextRange> range; float x=0,y=0; bool afterTerminal=false; };
struct State final {
  HWND window = nullptr;
  std::deque<TextFact> textFacts;
  std::optional<TextFact> pendingNativeTerminal;
  bool continuationPending=false,ownTerminalAccepted=false;
  bool nativeComposing=false, closeAfterText=false, debugAfterText=false;
  std::uint64_t imeSerial=0,textRequest=0,finishEffect=0,finishSession=0;
  char16_t highSurrogate=0;
  canvas::runtime::TextSessionToken highSurrogateToken{};
  canvas::runtime::TextSessionToken nativeTextToken{};
  canvas::runtime::TextRange nativeTextRange{};
  bool textFault=false;
  std::unique_ptr<MinimalRuntimeHost> host;
  std::unique_ptr<MinimalRuntimeFacade> facade;
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
  canvas::input::InputSourceId nextSource = 2U;
  std::map<UINT32, NativeContact> nativeContacts;
  std::map<std::pair<canvas::input::InputSourceId, UINT32>, POINTER_INFO> nativeWatermarks;
  std::unique_ptr<std::FILE, decltype(&std::fclose)> inputLog{nullptr, &std::fclose};
  std::unique_ptr<canvas::platform::WindowsRuntimeAdapter> platformAdapter;
  canvas::minimal_runtime_host::WindowsPresentationCache presentationCache;
  canvas::runtime::RuntimeFrameOpportunity frameOpportunity{};
};

#include "windows_pointer_input.inl"
#include "windows_text_input.inl"

canvas::render::SurfaceMetrics clientMetrics(HWND window, std::uint32_t width,
                                             std::uint32_t height) noexcept {
  const auto dpi = GetDpiForWindow(window);
  const auto scale = dpi == 0U ? 1.0F : static_cast<float>(dpi) / 96.0F;
  return {width / scale, height / scale, width, height, scale, scale};
}

void paintCanvas(State& state, HDC dc) {
  if (state.platformAdapter) {
    (void)state.platformAdapter->present();
    return;
  }
  auto& composition = state.host->composition();
  const auto surface = composition.surfaceSnapshot();
  const auto& opportunity = state.frameOpportunity;
  const bool hadPreview = opportunity.previewActive;
  const bool previewRenderedThisPaint = opportunity.previewRendered;
  if (!composition.canonicalProviderAvailable()) return;
  if (surface.width == 0U || surface.height == 0U) return;
  if (composition.canonicalBackend() == canvas::render::RenderTargetBackend::kD3D12) {
    if (const auto& frame = opportunity.canonicalWork; frame.has_value() &&
        frame->frame.frameId.value() != state.lastPresentedFrame) {
      if (composition.receivePlatformPresented({
              *frame, canvas::render::PresentOutcome::kPresented,
              canvas::render::PresentEvidenceKind::kPlatformQualified, std::nullopt})) {
        state.lastPresentedFrame = frame->frame.frameId.value();
      }
    }
    (void)composition.finishFrameOpportunity({opportunity.previewWork, true});
    return;
  }
  const auto byteCount = static_cast<std::size_t>(surface.width) * surface.height * 4U;
  // canonicalFrameCount is a stable content serial across the local submit /
  // platform-feedback transition. Using the transient submitted frame id
  // here would force one unnecessary full readback after every feedback.
  const auto canonicalSerial = composition.canonicalContentSerial();
  const auto surfaceGeneration = surface.surfaceGeneration;
  if (state.presentationCache.shouldReadCanonical(
          surfaceGeneration, canonicalSerial, byteCount)) {
    std::vector<std::uint8_t> pixels(byteCount);
    if (!composition.readCanonicalRgba(pixels)) return;
    state.presentationCache.markCanonicalRead(surfaceGeneration, canonicalSerial,
                                              std::move(pixels));
  }

  const bool previewProviderAvailable = composition.previewProviderAvailable();
  const auto previewGeneration = hadPreview ? opportunity.previewSurfaceGeneration : 0U;
  const auto previewRevision = hadPreview ? opportunity.previewRevision : 0U;
  if (!hadPreview) {
    state.presentationCache.clearPreview();
  } else if (previewProviderAvailable &&
             (previewRenderedThisPaint || state.presentationCache.shouldReadPreview(
                  previewGeneration, previewRevision, byteCount))) {
    std::vector<std::uint8_t> overlay(byteCount);
    if (composition.readPreviewRgba(overlay)) {
      state.presentationCache.markPreviewRead(previewGeneration, previewRevision,
                                              std::move(overlay));
    }
  }

  if (state.presentationCache.shouldRecompose(
          surfaceGeneration, canonicalSerial, previewGeneration, previewRevision,
          hadPreview)) {
    std::vector<std::uint8_t> pixels = state.presentationCache.canonicalPixels();
    const auto& overlay = state.presentationCache.previewPixels();
    if (hadPreview && overlay.size() == pixels.size()) {
      for (std::size_t i = 0; i + 3U < pixels.size(); i += 4U) {
        const float alpha = static_cast<float>(overlay[i + 3U]) / 255.0F;
        if (alpha <= 0.0F) continue;
        for (std::size_t c = 0; c < 3U; ++c) {
          pixels[i + c] = static_cast<std::uint8_t>(
              static_cast<float>(overlay[i + c]) * alpha +
              static_cast<float>(pixels[i + c]) * (1.0F - alpha));
        }
      }
    }
    state.presentationCache.markComposite(
        surfaceGeneration, canonicalSerial, previewGeneration, previewRevision,
        hadPreview, std::move(pixels));
  }
  BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = static_cast<LONG>(surface.width);
  info.bmiHeader.biHeight = -static_cast<LONG>(surface.height);
  info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
  RECT client{}; GetClientRect(state.window, &client);
  StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0,
                static_cast<int>(surface.width), static_cast<int>(surface.height),
                state.presentationCache.compositePixels().data(), &info,
                DIB_RGB_COLORS, SRCCOPY);
  if (const auto& frame = opportunity.canonicalWork; frame.has_value() &&
      frame->frame.frameId.value() != state.lastPresentedFrame) {
    if (composition.receivePlatformPresented({
            *frame, canvas::render::PresentOutcome::kPresented,
            canvas::render::PresentEvidenceKind::kPlatformQualified, std::nullopt})) {
      state.lastPresentedFrame = frame->frame.frameId.value();
    }
  }
  (void)composition.finishFrameOpportunity({opportunity.previewWork, true});
}

void sendPointer(State& state, LPARAM lParam, canvas::input::PointerPhase phase, WPARAM keyStates=0) {
  (void)state.host->composition().ownerPump();
  const auto surface = state.host->composition().surfaceSnapshot();
  if (!surface.available || surface.width == 0U || surface.height == 0U ||
      !std::isfinite(surface.logicalWidth) || !std::isfinite(surface.logicalHeight) ||
      surface.logicalWidth <= 0.0F || surface.logicalHeight <= 0.0F) {
    return;
  }
  // WM_MOUSE client coordinates are physical pixels under per-monitor DPI
  // awareness. Runtime consumes logical view coordinates exactly once.
  const auto x = static_cast<float>(GET_X_LPARAM(lParam)) * surface.logicalWidth / surface.width;
  const auto y = static_cast<float>(GET_Y_LPARAM(lParam)) * surface.logicalHeight / surface.height;
  canvas::input::PlatformPointerBatch batch;
  const auto messageTime = static_cast<DWORD>(GetMessageTime());
  const auto append = [&](float sampleX, float sampleY, DWORD time,
                          canvas::input::PointerPhase samplePhase,
                          canvas::input::SampleProvenance provenance) {
    // Match Windows' millisecond clock, including its 32-bit wrap, rather
    // than treating a callback counter as elapsed nanoseconds.
    const auto now = GetTickCount64();
    const auto age = static_cast<DWORD>(static_cast<DWORD>(now) - time);
    const auto sampledAt = now >= age ? (now - age) * 1'000'000U : 0U;
    state.timestamp = (std::max)(state.timestamp + 1U, sampledAt);
    batch.samples.push_back({1U, 1U, ++state.sequence, state.timestamp,
        sampleX, sampleY, 0.0F, 0.0F, 0.0F, {}, {}, provenance, samplePhase});
    if (provenance!=canvas::input::SampleProvenance::kCoalescedHistory)
      batch.samples.back().modifiers=(keyStates&MK_SHIFT)?canvas::input::PointerModifiers::kShift:canvas::input::PointerModifiers::kNone;
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
    const int count = GetMouseMovePointsEx(sizeof(MOUSEMOVEPOINT), &query,
        history.data(), static_cast<int>(history.size()), GMMP_USE_DISPLAY_POINTS);
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
            (end == count && static_cast<LONG>(point.time - state.lastMousePoint->time) <= 0))
          continue;
        const bool isCurrent = point.time == messageTime &&
            point.x == screen.x && point.y == screen.y;
        // Up is a single terminal observation. Its preceding history is Move.
        if (isCurrent && phase == canvas::input::PointerPhase::kUp) continue;
        POINT client{point.x, point.y};
        if (!ScreenToClient(state.window, &client)) continue;
        append(static_cast<float>(client.x) * surface.logicalWidth / surface.width,
               static_cast<float>(client.y) * surface.logicalHeight / surface.height,
               point.time, canvas::input::PointerPhase::kMove,
               isCurrent ? canvas::input::SampleProvenance::kConfirmedCurrent
                         : canvas::input::SampleProvenance::kCoalescedHistory);
        currentIncluded = currentIncluded || isCurrent;
      }
    }
  }
  if (!currentIncluded && (phase != canvas::input::PointerPhase::kMove ||
      !state.lastMousePoint || state.lastMousePoint->time != messageTime ||
      state.lastMousePoint->x != screen.x || state.lastMousePoint->y != screen.y)) {
    append(x, y, messageTime, phase, canvas::input::SampleProvenance::kConfirmedCurrent);
  }
  if (screenAvailable) state.lastMousePoint = MOUSEMOVEPOINT{screen.x, screen.y, messageTime, 0U};
  if (phase == canvas::input::PointerPhase::kUp || phase == canvas::input::PointerPhase::kCancel)
    state.lastMousePoint.reset();
  if (batch.samples.empty()) return;
  const auto before = state.mouseKey;
  if (phase == canvas::input::PointerPhase::kDown)
    state.mouseKey = state.host->publicComposition().beginContact(batch);
  const bool accepted = phase == canvas::input::PointerPhase::kDown
      ? state.mouseKey.has_value()
      : state.mouseKey && state.host->publicComposition().submitContact(*state.mouseKey, batch);
  logInput(state, "mouse", accepted, state.mouseKey ? *state.mouseKey : before.value_or(canvas::input::PointerKey{}), &batch);
  if (!accepted && state.mouseKey) {
    cancelKey(state, *state.mouseKey, "mouse-rejected");
    state.lastMousePoint.reset();
  }
  if (!accepted || phase == canvas::input::PointerPhase::kUp || phase == canvas::input::PointerPhase::kCancel)
    state.mouseKey.reset();
  InvalidateRect(state.window, nullptr, FALSE);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    state = static_cast<State*>(create->lpCreateParams);
    state->window = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  }
  if (state == nullptr) return DefWindowProcW(window, message, wParam, lParam);
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  if (state->debugUi && state->debugUi->handleMessage(window, message, wParam, lParam)) return 0;
#endif
  if(const auto text=windowsTextMessage(*state,message,wParam,lParam))return *text;
  switch (message) {
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
      if (sendNativePointer(*state, message, GET_POINTERID_WPARAM(wParam))) return 0;
      return DefWindowProcW(window, message, wParam, lParam);
    case WM_POINTERCAPTURECHANGED:
      if (const auto it = state->nativeContacts.find(GET_POINTERID_WPARAM(wParam));
          it != state->nativeContacts.end()) {
        POINTER_INFO info{};
        if (GetPointerInfo(GET_POINTERID_WPARAM(wParam), &info) &&
            (olderNativeObservation(info, it->second.last) ||
             info.sourceDevice != it->second.last.sourceDevice)) return 0;
      }
      cancelNativePointer(*state, GET_POINTERID_WPARAM(wParam), "pointer-capture-lost");
      return 0;
    case WM_POINTERDEVICECHANGE:
      if (wParam == PDC_REMOVAL) removeNativeDevice(*state, reinterpret_cast<HANDLE>(lParam));
      return 0;
    case WM_KILLFOCUS:
    case WM_CANCELMODE:
    case WM_ENTERSIZEMOVE:
      cancelHostPointers(*state, "window-cancel");
      return DefWindowProcW(window, message, wParam, lParam);
    case WM_ACTIVATEAPP:
      if (!wParam) cancelHostPointers(*state, "app-deactivated");
      return DefWindowProcW(window, message, wParam, lParam);
    case WM_MOVE:
    case WM_WINDOWPOSCHANGED: {
      const auto result = DefWindowProcW(window, message, wParam, lParam);
      PostMessageW(window, kOverlaySyncMessage, 0U, 0U);
      return result;
    }
    case kOverlaySyncMessage:
      if (state->platformAdapter) state->platformAdapter->reposition();
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->debugUi) state->debugUi->reposition();
#endif
      return 0;
    case WM_TIMER: {
      pumpWindowsText(*state);
      bool frameReady = false;
      bool needsCanvasPaint = false;
      bool frameChanged = false;
      if (state->platformAdapter) {
        frameReady = state->platformAdapter->prepareFrame(false);
        const auto frame = state->platformAdapter->frameStatus();
        needsCanvasPaint = frameReady && frame.needsPaint;
        frameChanged = frame.canonicalRequested || frame.previewRendered;
        if (frameReady && !needsCanvasPaint) (void)state->platformAdapter->present();
      } else {
        frameReady = state->host->composition().serviceFrameOpportunity(false, state->frameOpportunity);
        needsCanvasPaint = frameReady && state->frameOpportunity.needsPaint;
        frameChanged = state->frameOpportunity.canonicalRequested || state->frameOpportunity.previewRendered;
      }
      if (frameChanged)
        logInput(*state, "frame-opportunity", frameReady, {});
      if (state->host->composition().readback().lastOwnerControlCount != 0U)
        logInput(*state, "facade-control", true, {});
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->controller) state->controller->processHostRequests();
      if (state->debugUi) state->debugUi->refresh();
      if (state->inputLog && state->controller) {
        for (const auto& entry : state->controller->snapshot().activity.entries) {
          if (entry.sequence <= state->lastDebugActivitySequence) continue;
          const auto control = entry.productReceipt ? entry.productReceipt->control : std::nullopt;
          std::fprintf(state->inputLog.get(),
              "debug_control sequence=%llu request=%llu action=%s state=%u control_state=%d error=%d detail=%s\n",
              static_cast<unsigned long long>(entry.sequence),
              static_cast<unsigned long long>(entry.requestId), entry.action.c_str(),
              static_cast<unsigned>(entry.state), control ? static_cast<int>(control->state) : -1,
              control ? static_cast<int>(control->error) : -1, control ? control->detail.c_str() : "");
          state->lastDebugActivitySequence = entry.sequence;
          logInput(*state, "debug-control-readback",
              entry.state == canvas::debug_ui::DebugActivityState::kApplied, {});
        }
      }
#endif
      if (needsCanvasPaint) InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    case WM_SIZE:
      cancelHostPointers(*state, "resize");
      if (LOWORD(lParam) != 0U && HIWORD(lParam) != 0U)
        if (state->platformAdapter ? state->platformAdapter->resize(LOWORD(lParam), HIWORD(lParam),
              clientMetrics(window, LOWORD(lParam), HIWORD(lParam)).devicePixelRatio) :
              state->host->composition().resizeSurface(clientMetrics(window, LOWORD(lParam), HIWORD(lParam)))) {
          state->presentationCache.reset();
          InvalidateRect(window, nullptr, FALSE);
        }
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->debugUi) state->debugUi->reposition();
#endif
      return 0;
    case WM_DPICHANGED: {
      cancelHostPointers(*state, "dpi-change");
      const auto* bounds = reinterpret_cast<const RECT*>(lParam);
      SetWindowPos(window, nullptr, bounds->left, bounds->top,
          bounds->right - bounds->left, bounds->bottom - bounds->top,
          SWP_NOACTIVATE | SWP_NOZORDER);
      RECT client{};
      GetClientRect(window, &client);
      if (client.right > 0 && client.bottom > 0) {
        if (state->platformAdapter) (void)state->platformAdapter->resize(
            static_cast<std::uint32_t>(client.right), static_cast<std::uint32_t>(client.bottom),
            clientMetrics(window, static_cast<std::uint32_t>(client.right),
                          static_cast<std::uint32_t>(client.bottom)).devicePixelRatio);
        else (void)state->host->composition().resizeSurface(clientMetrics(window,
            static_cast<std::uint32_t>(client.right), static_cast<std::uint32_t>(client.bottom)));
        state->presentationCache.reset();
        InvalidateRect(window, nullptr, FALSE);
      }
      return 0;
    }
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE && state->facade->readImageCrop().active) {
        (void)state->facade->cancelImageCrop(state->facade->readImageCrop().cropToken);
        return 0;
      }
      if (wParam == VK_F12) {
#if AXIOM_MINIMAL_HAS_DEBUG_UI
        if (state->debugUi) {
          if(!admitWindowsDebugFocus(*state)){state->debugAfterText=true;return 0;}
          state->debugUi->toggle();
          logInput(*state, state->debugUi->visible() ? "debug-ui-on" : "debug-ui-off", true, {});
          return 0;
        }
#endif
      }
      return 0;
    case WM_LBUTTONDOWN:
      if (isPromotedPointerMouse()) return 0;
      // The owned Debug UI popup can retain keyboard focus after a tool click.
      // Canvas pointer entry must also restore the native text/IME destination.
      if (GetFocus() != window) SetFocus(window);
      SetCapture(window);
      sendPointer(*state, lParam, canvas::input::PointerPhase::kDown, wParam);
      if (!state->mouseKey) releaseHostMouseCapture(*state);
      return 0;
    case WM_MOUSEMOVE:
      if (!isPromotedPointerMouse() && !state->mouseKey) {
        const auto surface = state->host->composition().surfaceSnapshot();
        if (surface.width != 0U && surface.height != 0U)
          (void)state->host->composition().hoverSelectionAtViewPoint(
              static_cast<float>(GET_X_LPARAM(lParam)) * surface.logicalWidth / surface.width,
              static_cast<float>(GET_Y_LPARAM(lParam)) * surface.logicalHeight / surface.height);
      }
      if (!isPromotedPointerMouse() && state->mouseKey && (wParam & MK_LBUTTON))
        sendPointer(*state, lParam, canvas::input::PointerPhase::kMove, wParam);
      return 0;
    case WM_LBUTTONUP:
      if (isPromotedPointerMouse() || !state->mouseKey) return 0;
      state->releasingPointerUp = true;
      ReleaseCapture();
      state->releasingPointerUp = false;
      sendPointer(*state, lParam, canvas::input::PointerPhase::kUp, wParam);
      return 0;
    case WM_CAPTURECHANGED:
      if (!state->releasingPointerUp) {
        state->lastMousePoint.reset();
        if (state->mouseKey) cancelKey(*state, *state->mouseKey, "mouse-capture-lost");
        state->mouseKey.reset();
      }
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    case WM_PAINT: { PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); paintCanvas(*state, dc); EndPaint(window, &paint); return 0; }
    case WM_CLOSE:
      if(!finishWindowsText(*state)){state->closeAfterText=true;return 0;}
      cancelHostPointers(*state, "window-close");
      return DefWindowProcW(window, message, wParam, lParam);
    case WM_DESTROY:
      cancelHostPointers(*state, "window-destroy");
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->debugUi) state->debugUi->shutdown();
#endif
      if (state->platformAdapter) (void)state->platformAdapter->shutdown();
      PostQuitMessage(0); return 0;
    default: return DefWindowProcW(window, message, wParam, lParam);
  }
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
  (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  WNDCLASSW klass{}; klass.hInstance = instance; klass.lpfnWndProc = windowProc;
  klass.style=CS_DBLCLKS;
  klass.lpszClassName = kWindowClass; klass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  if (RegisterClassW(&klass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
  State state{}; state.host = std::make_unique<MinimalRuntimeHost>();
  wchar_t* logPath = nullptr;
  std::size_t logPathLength = 0;
  if (_wdupenv_s(&logPath, &logPathLength, L"AXIOM_MINIMAL_INPUT_LOG") == 0 && logPath) {
    // The CLI verifier may read the flushed diagnostics while the user keeps
    // drawing. Keep a single writer but permit shared readers.
    state.inputLog.reset(_wfsopen(logPath, L"w", _SH_DENYWR));
    std::free(logPath);
  }
  state.facade = std::make_unique<MinimalRuntimeFacade>(*state.host);
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  state.debugUi = std::make_unique<canvas::debug_ui::WindowsDebugUiHost>();
#endif
  state.window = CreateWindowExW(0, kWindowClass, L"Axiom Minimal Runtime Host",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
      1024, 768, nullptr, nullptr, instance, &state);
  if (!state.window) return 1;
  (void)RegisterPointerDeviceNotifications(state.window, TRUE);
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  if (!state.debugUi->initialize(state.window)) return 1;
#endif
  RECT client{};
  GetClientRect(state.window, &client);
  const auto width = static_cast<std::uint32_t>((std::max)(client.right, 1L));
  const auto height = static_cast<std::uint32_t>((std::max)(client.bottom, 1L));
  state.platformAdapter = std::make_unique<canvas::platform::WindowsRuntimeAdapter>(
      state.host->publicComposition(), state.window);
  const auto dpi = GetDpiForWindow(state.window);
  const auto scale = dpi == 0U ? 1.0F : static_cast<float>(dpi) / 96.0F;
  if (!state.platformAdapter->bind(width, height, scale) || !state.host->registerSource(1U)) return 1;
  std::array<wchar_t,32768> modulePath{};const auto moduleLength=GetModuleFileNameW(nullptr,modulePath.data(),static_cast<DWORD>(modulePath.size()));
  if(moduleLength==0 || moduleLength>=modulePath.size() || !state.platformAdapter->loadHostFonts(
      (std::filesystem::path(modulePath.data()).parent_path()/L"axiom-font.otf").c_str(),
      (std::filesystem::path(modulePath.data()).parent_path()/L"fonts").c_str()))return 1;
  if (state.inputLog) {
#if defined(AXIOM_BUILD_SOURCE_REVISION)
    std::fprintf(state.inputLog.get(), "build_revision=%s dirty=%d msvc=%d pid=%lu debug_ui_enabled=%d\n",
        AXIOM_BUILD_SOURCE_REVISION, AXIOM_BUILD_DIRTY, _MSC_FULL_VER,
        GetCurrentProcessId(), AXIOM_MINIMAL_HAS_DEBUG_UI);
#endif
    const auto metrics = state.host->composition().surfaceSnapshot();
    std::fprintf(state.inputLog.get(), "surface physical=%ux%u logical=%.3fx%.3f dpr=%.3f dpi=%u\n",
        metrics.width, metrics.height, metrics.logicalWidth, metrics.logicalHeight,
        metrics.devicePixelRatio, GetDpiForWindow(state.window));
    logInput(state, "startup", true, {});
  }
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  if (!state.facade->setMultiSelectionPermission(canvas::runtime::MultiSelectionPermissionMode::kLocalSingleUser)) return 1;
  state.controller = std::make_unique<canvas::debug_ui::DebugController>(
      canvas::debug_ui::DebugControllerContext{state.facade.get(), nullptr, nullptr,
          state.facade.get(), nullptr, nullptr, nullptr,
          [&state] { return canvas::minimal_runtime_host::chooseWindowsImageFile(state.window); }});
  state.debugUi->setController(state.controller.get());
  state.debugUi->setFocusAdmission([&state]{return admitWindowsDebugFocus(state);});
#endif
  SetTimer(state.window, kTimer, kTimerMs, nullptr);
  ShowWindow(state.window, show == SW_HIDE || show == 0 ? SW_SHOWNORMAL : show);
  UpdateWindow(state.window);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  KillTimer(state.window, kTimer);
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  state.debugUi->shutdown();
#endif
  return static_cast<int>(message.wParam);
}
