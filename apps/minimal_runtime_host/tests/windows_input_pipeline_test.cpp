#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {
DWORD fixtureTimeMs = 10U;
std::vector<MOUSEMOVEPOINT> mouseHistory;
LONG fixtureMessageTime() { return static_cast<LONG>(fixtureTimeMs); }
int fixtureMouseHistory(UINT, LPMOUSEMOVEPOINT, LPMOUSEMOVEPOINT out, int count, DWORD) {
  const auto size = (std::min)(static_cast<int>(mouseHistory.size()), count);
  std::copy_n(mouseHistory.begin(), size, out);
  return size;
}
}

// Supply only OS history observations. The actual host ingress, tool dispatch,
// BrushSession, canonical commit, and surface provider remain production code.
#define GetMessageTime fixtureMessageTime
#define GetMouseMovePointsEx fixtureMouseHistory
#define WinMain AxiomMinimalWindowEntryForTest
#include "../windows_main.cpp"
#undef WinMain
#undef GetMouseMovePointsEx
#undef GetMessageTime

namespace {
HWND previewWindow = nullptr;
void selectionUsesWindowMessagesAndLatchedShift() {
  using namespace canvas;
  mouseHistory.clear();fixtureTimeMs=100;
  State state{};state.host=std::make_unique<MinimalRuntimeHost>();
  state.facade=&state.host->publicComposition().controlFacade();
  WNDCLASSW klass{};klass.hInstance=GetModuleHandleW(nullptr);klass.lpfnWndProc=windowProc;
  klass.lpszClassName=L"AxiomRegionSelectionInputTest";assert(RegisterClassW(&klass));
  const auto window=CreateWindowExW(0,klass.lpszClassName,L"region fixture",WS_OVERLAPPEDWINDOW,
      80,100,800,600,nullptr,nullptr,klass.hInstance,&state);assert(window);
  auto& composition=state.host->composition();assert(composition.bindSurface(800,600));
  assert(composition.setPlatformPresentationDeferred(true));assert(state.host->registerSource(1));
  semantic::InsertObjectsOp insert;
  for (unsigned i=0;i<2;++i) {
    semantic::ObjectRecord r;r.id=foundation::ObjectId::fromUint64(9500+i);r.kind=semantic::ObjectKind::kShape;r.kind_version=1;
    r.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(i+1)});r.content=semantic::ShapeContent{1,80,60};
    r.transform.tx=100+i*160;r.transform.ty=100;insert.objects.push_back(r);
  }
  semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(9502)},
      semantic::DocumentId{foundation::ObjectId::fromUint64(1)},1,1,insert};
  assert(composition.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied);
  auto& facade=composition.controlFacade();std::uint64_t request=0;
  const auto setMode=[&](runtime::CanvasSelectionMode mode) {
    assert(facade.setSelectionAcquisitionMode(mode,++request).state==runtime::ProductControlState::kQueued);
    assert(composition.ownerPump());
  };
  const auto snapshot=[&] {return facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());};
  const auto message=[&](UINT kind,int x,int y,WPARAM flags) {++fixtureTimeMs;SendMessageW(window,kind,flags,MAKELPARAM(x,y));};
  const auto generation=composition.readback().semanticGeneration;
  setMode(runtime::CanvasSelectionMode::kRectangle);
  message(WM_LBUTTONDOWN,90,90,MK_LBUTTON);message(WM_MOUSEMOVE,185,165,MK_LBUTTON);message(WM_LBUTTONUP,185,165,0);
  assert(snapshot().selectedObjectCount==1);
  message(WM_LBUTTONDOWN,250,90,MK_LBUTTON|MK_SHIFT);
  message(WM_MOUSEMOVE,345,165,MK_LBUTTON);message(WM_LBUTTONUP,345,165,0);
  assert(snapshot().selectedObjectCount==2); // Shift-at-Down, released before Move/Up.
  setMode(runtime::CanvasSelectionMode::kFreeform);
  message(WM_LBUTTONDOWN,20,20,MK_LBUTTON);message(WM_MOUSEMOVE,500,200,MK_LBUTTON);
  SendMessageW(window,WM_CAPTURECHANGED,0,0);assert(!snapshot().regionSelectionActive);
  assert(snapshot().selectedObjectCount==2);
  assert(facade.setTool(4103,++request,facade.readRuntimeState().identity.runtimeGeneration).state==runtime::ProductControlState::kQueued);
  assert(composition.ownerPump());assert(snapshot().selectedObjectCount==2);
  setMode(runtime::CanvasSelectionMode::kPoint);assert(snapshot().selectedObjectCount==2);
  assert(composition.readback().semanticGeneration==generation);assert(DestroyWindow(window));
}
BOOL CALLBACK findPreview(HWND w, LPARAM) {
  wchar_t name[128]{};
  GetClassNameW(w, name, 128);
  if (std::wstring_view(name) == L"AxiomD3D12PreviewOverlay") previewWindow = w;
  return TRUE;
}

