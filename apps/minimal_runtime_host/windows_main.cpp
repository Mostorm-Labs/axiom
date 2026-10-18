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
  canvas::minimal_runtime_host::WindowsPresentationCache presentationCache;
};

void paintCanvas(State& state, HDC dc) {
  auto& composition = state.host->composition();
  auto& runtime = state.host->runtime();
  canvas::interaction::PreviewPresentationCapture previewCapture{};
  std::uint64_t previewPresentCount = 0U;
  const bool hadPreview = runtime.previewActive();
  bool previewRenderedThisPaint = false;
  if (hadPreview && runtime.previewRenderState().dirty &&
      runtime.capturePreviewPresentation(previewCapture)) {
    previewRenderedThisPaint =
        runtime.renderPreviewPresentation(previewCapture, &previewPresentCount);
  }
  auto* provider = runtime.activeSurfaceProvider();
  if (provider == nullptr) return;
  const auto& surface = state.host->runtime().surface();
  if (surface.width == 0U || surface.height == 0U) return;
  if (provider->describe().backend == canvas::render::RenderTargetBackend::kD3D12) {
    if (const auto frame = runtime.lastSubmittedCanonicalFrame(); frame.has_value() &&
        frame->frameId.value() != state.lastPresentedFrame) {
      if (composition.receivePlatformPresented({
              frame->viewId, frame->frameId, frame->surfaceGeneration,
              frame->metricsGeneration, canvas::render::PresentOutcome::kPresented,
              canvas::render::PresentEvidenceKind::kPlatformQualified, std::nullopt})) {
        state.lastPresentedFrame = frame->frameId.value();
      }
    }
    if (previewRenderedThisPaint && runtime.previewActive()) {
      (void)runtime.acknowledgePreviewPresentation(previewCapture, previewPresentCount);
    }
    return;
  }
  const auto byteCount = static_cast<std::size_t>(surface.width) * surface.height * 4U;
  // canonicalFrameCount is a stable content serial across the local submit /
  // platform-feedback transition. Using the transient submitted frame id
  // here would force one unnecessary full readback after every feedback.
  const auto canonicalSerial = runtime.canonicalFrameCount();
  const auto surfaceGeneration = surface.generation;
  if (state.presentationCache.shouldReadCanonical(
          surfaceGeneration, canonicalSerial, byteCount)) {
    std::vector<std::uint8_t> pixels(byteCount);
    if (provider->readbackRgba(pixels).code !=
        canvas::render::BackendSubmissionCode::kAccepted) return;
    state.presentationCache.markCanonicalRead(surfaceGeneration, canonicalSerial,
                                              std::move(pixels));
  }

  auto* previewProvider = runtime.previewSurfaceProvider();
  const auto previewState = runtime.previewRenderState();
  const auto previewGeneration = hadPreview ? runtime.previewSurfaceGeneration() : 0U;
  const auto previewRevision = hadPreview ? previewState.contentRevision : 0U;
  if (!hadPreview) {
    state.presentationCache.clearPreview();
  } else if (previewProvider != nullptr &&
             (previewRenderedThisPaint || state.presentationCache.shouldReadPreview(
                  previewGeneration, previewRevision, byteCount))) {
    std::vector<std::uint8_t> overlay(byteCount);
    if (previewProvider->readbackRgba(overlay).code ==
        canvas::render::BackendSubmissionCode::kAccepted) {
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
  if (const auto frame = runtime.lastSubmittedCanonicalFrame(); frame.has_value() &&
      frame->frameId.value() != state.lastPresentedFrame) {
    if (composition.receivePlatformPresented({
            frame->viewId, frame->frameId, frame->surfaceGeneration,
            frame->metricsGeneration, canvas::render::PresentOutcome::kPresented,
            canvas::render::PresentEvidenceKind::kPlatformQualified, std::nullopt})) {
      state.lastPresentedFrame = frame->frameId.value();
    }
  }
  if (previewRenderedThisPaint && runtime.previewActive()) {
    (void)runtime.acknowledgePreviewPresentation(previewCapture, previewPresentCount);
  }
}

void sendPointer(State& state, LPARAM lParam, canvas::input::PointerPhase phase) {
  (void)state.host->composition().ownerPump();
  const auto x = static_cast<float>(GET_X_LPARAM(lParam));
  const auto y = static_cast<float>(GET_Y_LPARAM(lParam));
  canvas::input::PlatformPointerBatch batch;
  batch.samples.push_back({1U, 1U, ++state.sequence, ++state.timestamp, x, y, 0.5F, 0.0F, 0.0F,
                           {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent, phase});
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
    case WM_TIMER: {
      (void)state->host->composition().ownerPump();
      bool needsCanvasPaint = false;
      if (state->host->runtime().canonicalPresentationDirty()) {
        const auto frameId = state->host->runtime().nextCanonicalFrameId();
        if (frameId != 0U) {
          needsCanvasPaint = state->host->composition().requestCanonicalFrame(frameId) ||
              needsCanvasPaint;
        }
      }
      const auto& runtime = state->host->runtime();
      needsCanvasPaint = runtime.previewRenderState().dirty ||
          runtime.transientOverlayDirty() ||
          runtime.pendingCanonicalHandoffCount() != 0U ||
          runtime.lastSubmittedCanonicalFrame().has_value() || needsCanvasPaint;
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->debugUi) state->debugUi->refresh();
#endif
      if (needsCanvasPaint) InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    case WM_SIZE:
      if (LOWORD(lParam) != 0U && HIWORD(lParam) != 0U)
        if (state->host->composition().resizeSurface(LOWORD(lParam), HIWORD(lParam))) {
          state->presentationCache.reset();
          InvalidateRect(window, nullptr, FALSE);
        }
#if AXIOM_MINIMAL_HAS_DEBUG_UI
      if (state->debugUi) state->debugUi->reposition();
#endif
      return 0;
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
  auto canonicalProvider = std::make_unique<canvas::minimal_runtime_host::WindowsD3D12SurfaceProvider>(
      state.window, true);
  if (canonicalProvider->resize(1024U, 768U).code != canvas::render::BackendSubmissionCode::kAccepted ||
      !state.host->runtime().registerSurfaceProvider("windows-d3d12-canonical",
          std::move(canonicalProvider))) return 1;
  auto previewProvider = std::make_unique<canvas::minimal_runtime_host::WindowsD3D12SurfaceProvider>(
      state.window, false);
  previewProvider->setOverlayOffset(0, 0);
  if (previewProvider->resize(1024U, 768U).code != canvas::render::BackendSubmissionCode::kAccepted ||
      !state.host->runtime().registerPreviewSurfaceProvider("windows-d3d12-preview",
          std::move(previewProvider))) return 1;
  if (!state.host->composition().bindSurface(1024U, 768U) || !state.host->registerSource(1U)) return 1;
  state.host->runtime().setPlatformPresentationDeferred(true);
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
