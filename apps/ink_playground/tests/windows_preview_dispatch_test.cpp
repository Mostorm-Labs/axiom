// Run production Win32 dispatch and Runtime rendering. Synthetic messages are
// regression tests, not physical mouse/touch qualification.
#include "../platform/windows/main.cpp"
#include "ink_playground_history_test_access.hpp"
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

void transientOverlayRetriesAfterBusySurface(State& s, BusyPreview& preview) {
  assert(s.host->selectCreationPreset(canvas::ink_playground::CreationPreset::kRectangle));
  preview.busy = true;
  const canvas::input::PointerKey key{7U, 901U, 1U};
  assert(s.host->creationPointer(key, canvas::input::PointerPhase::kDown, 20.0F, 20.0F));
  assert(s.host->creationPointer(key, canvas::input::PointerPhase::kMove, 120.0F, 100.0F));
  assert(s.host->transientOverlayDirty());
  preview.busy = false;
  tick(s);
  assert(!s.host->transientOverlayDirty());
  assert(preview.presentCount() > 0U);
  assert(s.host->creationPointer(key, canvas::input::PointerPhase::kCancel, 120.0F, 100.0F));
}

void creationReleaseRetiresBusyGuideWithoutBlockingNextGesture(State& s,
                                                               BusyPreview& preview) {
  assert(s.host->setSelectionMode(false));
  assert(s.host->selectCreationPreset(canvas::ink_playground::CreationPreset::kRectangle));
  const canvas::input::PointerKey key{7U, 902U, 1U};
  assert(s.host->creationPointer(key, canvas::input::PointerPhase::kDown, 20.0F, 20.0F));
  assert(s.host->creationPointer(key, canvas::input::PointerPhase::kMove, 120.0F, 100.0F));
  preview.busy = false;
  tick(s);
  assert(!s.host->transientOverlayDirty());
  preview.busy = true;
  assert(s.host->creationPointer(key, canvas::input::PointerPhase::kUp, 120.0F, 100.0F));
  assert(!s.host->creationActive());
  assert(!s.host->transientOverlayDirty());
  assert(preview.overlayVisible());
  assert(s.host->presentCanonicalFrame(s.host->canonicalFrameCount() + 1U, 0.0));
  assert(!preview.overlayVisible());
  preview.busy = false;
  const canvas::input::PointerKey next{7U, 903U, 1U};
  assert(s.host->creationPointer(next, canvas::input::PointerPhase::kDown, 40.0F, 40.0F));
  assert(s.host->creationPointer(next, canvas::input::PointerPhase::kMove, 160.0F, 140.0F));
  assert(s.host->creationPointer(next, canvas::input::PointerPhase::kUp, 160.0F, 140.0F));
  assert(s.host->semanticObjectCount() == 2U);
  assert(s.host->selectTool(InkPlaygroundHost::ToolMode::kBrush));
}

void connectorCreationCanRepeatThroughWin32Mouse(State& s,
    canvas::ink_playground::CreationPreset preset) {
  assert(s.host->bindSurface(256U, 256U));
  assert(s.host->selectCreationPreset(preset));
  const auto draw = [&](int x0, int y0, int x1, int y1) {
    assert(submitMouseSample(s.window, s, WM_LBUTTONDOWN, MK_LBUTTON,
                             MAKELPARAM(x0, y0 + State::kToolbarHeight)));
    assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON,
                             MAKELPARAM(x1, y1 + State::kToolbarHeight)));
    assert(submitMouseSample(s.window, s, WM_LBUTTONUP, 0,
                             MAKELPARAM(x1, y1 + State::kToolbarHeight)));
  };
  draw(20, 30, 120, 30);
  assert(s.host->semanticObjectCount() == 1U);
  tick(s);
  draw(40, 60, 160, 60);
  assert(s.host->semanticObjectCount() == 2U &&
         "Connector must remain reusable through the Win32 mouse adapter");
  tick(s);
  draw(60, 90, 180, 90);
  assert(s.host->semanticObjectCount() == 3U &&
         "Connector must remain reusable for a third Win32 mouse gesture");
}