void mouseHistoryReachesRuntimeAfterShape() {
  State state{};
  state.host = std::make_unique<MinimalRuntimeHost>();
  state.facade = &state.host->publicComposition().controlFacade();
  WNDCLASSW klass{}; klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = windowProc; klass.lpszClassName = L"AxiomMouseHistoryTest";
  assert(RegisterClassW(&klass));
  auto window = CreateWindowExW(0U, klass.lpszClassName, L"history fixture",
      WS_OVERLAPPEDWINDOW, 80, 100, 640, 480, nullptr, nullptr, klass.hInstance, &state);
  assert(window);
  auto& composition = state.host->composition();
  assert(composition.bindSurface(640U, 480U));
  assert(composition.setPlatformPresentationDeferred(true));
  assert(state.host->registerSource(1U));
  auto& runtime = composition.qualificationRuntime();
  assert(runtime.selectCreationPreset(canvas::interaction::CreationPreset::kRectangle));
  sendPointer(state, MAKELPARAM(40, 40), canvas::input::PointerPhase::kDown);
  sendPointer(state, MAKELPARAM(160, 130), canvas::input::PointerPhase::kMove);
  sendPointer(state, MAKELPARAM(160, 130), canvas::input::PointerPhase::kUp);
  assert(runtime.semanticObjectCount() == 1U);
  assert(runtime.selectTool(canvas::interaction::CanvasEditingRuntime::ToolMode::kBrush));
  fixtureTimeMs = 100U;
  sendPointer(state, MAKELPARAM(200, 200), canvas::input::PointerPhase::kDown);
  POINT origin{}; assert(ClientToScreen(window, &origin));
  mouseHistory = {{origin.x + 240, origin.y + 208, 104U, 0U},
                  {origin.x + 230, origin.y + 203, 103U, 0U},
                  {origin.x + 220, origin.y + 200, 102U, 0U},
                  {origin.x + 210, origin.y + 201, 101U, 0U},
                  {origin.x + 200, origin.y + 200, 100U, 0U}};
  fixtureTimeMs = 104U;
  sendPointer(state, MAKELPARAM(240, 208), canvas::input::PointerPhase::kMove);
  const auto& trace = runtime.baselineTrace();
  std::printf("Runtime confirmed trace: expected=5 actual=%zu\n", trace.size());
  std::fflush(stdout);
  assert(trace.size() == 5U);
  assert(trace[1].viewX == 210.0F && trace[2].viewX == 220.0F);
  assert(trace[3].viewX == 230.0F && trace[4].viewX == 240.0F);
  // Delivering the same coalesced window twice must not duplicate samples.
  sendPointer(state, MAKELPARAM(240, 208), canvas::input::PointerPhase::kMove);
  assert(trace.size() == 5U);
  // Windows timestamps have millisecond precision. Distinct observations in
  // the same millisecond remain distinct; exactly repeated history is ignored.
  mouseHistory.insert(mouseHistory.begin(),
      {origin.x + 250, origin.y + 213, 104U, 0U});
  sendPointer(state, MAKELPARAM(250, 213), canvas::input::PointerPhase::kMove);
  assert(trace.size() == 6U && trace.back().viewX == 250.0F);
  // Up flushes any unconsumed history before its one terminal observation.
  mouseHistory.insert(mouseHistory.begin(),
      {origin.x + 260, origin.y + 220, 105U, 0U});
  fixtureTimeMs = 106U;
  sendPointer(state, MAKELPARAM(270, 230), canvas::input::PointerPhase::kUp);
  assert(trace.size() == 8U);
  assert(trace[6].viewX == 260.0F && trace[7].viewX == 270.0F);
  assert(trace[6].phase == canvas::input::PointerPhase::kMove);
  assert(trace[7].phase == canvas::input::PointerPhase::kUp);
  for (std::size_t i = 1U; i < trace.size(); ++i)
    assert(trace[i].timestampNs > trace[i - 1U].timestampNs);
  assert(runtime.semanticObjectCount() == 2U);
  // The next stroke must not replay any of the previous stroke's history.
  fixtureTimeMs = 200U;
  sendPointer(state, MAKELPARAM(300, 300), canvas::input::PointerPhase::kDown);
  fixtureTimeMs = 201U;
  sendPointer(state, MAKELPARAM(310, 310), canvas::input::PointerPhase::kMove);
  assert(trace.size() == 10U);
  sendPointer(state, MAKELPARAM(310, 310), canvas::input::PointerPhase::kUp);
  assert(runtime.semanticObjectCount() == 3U);

  // A screen location to the left of the primary display is returned as a
  // signed 16-bit coordinate by GetMouseMovePointsEx.
  assert(SetWindowPos(window, nullptr, -500, 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER));
  origin = {}; assert(ClientToScreen(window, &origin));
  fixtureTimeMs = 300U;
  sendPointer(state, MAKELPARAM(200, 200), canvas::input::PointerPhase::kDown);
  mouseHistory = {{(origin.x + 220) & 0xFFFF, (origin.y + 200) & 0xFFFF, 302U, 0U},
                  {(origin.x + 210) & 0xFFFF, (origin.y + 201) & 0xFFFF, 301U, 0U},
                  {(origin.x + 200) & 0xFFFF, (origin.y + 200) & 0xFFFF, 300U, 0U}};
  fixtureTimeMs = 302U;
  sendPointer(state, MAKELPARAM(220, 200), canvas::input::PointerPhase::kMove);
  assert(trace.size() == 14U && trace[12].viewX == 210.0F);
  sendPointer(state, MAKELPARAM(220, 200), canvas::input::PointerPhase::kUp);
  assert(runtime.semanticObjectCount() == 4U);

  // The same physical point maps once to logical space at non-unit DPI.
  assert(composition.resizeSurface({320.0F, 240.0F, 640U, 480U, 2.0F, 2.0F}));
  mouseHistory.clear();
  fixtureTimeMs = 400U;
  sendPointer(state, MAKELPARAM(200, 200), canvas::input::PointerPhase::kDown);
  const auto firstScaledIndex = trace.size() - 1U;
  assert(trace.back().viewX == 100.0F && trace.back().viewY == 100.0F);
  fixtureTimeMs = 401U;
  sendPointer(state, MAKELPARAM(240, 208), canvas::input::PointerPhase::kUp);
  assert(trace.size() == firstScaledIndex + 2U);
  assert(runtime.semanticObjectCount() == 5U);
  assert(DestroyWindow(window));
}

