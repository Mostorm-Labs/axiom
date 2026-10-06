// Run production Win32 dispatch and Runtime rendering. Synthetic messages are
// regression tests, not physical mouse/touch qualification.
#include "../platform/windows/main.cpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
class BusyPreview final : public canvas::render::SkiaSurfaceProvider {
 public:
  canvas::render::TransparentOverlaySkiaSurfaceProvider raster;
  bool busy = false;
  bool rejectPresent = false;
  std::uint64_t acquisitions = 0;
  DWORD lastAcquireThread = 0;
  canvas::render::RenderTargetInfo describe() const noexcept override { return raster.describe(); }
  canvas::render::SkiaSurfaceAcquireResult acquire() noexcept override {
    ++acquisitions;
    lastAcquireThread = GetCurrentThreadId();
    if (busy) return canvas::render::SkiaSurfaceAcquireResult::rejected(
        canvas::render::SkiaSurfaceAcquireCode::kUnavailable, "busy");
    return raster.acquire();
  }
  void release() noexcept override { raster.release(); }
  canvas::render::BackendSubmissionResult present() noexcept override {
    if (rejectPresent) return canvas::render::BackendSubmissionResult::rejected("busy present");
    return raster.present();
  }
  canvas::render::BackendSubmissionResult resize(std::uint32_t w, std::uint32_t h) noexcept override { return raster.resize(w, h); }
  canvas::render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> b) noexcept override { return raster.readbackRgba(b); }
  std::uint64_t generation() const noexcept override { return raster.generation(); }
  canvas::render::BackendSubmissionResult advanceGeneration() noexcept override { return raster.advanceGeneration(); }
  std::uint64_t readbackCount() const noexcept override { return raster.readbackCount(); }
  std::uint64_t cpuCopyCount() const noexcept override { return raster.cpuCopyCount(); }
  std::uint64_t presentCount() const noexcept override { return raster.presentCount(); }
  void setOverlayVisible(bool v) noexcept override { raster.setOverlayVisible(v); }
  bool overlayVisible() const noexcept override { return raster.overlayVisible(); }
};

void tick(State& s) { WindowProc(s.window, WM_TIMER, State::kRenderTimerId, 0); }