// Regression for the physical failure path: while creating a connector, the
// endpoint snaps to a rectangle's vertical centre guide.  The guide must not
// poison the pointer/canonical handoff for the next connector gesture.
void connectorCreationCanRepeatAfterCenterGuide(State& s,
    canvas::ink_playground::CreationPreset preset) {
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  assert(s.host->bindSurface(256U, 256U));
  canvas::semantic::ObjectRecord target{};
  target.id = canvas::foundation::ObjectId::fromUint64(0x5001U);
  target.kind = canvas::semantic::ObjectKind::kShape;
  target.kind_version = 1U;
  target.placement.order_key = canvas::semantic::OrderKey{{0x20U}};
  target.transform.tx = 100.0;
  target.transform.ty = 40.0;
  target.content = canvas::semantic::ShapeContent{1U, 40.0, 40.0};
  assert(Access::applyRemote(*s.host, canvas::semantic::InsertObjectsOp{{target}}));
  assert(s.host->selectCreationPreset(preset));
  const auto draw = [&](int y, int x1) {
    assert(submitMouseSample(s.window, s, WM_LBUTTONDOWN, MK_LBUTTON,
                             MAKELPARAM(20, y + State::kToolbarHeight)));
    assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON,
                             MAKELPARAM(x1, y + State::kToolbarHeight)));
    assert(submitMouseSample(s.window, s, WM_LBUTTONUP, 0,
                             MAKELPARAM(x1, y + State::kToolbarHeight)));
  };
  // x=120 is the target's vertical centre.  Leave the guide engaged through
  // the release, then immediately start two more gestures.
  draw(20, 120);
  tick(s);
  draw(80, 120);
  tick(s);
  draw(140, 120);
  assert(s.host->semanticObjectCount() == 4U &&
         "A centre snap guide must not block subsequent Line/Arrow gestures");
}

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
  // Regression: a worker can finish the preview render just before pointer-up
  // closes the presentation gate. The UI completion message must still make
  // that matching frame visible until CanonicalVisible retires it.
  assert(s.host->beginBrushSession(700U, 1U));
  assert(s.host->appendBrushSample(700U, 24.0, 24.0, 0.5, 1U));
  PreviewPresentationCapture racedCapture{};
  assert(s.host->capturePreviewPresentation(racedCapture));
  assert(s.host->renderPreviewPresentation(racedCapture));
  s.host->setPreviewOverlayVisible(false);
  s.canonicalPending = true;
  stopPreviewPresentation(s);
  {
    std::lock_guard completionLock(s.previewCompletionMutex);
    s.previewCompletion = racedCapture;
    s.previewCompletionPresentCount = s.host->previewPresentCount();
  }
  WindowProc(s.window, State::kPreviewPresentedMessage, 0, 0);
  assert(p->overlayVisible());
  assert(s.host->cancelBrushSession(700U));
  s.canonicalPending = false;
  tick(s);
  // Reproduce the real Runtime multi-contact sequence rather than calling
  // applyViewportNavigation directly. A settled pinch must not leave the
  // Windows canonical-pending gate blocking the first post-pinch preview.
  deliverRuntimeSample(s, 41U, 1U, 1'000'000U, 24.0F, 24.0F,
                       canvas::input::PointerPhase::kDown);
  startPreviewRenderPump(s);
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
  // A successful render must make the transient platform surface visible
  // before the UI-thread acknowledgement message is serviced.
  s.host->setPreviewOverlayVisible(false);
  PreviewPresentationCapture directCapture{};
  assert(s.host->capturePreviewPresentation(directCapture));
  assert(s.host->renderPreviewPresentation(directCapture));
  assert(p->overlayVisible());
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
  // A partial erase after a settled zoom must retire its transient session
  // without leaving the Windows deferred-presentation gate closed for the
  // next brush stroke. This is the production sequence reported on the
  // physical host: zoom, erase, canonical tick, then draw again.
  assert(s.host->selectTool(InkPlaygroundHost::ToolMode::kPartialEraser));
  assert(s.host->eraserBegin(440U));
  assert(s.host->eraserSample(440U, 96.0, 64.0));
  assert(s.host->eraserFinish(440U));
  assert(!s.host->previewActive());
  s.canonicalFrameReady = false;
  s.canonicalPending = true;
  stopPreviewPresentation(s);
  tick(s);
  assert(!s.canonicalPending);
  assert(!s.host->previewActive() && !p->overlayVisible());
  assert(s.host->selectTool(InkPlaygroundHost::ToolMode::kBrush));
  assert(s.host->selectBrushProfile("vector-solid-v1", 1U));
  const auto afterErasePreviewBaseline = s.host->previewPresentCount();
  assert(submitMouseSample(s.window, s, WM_LBUTTONDOWN, MK_LBUTTON,
                           MAKELPARAM(30, 90)));
  assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON,
                           MAKELPARAM(110, 150)));
  waitForPreview(s, afterErasePreviewBaseline + 1U);
  assert(s.host->previewActive());
  assert(s.host->previewPresentCount() > afterErasePreviewBaseline);
  assert(submitMouseSample(s.window, s, WM_LBUTTONUP, 0,
                           MAKELPARAM(130, 170)));
  tick(s);
  assert(!s.host->previewActive() && !p->overlayVisible());
  // Repeat with an object transform so the erase mask is built in local
  // coordinates while the Windows preview lifecycle remains deferred.
  assert(s.host->selectTool(InkPlaygroundHost::ToolMode::kBrush));
  assert(s.host->beginBrushSession(441U, 1U));
  assert(s.host->appendBrushSample(441U, 32.0, 64.0, 0.5, 1U));
  assert(s.host->appendBrushSample(441U, 160.0, 64.0, 0.5, 2U));
  assert(s.host->finishBrushSession(441U));
  assert(s.host->presentCanonicalFrame(s.host->canonicalFrameCount() + 1U, 0.0));
  assert(s.host->setSelectionMode(true));
  assert(s.host->selectAtViewPoint(80.0F, 64.0F));
  const auto transformHandle = s.host->selectionOverlay()->handle(
      canvas::render::HandleKind::kRight).center;
  assert(s.host->selectionPointer(443U, canvas::input::PointerPhase::kDown,
                                  transformHandle.x, transformHandle.y));
  assert(s.host->selectionPointer(443U, canvas::input::PointerPhase::kMove,
                                  transformHandle.x + 40.0F, transformHandle.y));
  assert(s.host->selectionPointer(443U, canvas::input::PointerPhase::kUp,
                                  transformHandle.x + 40.0F, transformHandle.y));
  assert(s.host->setSelectionMode(false));
  assert(s.host->selectTool(InkPlaygroundHost::ToolMode::kPartialEraser));
  assert(s.host->eraserBegin(442U));
  assert(s.host->eraserSample(442U, 120.0, 96.0));
  assert(s.host->eraserFinish(442U));
  assert(!s.host->previewActive());
  s.canonicalFrameReady = false;
  s.canonicalPending = true;
  stopPreviewPresentation(s);
  tick(s);
  assert(!s.canonicalPending);
  assert(s.host->selectTool(InkPlaygroundHost::ToolMode::kBrush));
  assert(s.host->selectBrushProfile("vector-solid-v1", 1U));
  const auto afterTransformedErasePreviewBaseline = s.host->previewPresentCount();
  assert(submitMouseSample(s.window, s, WM_LBUTTONDOWN, MK_LBUTTON,
                           MAKELPARAM(34, 94)));
  assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON,
                           MAKELPARAM(104, 154)));
  waitForPreview(s, afterTransformedErasePreviewBaseline + 1U);
  assert(s.host->previewActive());
  assert(s.host->previewPresentCount() > afterTransformedErasePreviewBaseline);
  assert(submitMouseSample(s.window, s, WM_LBUTTONUP, 0,
                           MAKELPARAM(130, 180)));
  tick(s);
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
  assert(s.host->semanticObjectCount() == 11U);
  stopPreviewRenderPump(s);
  DestroyWindow(s.window);
}
void selectionDispatch() {
  State s;
  s.host = std::make_unique<InkPlaygroundHost>();
  assert(s.host->bindSurface(256, 256));
  s.window = CreateWindowExW(0, L"AxiomPreviewDispatchTest", L"transform dispatch",
      WS_POPUP, 0, 0, 256, 312, nullptr, nullptr, GetModuleHandleW(nullptr), &s);
  assert(s.window);
  s.runtimeFacade = std::make_unique<WindowsRuntimeFacade>(s);
  assert(s.host->beginBrushSession(900U));
  assert(s.host->appendBrushSample(900U, 32, 64, 0.5, 1U));
  assert(s.host->appendBrushSample(900U, 160, 64, 0.5, 2U));
  assert(s.host->finishBrushSession(900U));
  assert(s.runtimeFacade->setSelectionMode(true, 1U, 1U).state ==
         canvas::runtime::ProductControlState::kApplied);
  assert(s.host->selectAtViewPoint(80, 64));
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  const auto original = Access::objects(*s.host);
  const auto operations = s.host->submittedOperationCount();
  const auto handle = s.host->selectionOverlay()->handle(canvas::render::HandleKind::kRight).center;
  const auto point = [&](int dx) { return MAKELPARAM(static_cast<short>(handle.x + dx),
      static_cast<short>(handle.y + State::kToolbarHeight)); };
  const auto beginDrag = [&] {
    assert(submitMouseSample(s.window, s, WM_LBUTTONDOWN, MK_LBUTTON, point(0)));
    assert(submitMouseSample(s.window, s, WM_MOUSEMOVE, MK_LBUTTON, point(40)));
    assert(s.host->selectionTransformActive());
    assert(s.host->transientTransformCount() == 1U);
    assert(Access::objects(*s.host) == original);
  };
  beginDrag();
  assert(submitMouseSample(s.window, s, WM_LBUTTONUP, 0, point(40)));
  assert(s.host->submittedOperationCount() == operations + 1U);
  assert(s.selectionPointers.empty() && s.canvasInputSequences.empty());
  assert(s.host->undo());
  const auto afterUndoOperations = s.host->submittedOperationCount();
  for (const auto message : {WM_KEYDOWN, WM_KILLFOCUS, WM_CAPTURECHANGED, WM_SIZE}) {
    beginDrag();
    std::cout << "cancel message=" << message << std::endl;
    WindowProc(s.window, message, message == WM_KEYDOWN ? VK_ESCAPE : 0U, 0);
    assert(!s.host->selectionTransformActive());
    assert(s.host->transientTransformCount() == 0U);
    assert(s.selectionPointers.empty() && s.canvasInputSequences.empty());
    assert(GetCapture() != s.window);
    assert(Access::objects(*s.host) == original);
  }
  beginDrag();
  WindowProc(s.window, WM_POINTERCAPTURECHANGED, 77U, reinterpret_cast<LPARAM>(s.window));
  assert(s.host->selectionTransformActive());
  WindowProc(s.window, WM_CAPTURECHANGED, 0U, 0);
  assert(!s.host->selectionTransformActive());
  beginDrag();
  assert(s.runtimeFacade->setSelectionMode(false, 2U, 1U).state ==
         canvas::runtime::ProductControlState::kApplied);
  assert(!s.host->selectionTransformActive());
  assert(GetCapture() != s.window);
  assert(s.host->submittedOperationCount() == afterUndoOperations);
  assert(s.runtimeFacade->submitSelectionPointer({77U,
      canvas::runtime::SelectionPointerPhase::kDown, 80, 64, 3U, 999U}).state ==
         canvas::runtime::ProductControlState::kRejected);
  assert(s.runtimeFacade->setSelectionMode(true, 4U, 1U).state ==
         canvas::runtime::ProductControlState::kApplied);
  assert(s.host->selectAtViewPoint(80, 64));
  using P = canvas::runtime::SelectionPointerPhase;
  // The production native touch/pen and legacy touch adapters use this same
  // sequence owner. IDs are independent of the mouse and cannot steal a drag.
  assert(submitSelectionSample(s, 71U, P::kDown, handle.x, handle.y));
  assert(submitSelectionSample(s, 72U, P::kDown, handle.x, handle.y));
  assert(submitSelectionSample(s, 71U, P::kMove, handle.x + 30, handle.y));
  assert(submitSelectionSample(s, 72U, P::kUp, handle.x + 100, handle.y));
  WindowProc(s.window, WM_POINTERCAPTURECHANGED, 72U, 0);
  assert(s.host->selectionTransformActive());
  WindowProc(s.window, WM_POINTERCAPTURECHANGED, 71U, 0);
  assert(!s.host->selectionTransformActive());
  assert(s.selectionPointers.empty() && s.canvasInputSequences.empty());
  assert(Access::objects(*s.host) == original);
  const canvas::debug_ui::DebugInputSequence debugSequence{91U, 1000U};
  assert(s.inputCapture.begin(debugSequence, canvas::debug_ui::DebugInputOwner::kDebug) ==
         canvas::debug_ui::DebugInputOwner::kDebug);
  s.canvasInputSequences[91U] = debugSequence;
  assert(!submitSelectionSample(s, 91U, P::kDown, handle.x, handle.y));
  assert(!s.host->selectionTransformActive());
  clearCanvasInput(s);
  // Maximize/restore must also terminate an ordinary drawing session. A
  // stale active key used to survive WM_SIZE and keep the render gate closed
  // after the deferred surface rebuild.
  assert(s.host->setSelectionMode(false));
  const canvas::input::PointerKey resizeKey{7U, 992U, 1U};
  s.activeKeys.emplace(992U, resizeKey);
  s.pointerStrokes.emplace(992U, 17U);
  s.canonicalPending = true;
  SetCapture(s.window);
  WindowProc(s.window, WM_SIZE, SIZE_MAXIMIZED, MAKELPARAM(512U, 412U));
  assert(s.activeKeys.empty());
  assert(s.pointerStrokes.empty());
  assert(!s.canonicalPending);
  assert(GetCapture() != s.window);
  std::cout << "PASS: Windows mouse RuntimeFacade commit/Escape/focus/capture/resize/tool/stale\n";
  DestroyWindow(s.window);
}
}
int main() {
  if (__argc > 1 && std::string_view(__argv[1]) == "guide-release") {
    State s;
    s.host = std::make_unique<InkPlaygroundHost>();
    s.canonicalRenderer = std::make_unique<canvas::render::SkiaRenderer>();
    assert(s.host->bindSurface(256U, 256U));
    auto preview = std::make_unique<BusyPreview>();
    assert(preview->resize(256U, 256U).code == canvas::render::BackendSubmissionCode::kAccepted);
    auto* p = preview.get();
    assert(s.host->registerPreviewSurfaceProvider("guide-release", std::move(preview)));
    s.host->setPlatformPresentationDeferred(true);
    WNDCLASSW klass{};
    klass.hInstance = GetModuleHandleW(nullptr);
    klass.lpfnWndProc = WindowProc;
    klass.lpszClassName = L"AxiomGuideReleaseTest";
    RegisterClassW(&klass);
    s.window = CreateWindowExW(0, klass.lpszClassName, L"guide release", WS_POPUP,
                               0, 0, 256, 312, nullptr, nullptr, klass.hInstance, &s);
    assert(s.window);
    creationReleaseRetiresBusyGuideWithoutBlockingNextGesture(s, *p);
    DestroyWindow(s.window);
    return 0;
  }
  run();
  selectionDispatch();
  State creation;
  creation.host = std::make_unique<InkPlaygroundHost>();
  creation.canonicalRenderer = std::make_unique<canvas::render::SkiaRenderer>();
  creation.previewBridge = std::make_unique<arc::Bridge>(arc::CreateNullBackend(),
                                                           arc::CreateNullBackend());
  WNDCLASSW klass{};
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = WindowProc;
  klass.lpszClassName = L"AxiomPreviewCreationDispatchTest";
  RegisterClassW(&klass);
  creation.window = CreateWindowExW(0, klass.lpszClassName, L"creation dispatch",
      WS_POPUP, 0, 0, 256, 312, nullptr, nullptr, klass.hInstance, &creation);
  assert(creation.window);
  connectorCreationCanRepeatThroughWin32Mouse(creation,
      canvas::ink_playground::CreationPreset::kArrow);
  assert(creation.host->setSelectionMode(false));
  assert(creation.host->selectCreationPreset(
      canvas::ink_playground::CreationPreset::kLine));
  const auto drawLine = [&] (int y) {
    assert(submitMouseSample(creation.window, creation, WM_LBUTTONDOWN, MK_LBUTTON,
                             MAKELPARAM(20, y + State::kToolbarHeight)));
    assert(submitMouseSample(creation.window, creation, WM_MOUSEMOVE, MK_LBUTTON,
                             MAKELPARAM(120, y + State::kToolbarHeight)));
    assert(submitMouseSample(creation.window, creation, WM_LBUTTONUP, 0,
                             MAKELPARAM(120, y + State::kToolbarHeight)));
  };
  drawLine(90);
  drawLine(120);
  assert(creation.host->semanticObjectCount() == 5U);
  State centerGuide;
  centerGuide.host = std::make_unique<InkPlaygroundHost>();
  centerGuide.canonicalRenderer = std::make_unique<canvas::render::SkiaRenderer>();
  centerGuide.previewBridge = std::make_unique<arc::Bridge>(arc::CreateNullBackend(),
                                                             arc::CreateNullBackend());
  centerGuide.window = CreateWindowExW(0, klass.lpszClassName, L"center guide dispatch",
      WS_POPUP, 0, 0, 256, 312, nullptr, nullptr, klass.hInstance, &centerGuide);
  assert(centerGuide.window);
  connectorCreationCanRepeatAfterCenterGuide(centerGuide,
      canvas::ink_playground::CreationPreset::kArrow);
  DestroyWindow(centerGuide.window);
  DestroyWindow(creation.window);
}