void previewAndCanonicalUseSameDeviceCoordinates() {
  using namespace canvas;
  runtime::RuntimeCompositionCore composition;
  assert(composition.open());
  assert(composition.bindSurface({320.0F, 240.0F, 640U, 480U, 2.0F, 2.0F}));
  assert(composition.setPlatformPresentationDeferred(true));
  auto& runtime = composition.qualificationRuntime();
  assert(runtime.setViewportZoomAt(1.55F, 160.0F, 120.0F));
  assert(runtime.applyViewportNavigation(
      {interaction::ViewportNavigationKind::kWheelPan, 24.0F, -17.0F, 0.0F, 0.0F, 1.0F}));
  assert(runtime.selectBrushProfile("vector-solid-v1", 1U));
  assert(runtime.setInkOptions(8.0F, 0.02F, 0.04F, 0.1F, 1.0F, 1.0F));
  assert(runtime.beginBrushSession(7U, 1U));
  // Repeated endpoints remove the deliberate preview-only unfinished tail.
  for (std::uint64_t i = 1U; i <= 30U; ++i) {
    const auto content = runtime.viewToContent(i <= 20U ? 100.0F + 4.0F * static_cast<float>(i) : 180.0F,
                                               120.0F);
    assert(runtime.appendBrushSample(7U, content.first, content.second, .5F, i, false, false));
  }
  assert(runtime.presentBrushPreview());
  std::vector<std::uint8_t> preview(640U * 480U * 4U), canonical(preview.size());
  assert(composition.readPreviewRgba(preview));
  assert(runtime.finishBrushSession(7U));
  runtime::RuntimeFrameOpportunity frame;
  assert(composition.serviceFrameOpportunity(true, frame));
  assert(composition.readCanonicalRgba(canonical));
  const auto yCenter = [](const auto& pixels, bool isPreview) {
    std::uint64_t count = 0U, total = 0U;
    for (std::uint32_t y = 0U; y < 480U; ++y)
      for (std::uint32_t x = 250U; x < 330U; ++x) {
        const auto offset = (y * 640U + x) * 4U;
        const bool covered = isPreview ? pixels[offset + 3U] > 128U
            : pixels[offset] < 64U && pixels[offset + 1U] < 64U && pixels[offset + 2U] < 64U;
        if (covered) { total += y; ++count; }
      }
    assert(count > 100U);
    return static_cast<double>(total) / static_cast<double>(count);
  };
  const auto previewY = yCenter(preview, true);
  const auto canonicalY = yCenter(canonical, false);
  std::printf("DPR=2 zoom=1.55 pan=(24,-17): previewY=%.3f canonicalY=%.3f\n",
      previewY, canonicalY);
  assert(std::abs(previewY - canonicalY) < 0.6);
  assert(std::abs(previewY - 239.5) < 0.6);
}