void waitForPreview(State& s, std::uint64_t minimum) {
  for (int i = 0; i < 100 && s.host->previewPresentCount() < minimum; ++i) {
    wakeRenderPump(s);
    MSG message{};
    while (PeekMessageW(&message, s.window, State::kPreviewPresentedMessage,
                        State::kPreviewPresentedMessage, PM_REMOVE)) {
      DispatchMessageW(&message);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

void pumpPreviewMessages(State& s) {
  MSG message{};
  while (PeekMessageW(&message, s.window, State::kPreviewPresentedMessage,
                      State::kPreviewPresentedMessage, PM_REMOVE)) {
    DispatchMessageW(&message);
  }
}

canvas::input::PlatformPointerSample runtimeSample(
    std::uint64_t pointer, std::uint64_t sequence, std::uint64_t timestamp,
    float x, float y, canvas::input::PointerPhase phase) {
  return canvas::input::PlatformPointerSample{
      7U, pointer, sequence, timestamp, x, y, 0.5F, 0.0F, 0.0F, {}, {},
      canvas::input::SampleProvenance::kConfirmedCurrent, phase};
}

void deliverRuntimeSample(State& s, std::uint64_t pointer, std::uint64_t sequence,
                          std::uint64_t timestamp, float x, float y,
                          canvas::input::PointerPhase phase) {
  canvas::input::PlatformPointerBatch batch;
  batch.samples.push_back(runtimeSample(pointer, sequence, timestamp, x, y, phase));
  assert(s.host->acceptPlatformBatch(batch, timestamp));
  if (phase == canvas::input::PointerPhase::kDown) {
    const auto key = s.host->platformKey(7U, pointer);
    assert(key.has_value());
    s.activeKeys[pointer] = *key;
    s.pointerStrokes[pointer] = s.host->platformStrokeId(*key).value_or(++s.stroke);
    enablePreviewPresentation(s);
  } else if (phase == canvas::input::PointerPhase::kUp ||
             phase == canvas::input::PointerPhase::kCancel) {
    s.activeKeys.erase(pointer);
    s.pointerStrokes.erase(pointer);
    if (s.activeKeys.empty()) {
      s.canonicalFrameReady = false;
      s.canonicalPending = s.host->pendingCanonicalHandoffCount() != 0U;
      if (s.canonicalPending) stopPreviewPresentation(s);
      else enablePreviewPresentation(s);
    }
  }
  s.previewDirty = true;
  wakeRenderPump(s);
}

void run() {
  State s;
  s.host = std::make_unique<InkPlaygroundHost>();
  s.canonicalRenderer = std::make_unique<canvas::render::SkiaRenderer>();
  assert(s.host->bindSurface(256, 256));
  auto preview = std::make_unique<BusyPreview>();
  assert(preview->resize(256, 256).code == canvas::render::BackendSubmissionCode::kAccepted);
  auto* p = preview.get();
  assert(s.host->registerPreviewSurfaceProvider("dispatch-preview", std::move(preview)));
  s.host->setPlatformPresentationDeferred(true);
  WNDCLASSW klass{};
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = WindowProc;
  klass.lpszClassName = L"AxiomPreviewDispatchTest";
  assert(RegisterClassW(&klass));
  s.window = CreateWindowExW(0, klass.lpszClassName, L"preview dispatch", WS_POPUP,
      0, 0, 256, 312, nullptr, nullptr, klass.hInstance, &s);
  assert(s.window);
  // Exercise the same ARC lifecycle bridge used by the production Windows
  // host.  The previous loop only tested the Runtime/Skia path and therefore
  // could not catch a handoff state that blocks a later stroke.
  s.previewBridge = std::make_unique<arc::Bridge>(arc::CreateNullBackend(),
                                                   arc::CreateNullBackend());
  s.previewGeneration = p->generation();
  arc_preview_target_v0 target{};
  target.struct_size = sizeof(target);
  target.abi_version = ARC_ABI_VERSION;
  target.platform_kind = ARC_PLATFORM_WINDOWS;
  target.target_id = 1U;
  target.target_generation = s.previewGeneration;
  target.width_pixels = 256U;
  target.height_pixels = 256U;
  target.device_pixel_ratio = 1.0F;
  target.opaque_platform_handle = reinterpret_cast<std::uint64_t>(p);
  assert(s.previewBridge->Attach(target) == arc::Status::kOk);
  s.runtimeSinks = std::make_unique<WindowsArcRuntimeSinks>(s);
  s.host->setArcPreviewSink(s.runtimeSinks.get());
  s.host->setCanonicalVisibilitySink(s.runtimeSinks.get());
  startPreviewRenderPump(s);
  tick(s);
  // Reproduce the real Runtime multi-contact sequence rather than calling
  // applyViewportNavigation directly. A settled pinch must not leave the
  // Windows canonical-pending gate blocking the first post-pinch preview.
  deliverRuntimeSample(s, 41U, 1U, 1'000'000U, 24.0F, 24.0F,
                       canvas::input::PointerPhase::kDown);
  deliverRuntimeSample(s, 42U, 2U, 100'000'000U, 64.0F, 24.0F,
                       canvas::input::PointerPhase::kDown);
  deliverRuntimeSample(s, 42U, 3U, 120'000'000U, 96.0F, 24.0F,
                       canvas::input::PointerPhase::kMove);
  tick(s);
  deliverRuntimeSample(s, 41U, 4U, 140'000'000U, 24.0F, 24.0F,
                       canvas::input::PointerPhase::kUp);
  deliverRuntimeSample(s, 42U, 5U, 160'000'000U, 96.0F, 24.0F,
                       canvas::input::PointerPhase::kUp);
  tick(s);
  const auto postPinchPreviewBaseline = s.host->previewPresentCount();
  deliverRuntimeSample(s, 43U, 6U, 500'000'000U, 32.0F, 40.0F,
                       canvas::input::PointerPhase::kDown);
  deliverRuntimeSample(s, 43U, 7U, 510'000'000U, 96.0F, 96.0F,
                       canvas::input::PointerPhase::kMove);
  waitForPreview(s, postPinchPreviewBaseline + 1U);
  assert(s.host->previewPresentCount() > postPinchPreviewBaseline);
  deliverRuntimeSample(s, 43U, 8U, 520'000'000U, 128.0F, 128.0F,
                       canvas::input::PointerPhase::kUp);
  tick(s);
  assert(s.host->pendingCanonicalHandoffCount() == 0U);
  assert(!s.host->previewActive() && !p->overlayVisible());
  // A settled pinch/navigation must be visible before the first post-gesture
  // stroke. The first stroke must then complete its CanonicalVisible handoff
  // on the next render tick without requiring another pointer-down.
  const auto framesBeforePinch = s.host->canonicalFrameCount();
  assert(s.host->applyViewportNavigation({
      canvas::interaction::ViewportNavigationKind::kBrowserGesture,
      128.0F, 128.0F, 0.0F, 0.0F, 1.25F}));
  assert(s.host->canonicalPresentationDirty());
  tick(s);
  assert(s.host->canonicalFrameCount() == framesBeforePinch + 1U);
  assert(!s.host->canonicalPresentationDirty());
  const auto pinchFirstStrokeBaseline = s.host->canonicalFrameCount();
  assert(submitMouseSample(s.window, s, WM_LBUTTONDOWN, MK_LBUTTON,
                           MAKELPARAM(24, 84)));
  assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON,
                           MAKELPARAM(96, 144)));
  waitForPreview(s, s.host->previewPresentCount() + 1U);
  assert(s.host->previewActive());
  assert(submitMouseSample(s.window, s, WM_LBUTTONUP, 0,
                           MAKELPARAM(132, 180)));
  tick(s);
  assert(s.host->canonicalFrameCount() == pinchFirstStrokeBaseline + 1U);
  assert(s.host->pendingCanonicalHandoffCount() == 0U);
  assert(!s.host->previewActive() && !p->overlayVisible());
  const auto baseAcquire = p->acquisitions;
  assert(submitMouseSample(s.window, s, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 80)));
  for (int i=0;i<100;++i)
    assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(20+i, 90+i)));
  std::cout << "Input preview acquisitions=" << p->acquisitions-baseAcquire << std::endl;
  // The worker may race the tail of this input burst; the contract is that
  // any acquisition happens off the caller thread, not that it waits for the
  // burst to finish.
  waitForPreview(s, 1U);
  assert(s.host->previewPresentCount() >= 1U);
  // The complete Windows path must render off the input/UI thread. Moving
  // synchronous rendering from a pointer callback to WM_TIMER is insufficient.
  assert(p->lastAcquireThread != GetCurrentThreadId());
  p->busy = true;
  assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(140, 220)));
  waitForPreview(s, s.host->previewPresentCount() + 1U);
  assert(s.host->previewRenderState().dirty && s.previewDirty);
  assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(150, 230)));
  const auto latest = s.host->previewRenderState().contentRevision;
  p->busy = false;
  p->rejectPresent = true;
  waitForPreview(s, s.host->previewPresentCount());
  assert(s.previewDirty && s.host->previewRenderState().submittedRevision != latest);
  p->rejectPresent = false;
  for (int i = 0; i < 100 &&
       (s.previewDirty || s.host->previewRenderState().submittedRevision != latest); ++i) {
    wakeRenderPump(s);
    pumpPreviewMessages(s);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  assert(!s.previewDirty && s.host->previewRenderState().submittedRevision == latest);
  const auto frames = s.host->canonicalFrameCount();
  assert(submitMouseSample(s.window, s, WM_LBUTTONUP, 0, MAKELPARAM(160, 240)));
  // The final pointer-up must enter the canonical handoff state immediately;
  // this prevents an in-flight worker completion from becoming a new visible
  // preview before the canonical frame retires the session.
  assert(s.canonicalPending);
  // Keep the last presented preview visible through the canonical handoff.
  // Hiding it at pointer-up creates a one-frame blank/old-canvas flash while
  // the canonical provider is waiting for its next render tick.
  assert(p->overlayVisible());
  assert(s.host->canonicalFrameCount() == frames);
  // A completion that was already queued before pointer-up must be harmless;
  // processing it cannot resurrect the retired overlay.
  pumpPreviewMessages(s);
  assert(p->overlayVisible());
  tick(s);
  assert(s.host->canonicalFrameCount() == frames+1U);
  assert(!s.host->previewActive() && !p->overlayVisible());
  for(int i=0;i<5;++i) {
    assert(submitMouseSample(s.window,s,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(20,80+i*20)));
    assert(submitMouseSample(s.window,s,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(120,80+i*20)));
    waitForPreview(s, s.host->previewPresentCount() + 1U);
    assert(p->overlayVisible());
    assert(submitMouseSample(s.window,s,WM_LBUTTONUP,0,MAKELPARAM(140,80+i*20)));
    tick(s);
    waitForPreview(s, s.host->previewPresentCount());
    assert(!s.canonicalPending);
    assert(s.host->pendingCanonicalHandoffCount() == 0U);
    assert(!p->overlayVisible());
  }
  assert(s.host->semanticObjectCount() == 8U);
  stopPreviewRenderPump(s);
  DestroyWindow(s.window);
}
}
int main() { run(); }
