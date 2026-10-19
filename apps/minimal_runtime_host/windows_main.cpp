#include "minimal_runtime_host.hpp"
#include "minimal_runtime_facade.hpp"
#include "windows_presentation_cache.hpp"
#if AXIOM_MINIMAL_HAS_DEBUG_UI
#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/windows_host.hpp"
#endif
#include "windows_d3d12_surface_provider.hpp"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

struct State final {
  HWND window = nullptr;
  std::unique_ptr<MinimalRuntimeHost> host;
  std::unique_ptr<MinimalRuntimeFacade> facade;
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  std::unique_ptr<canvas::debug_ui::DebugController> controller;
  std::unique_ptr<canvas::debug_ui::WindowsDebugUiHost> debugUi;
#endif
  std::uint64_t sequence = 0;
  std::uint64_t timestamp = 0;
  std::uint64_t lastPresentedFrame = 0;
  bool releasingPointerUp = false;
  std::optional<MOUSEMOVEPOINT> lastMousePoint;
  canvas::minimal_runtime_host::WindowsD3D12SurfaceProvider* previewProvider = nullptr;
  canvas::minimal_runtime_host::WindowsPresentationCache presentationCache;
  canvas::runtime::RuntimeFrameOpportunity frameOpportunity{};
};

canvas::render::SurfaceMetrics clientMetrics(HWND window, std::uint32_t width,
                                             std::uint32_t height) noexcept {
  const auto dpi = GetDpiForWindow(window);
  const auto scale = dpi == 0U ? 1.0F : static_cast<float>(dpi) / 96.0F;
  return {width / scale, height / scale, width, height, scale, scale};
}

void paintCanvas(State& state, HDC dc) {
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

void sendPointer(State& state, LPARAM lParam, canvas::input::PointerPhase phase) {
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
        sampleX, sampleY, 0.5F, 0.0F, 0.0F, {}, {}, provenance, samplePhase});
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
  (void)state.host->submit(batch);
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
  switch (message) {
    case WM_MOVE:
    case WM_WINDOWPOSCHANGED: {
      const auto result = DefWindowProcW(window, message, wParam, lParam);
      PostMessageW(window, kOverlaySyncMessage, 0U, 0U);
      return result;
    }
    case kOverlaySyncMessage:
      if (state->previewProvider) state->previewProvider->reposition();
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->debugUi) state->debugUi->reposition();
#endif
      return 0;
    case WM_TIMER: {
      const bool frameReady = state->host->composition().serviceFrameOpportunity(
          false, state->frameOpportunity);
      const bool needsCanvasPaint = frameReady && state->frameOpportunity.needsPaint;
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->debugUi) state->debugUi->refresh();
#endif
      if (needsCanvasPaint) InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    case WM_SIZE:
      if (LOWORD(lParam) != 0U && HIWORD(lParam) != 0U)
        if (state->host->composition().resizeSurface(clientMetrics(window, LOWORD(lParam), HIWORD(lParam)))) {
          state->presentationCache.reset();
          InvalidateRect(window, nullptr, FALSE);
        }
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->debugUi) state->debugUi->reposition();
#endif
      return 0;
    case WM_DPICHANGED: {
      const auto* bounds = reinterpret_cast<const RECT*>(lParam);
      SetWindowPos(window, nullptr, bounds->left, bounds->top,
          bounds->right - bounds->left, bounds->bottom - bounds->top,
          SWP_NOACTIVATE | SWP_NOZORDER);
      RECT client{};
      GetClientRect(window, &client);
      if (client.right > 0 && client.bottom > 0) {
        (void)state->host->composition().resizeSurface(clientMetrics(window,
            static_cast<std::uint32_t>(client.right), static_cast<std::uint32_t>(client.bottom)));
        state->presentationCache.reset();
        InvalidateRect(window, nullptr, FALSE);
      }
      return 0;
    }
    case WM_KEYDOWN:
      if (wParam == VK_F12) {
#if AXIOM_MINIMAL_HAS_DEBUG_UI
        if (state->debugUi) { state->debugUi->toggle(); return 0; }
#endif
      }
      return 0;
    case WM_LBUTTONDOWN: SetCapture(window); sendPointer(*state, lParam, canvas::input::PointerPhase::kDown); return 0;
    case WM_MOUSEMOVE: if (wParam & MK_LBUTTON) sendPointer(*state, lParam, canvas::input::PointerPhase::kMove); return 0;
    case WM_LBUTTONUP:
      state->releasingPointerUp = true;
      ReleaseCapture();
      state->releasingPointerUp = false;
      sendPointer(*state, lParam, canvas::input::PointerPhase::kUp);
      return 0;
    case WM_CAPTURECHANGED:
      if (!state->releasingPointerUp) {
        state->lastMousePoint.reset();
        (void)state->host->unregisterSource(1U, ++state->sequence, ++state->timestamp);
        (void)state->host->registerSource(1U);
      }
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    case WM_PAINT: { PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); paintCanvas(*state, dc); EndPaint(window, &paint); return 0; }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProcW(window, message, wParam, lParam);
  }
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
  (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  WNDCLASSW klass{}; klass.hInstance = instance; klass.lpfnWndProc = windowProc;
  klass.lpszClassName = kWindowClass; klass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  if (RegisterClassW(&klass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
  State state{}; state.host = std::make_unique<MinimalRuntimeHost>();
  state.facade = std::make_unique<MinimalRuntimeFacade>(*state.host);
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  state.debugUi = std::make_unique<canvas::debug_ui::WindowsDebugUiHost>();
#endif
  state.window = CreateWindowExW(0, kWindowClass, L"Axiom Minimal Runtime Host",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
      1024, 768, nullptr, nullptr, instance, &state);
  if (!state.window) return 1;
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  if (!state.debugUi->initialize(state.window)) return 1;
#endif
  RECT client{};
  GetClientRect(state.window, &client);
  const auto width = static_cast<std::uint32_t>((std::max)(client.right, 1L));
  const auto height = static_cast<std::uint32_t>((std::max)(client.bottom, 1L));
  auto canonicalProvider = std::make_unique<canvas::minimal_runtime_host::WindowsD3D12SurfaceProvider>(
      state.window, true);
  if (canonicalProvider->resize(width, height).code != canvas::render::BackendSubmissionCode::kAccepted ||
      !state.host->composition().registerSurfaceProvider("windows-d3d12-canonical",
          std::move(canonicalProvider))) return 1;
  auto previewProvider = std::make_unique<canvas::minimal_runtime_host::WindowsD3D12SurfaceProvider>(
      state.window, false);
  state.previewProvider = previewProvider.get();
  previewProvider->setOverlayOffset(0, 0);
  if (previewProvider->resize(width, height).code != canvas::render::BackendSubmissionCode::kAccepted ||
      !state.host->composition().registerPreviewSurfaceProvider("windows-d3d12-preview",
          std::move(previewProvider))) return 1;
  if (!state.host->composition().bindSurface(clientMetrics(state.window, width, height)) ||
      !state.host->registerSource(1U)) return 1;
  (void)state.host->composition().setPlatformPresentationDeferred(true);
#if AXIOM_MINIMAL_HAS_DEBUG_UI
  state.controller = std::make_unique<canvas::debug_ui::DebugController>(
      canvas::debug_ui::DebugControllerContext{state.facade.get(), nullptr, nullptr,
          state.facade.get(), nullptr, nullptr, nullptr});
  state.debugUi->setController(state.controller.get());
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