void previewOriginMatchesOwner() {
  State state{}; state.host = std::make_unique<MinimalRuntimeHost>();
  WNDCLASSW klass{}; klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = windowProc; klass.lpszClassName = L"AxiomPreviewOriginTest";
  assert(RegisterClassW(&klass));
  auto owner = CreateWindowExW(0U, klass.lpszClassName, L"preview origin fixture",
      WS_OVERLAPPEDWINDOW, 220, 180, 640, 480, nullptr, nullptr, klass.hInstance, &state);
  assert(owner);
  {
    state.platformAdapter = std::make_unique<canvas::platform::WindowsRuntimeAdapter>(
        state.host->publicComposition(), owner);
    assert(state.platformAdapter->bind(640U, 480U, 1.0F));
    EnumThreadWindows(GetCurrentThreadId(), findPreview, 0);
    assert(previewWindow);
    POINT expected{}; assert(ClientToScreen(owner, &expected));
    RECT actual{}; assert(GetWindowRect(previewWindow, &actual));
    std::printf("Preview origin: expected=(%ld,%ld) actual=(%ld,%ld)\n",
        expected.x, expected.y, actual.left, actual.top);
    std::fflush(stdout);
    assert(actual.left == expected.x && actual.top == expected.y);
    ShowWindow(previewWindow, SW_SHOWNOACTIVATE);
    // A visible preview must also follow owner moves without another input
    // sample or a new surface binding.
    assert(SetWindowPos(owner, nullptr, 470, 320, 0, 0, SWP_NOSIZE | SWP_NOZORDER));
    MSG message{};
    while (PeekMessageW(&message, owner, kOverlaySyncMessage, kOverlaySyncMessage, PM_REMOVE))
      DispatchMessageW(&message);
    expected = {}; assert(ClientToScreen(owner, &expected));
    assert(GetWindowRect(previewWindow, &actual));
    assert(actual.left == expected.x && actual.top == expected.y);
    assert(SetWindowPos(owner, nullptr, 390, 240, 0, 0, SWP_NOSIZE | SWP_NOZORDER));
    state.platformAdapter->reposition();
    expected = {}; assert(ClientToScreen(owner, &expected));
    assert(GetWindowRect(previewWindow, &actual));
    assert(actual.left == expected.x && actual.top == expected.y);
    assert(state.platformAdapter->shutdown());
    state.platformAdapter.reset();
  }
  assert(DestroyWindow(owner));
}
}

int main(int argc, char** argv) {
  (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  if (argc > 1 && std::string_view(argv[1]) == "selection") selectionUsesWindowMessagesAndLatchedShift();
  else if (argc > 1 && std::string_view(argv[1]) == "origin") previewOriginMatchesOwner();
  else if (argc > 1 && std::string_view(argv[1]) == "coordinates")
    previewAndCanonicalUseSameDeviceCoordinates();
  else mouseHistoryReachesRuntimeAfterShape();
  return 0;
}
