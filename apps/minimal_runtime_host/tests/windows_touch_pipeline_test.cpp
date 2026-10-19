#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
POINTER_INFO nativePointer{};
std::vector<POINTER_TOUCH_INFO> touchHistory;
std::vector<POINTER_PEN_INFO> penHistory;
bool forceNoHistory = false;
LPARAM extraInfo = 0;
[[maybe_unused]] BOOL fixturePointerInfo(UINT32 id, POINTER_INFO* out) {
  if (id != nativePointer.pointerId) return FALSE;
  *out = nativePointer;
  return TRUE;
}
[[maybe_unused]] BOOL fixtureTouchHistory(UINT32, UINT32* count, POINTER_TOUCH_INFO* out) {
  if (forceNoHistory) return FALSE;
  if (!out) { *count = static_cast<UINT32>(touchHistory.size()); return *count != 0U; }
  const auto size = (std::min)(*count, static_cast<UINT32>(touchHistory.size()));
  std::copy_n(touchHistory.begin(), size, out);
  *count = size;
  return size != 0U;
}
[[maybe_unused]] BOOL fixturePenHistory(UINT32, UINT32* count, POINTER_PEN_INFO* out) {
  if (!out) { *count = static_cast<UINT32>(penHistory.size()); return *count != 0U; }
  const auto size = (std::min)(*count, static_cast<UINT32>(penHistory.size()));
  std::copy_n(penHistory.begin(), size, out); *count = size; return size != 0U;
}
[[maybe_unused]] BOOL fixtureTouchInfo(UINT32, POINTER_TOUCH_INFO* out) {
  if (touchHistory.empty()) return FALSE;
  *out = touchHistory.front(); return TRUE;
}
[[maybe_unused]] BOOL fixturePenInfo(UINT32, POINTER_PEN_INFO* out) {
  if (penHistory.empty()) return FALSE;
  *out = penHistory.front(); return TRUE;
}
[[maybe_unused]] LPARAM fixtureExtraInfo() { return extraInfo; }
}

// Only replace the OS observation seam; use the production WindowProc,
// composition, ingress, arbitration, dispatch, Brush and canonical commit.
#define GetPointerInfo fixturePointerInfo
#define GetPointerTouchInfoHistory fixtureTouchHistory
#define GetPointerPenInfoHistory fixturePenHistory
#define GetPointerTouchInfo fixtureTouchInfo
#define GetPointerPenInfo fixturePenInfo
#define GetMessageExtraInfo fixtureExtraInfo
#define WinMain AxiomMinimalWindowEntryForTouchTest
#include "../windows_main.cpp"
#undef WinMain
#undef GetMessageExtraInfo
#undef GetPointerPenInfoHistory
#undef GetPointerTouchInfo
#undef GetPointerPenInfo
#undef GetPointerTouchInfoHistory
#undef GetPointerInfo

int main() {
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  State state{};
  state.host = std::make_unique<MinimalRuntimeHost>();
  std::FILE* diagnosticFile = nullptr;
  assert(_wfopen_s(&diagnosticFile, L"touch-fixture.log", L"w+") == 0);
  state.inputLog.reset(diagnosticFile);
  assert(state.inputLog);
  WNDCLASSW klass{};
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = windowProc;
  klass.lpszClassName = L"AxiomTouchIngressTest";
  assert(RegisterClassW(&klass));
  const auto window = CreateWindowExW(0U, klass.lpszClassName, L"touch fixture",
      WS_OVERLAPPEDWINDOW, 100, 100, 640, 480, nullptr, nullptr, klass.hInstance, &state);
  assert(window);
  auto& composition = state.host->composition();
  assert(composition.bindSurface({320.0F, 240.0F, 640U, 480U, 2.0F, 2.0F}));
  assert(composition.setPlatformPresentationDeferred(true));
  assert(state.host->registerSource(1U));
  auto& runtime = composition.qualificationRuntime();
  POINT origin{};
  assert(ClientToScreen(window, &origin));
  nativePointer.pointerType = PT_TOUCH;
  nativePointer.pointerId = 7U;
  nativePointer.sourceDevice = reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(0x123U));
  nativePointer.hwndTarget = window;
  nativePointer.pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT;
  nativePointer.dwTime = 100U;
  nativePointer.ptPixelLocation = {origin.x + 200, origin.y + 180};
  POINTER_TOUCH_INFO touch{};
  touch.pointerInfo = nativePointer;
  touch.touchMask = TOUCH_MASK_PRESSURE | TOUCH_MASK_CONTACTAREA;
  touch.pressure = 512U;
  touch.rcContact = {origin.x + 194, origin.y + 174, origin.x + 206, origin.y + 186};
  touchHistory = {touch};
  SendMessageW(window, WM_POINTERDOWN, 7U, 0);
  std::printf("WM_POINTERDOWN -> Runtime: expected=1 actual=%zu\n", runtime.baselineTrace().size());
  std::fflush(stdout);
  assert(runtime.baselineTrace().size() == 1U);
  assert(runtime.baselineTrace().back().key.pointer == 7U);
  assert(runtime.baselineTrace().back().viewX == 100.0F);
  assert(runtime.baselineTrace().back().viewY == 90.0F);
  const auto firstKey = runtime.baselineTrace().back().key;
  assert(firstKey.source != 1U && firstKey.generation != 0U);
  assert(GetCapture() == nullptr); // Touch never takes mouse-global capture.
  const auto touchAt = [&](UINT32 id, DWORD time, LONG x, LONG y, UINT message,
                           POINTER_FLAGS flags = POINTER_FLAG_NONE) {
    nativePointer.pointerId = id;
    nativePointer.dwTime = time;
    nativePointer.frameId = time;
    nativePointer.ptPixelLocation = {origin.x + x, origin.y + y};
    nativePointer.pointerFlags = flags != POINTER_FLAG_NONE ? flags
        : message == WM_POINTERDOWN ? POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT
        : message == WM_POINTERUP ? POINTER_FLAG_UP : POINTER_FLAG_UPDATE | POINTER_FLAG_INCONTACT;
    touch.pointerInfo = nativePointer;
    touchHistory = {touch};
    SendMessageW(window, message, id, 0);
  };
  const auto checkpoint = [&](const char* name) {
    std::printf("PASS %s objects=%zu operations=%zu trace=%zu viewport=%d scale=%.6f\n",
        name, runtime.semanticObjectCount(), runtime.submittedOperationCount(),
        runtime.baselineTrace().size(), runtime.viewportGestureClaimed(), runtime.viewportGesture().scale);
    std::fflush(stdout);
  };

  // Recover real OS history in chronological order; a repeated window is a no-op.
  nativePointer.pointerFlags = POINTER_FLAG_UPDATE | POINTER_FLAG_INCONTACT;
  nativePointer.dwTime = 102U;
  nativePointer.ptPixelLocation.x = origin.x + 220;
  auto h1 = touch; h1.pointerInfo.pointerFlags = nativePointer.pointerFlags;
  h1.pointerInfo.dwTime = 101U; h1.pointerInfo.ptPixelLocation.x = origin.x + 210;
  auto h2 = h1; h2.pointerInfo = nativePointer;
  touchHistory = {h2, h1, touch};
  SendMessageW(window, WM_POINTERUPDATE, 7U, 0);
  assert(runtime.baselineTrace().size() == 3U);
  assert(runtime.baselineTrace()[1].viewX == 105.0F && runtime.baselineTrace()[2].viewX == 110.0F);
  SendMessageW(window, WM_POINTERUPDATE, 7U, 0);
  assert(runtime.baselineTrace().size() == 3U);
  extraInfo = static_cast<LPARAM>(0xFF515780U);
  SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(200, 180));
  SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(220, 180));
  SendMessageW(window, WM_LBUTTONUP, 0U, MAKELPARAM(240, 180));
  extraInfo = 0;
  assert(runtime.baselineTrace().size() == 3U);
  assert(runtime.platformKey(firstKey.source, 7U) == firstKey);
  touchAt(7U, 103U, 240, 180, WM_POINTERUP);
  assert(runtime.semanticObjectCount() == 1U && !runtime.platformKey(firstKey.source, 7U));
  checkpoint("single/history/promotion/one-coordinate-map");

  // Reuse the native ID, but retain the Runtime's newly allocated generation.
  touchAt(7U, 200U, 200, 180, WM_POINTERDOWN);
  const auto reusedKey = runtime.baselineTrace().back().key;
  assert(reusedKey.source == firstKey.source && reusedKey.pointer == firstKey.pointer);
  assert(reusedKey.generation > firstKey.generation);
  assert(!composition.cancelPlatformPointer(firstKey, ++state.sequence, ++state.timestamp));
  assert(runtime.platformKey(reusedKey.source, 7U) == reusedKey);
  touchAt(7U, 150U, 200, 180, WM_POINTERUPDATE, POINTER_FLAG_CANCELED);
  SendMessageW(window, WM_POINTERCAPTURECHANGED, 7U, 0);
  assert(runtime.platformKey(reusedKey.source, 7U) == reusedKey);
  touchAt(7U, 201U, 220, 180, WM_POINTERUPDATE);
  touchAt(7U, 202U, 240, 180, WM_POINTERUP);
  const auto beforeStale = runtime.baselineTrace().size();
  touchAt(7U, 100U, 200, 180, WM_POINTERDOWN);
  assert(runtime.baselineTrace().size() == beforeStale);
  checkpoint("generation-reuse/stale-key/stale-native-cancel/stale-down");

  // Pending contacts at exactly 250 ms retain frozen AutoIntent navigation.
  touchAt(7U, 1000U, 100, 180, WM_POINTERDOWN);
  touchAt(8U, 1250U, 300, 180, WM_POINTERDOWN);
  assert(runtime.viewportGestureClaimed());
  touchAt(7U, 1251U, 80, 180, WM_POINTERUPDATE);
  touchAt(8U, 1252U, 340, 180, WM_POINTERUPDATE);
  assert(runtime.viewportGesture().scale > 1.1F);
  const auto beforeGestureUp = runtime.submittedOperationCount();
  touchAt(7U, 1253U, 80, 180, WM_POINTERUP);
  assert(runtime.viewportGestureClaimed());
  touchAt(8U, 1254U, 350, 180, WM_POINTERUPDATE);
  touchAt(8U, 1255U, 350, 180, WM_POINTERUP);
  assert(!runtime.viewportGestureClaimed() && runtime.submittedOperationCount() == beforeGestureUp);
  checkpoint("250ms-chord/zoom/one-up/other-continues");

  // Focus/capture cancellation must commit the current camera baseline before
  // the next independent gesture, and must not reset the viewport to identity.
  touchAt(7U, 1400U, 100, 180, WM_POINTERDOWN);
  touchAt(8U, 1400U, 300, 180, WM_POINTERDOWN);
  touchAt(8U, 1401U, 400, 180, WM_POINTERUPDATE);
  const auto scaleBeforeCancel = runtime.viewportGesture().scale;
  SendMessageW(window, WM_KILLFOCUS, 0U, 0);
  assert(!runtime.viewportGestureClaimed());
  assert(state.nativeContacts.empty());
  touchAt(7U, 1500U, 100, 180, WM_POINTERDOWN);
  touchAt(8U, 1500U, 300, 180, WM_POINTERDOWN);
  assert(std::abs(runtime.viewportGesture().scale - scaleBeforeCancel) < .0001F);
  SendMessageW(window, WM_POINTERCAPTURECHANGED, 7U, 0);
  assert(!runtime.platformKey(reusedKey.source, 7U));
  assert(runtime.platformKey(reusedKey.source, 8U));
  SendMessageW(window, WM_CANCELMODE, 0U, 0);
  checkpoint("focus/capture/cancel-camera-baseline");

  // Outside 250 ms, a late second finger is the original second Ink stroke.
  const auto beforeDelayed = runtime.submittedOperationCount();
  touchAt(7U, 2000U, 100, 180, WM_POINTERDOWN);
  touchAt(8U, 2251U, 300, 180, WM_POINTERDOWN);
  assert(!runtime.viewportGestureClaimed());
  touchAt(7U, 2252U, 140, 180, WM_POINTERUP);
  touchAt(8U, 2253U, 340, 180, WM_POINTERUP);
  assert(runtime.submittedOperationCount() == beforeDelayed + 2U);
  checkpoint("251ms-second-ink");

  // Missing native metadata stays unavailable, and source devices stay distinct.
  touch.touchMask = 0U;
  touchAt(7U, 3000U, 100, 180, WM_POINTERDOWN);
  const auto noPressureIndex = runtime.baselineTrace().size() - 1U;
  assert(runtime.baselineTrace()[noPressureIndex].pressure == 0.0F);
  touchAt(7U, 3001U, 120, 180, WM_POINTERUPDATE);
  nativePointer.sourceDevice = reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(0x456U));
  touchAt(8U, 3300U, 300, 180, WM_POINTERDOWN);
  const auto secondSourceKey = runtime.baselineTrace().back().key;
  assert(secondSourceKey.source != reusedKey.source);
  SendMessageW(window, WM_POINTERDEVICECHANGE, PDC_REMOVAL,
      reinterpret_cast<LPARAM>(nativePointer.sourceDevice));
  assert(!runtime.platformKey(secondSourceKey.source, 8U));
  assert(runtime.platformKey(reusedKey.source, 7U));
  SendMessageW(window, WM_CANCELMODE, 0U, 0);
  nativePointer.sourceDevice = reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(0x123U));
  checkpoint("unknown-metadata/device-source/device-removal");

  // Resizing/maximize/restore and minimizing cancel contacts before rebinding.
  for (const auto sizeKind : {SIZE_RESTORED, SIZE_MAXIMIZED, SIZE_MINIMIZED}) {
    touchAt(7U, nativePointer.dwTime + 100U, 100, 180, WM_POINTERDOWN);
    SendMessageW(window, WM_SIZE, sizeKind, sizeKind == SIZE_MINIMIZED ? 0 : MAKELPARAM(640, 480));
    assert(!runtime.platformKey(reusedKey.source, 7U));
    assert(state.nativeContacts.empty());
  }
  SendMessageW(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(640, 480));
  const auto beforeTen = runtime.submittedOperationCount();
  for (DWORD i = 0; i < 10U; ++i) {
    const auto beforeTrace = runtime.baselineTrace().size();
    touchAt(7U, 4000U + i * 100U, 100, 180, WM_POINTERDOWN);
    touchAt(7U, 4001U + i * 100U, 140, 180, WM_POINTERUPDATE);
    touchAt(7U, 4002U + i * 100U, 180, 180, WM_POINTERUP);
    assert(runtime.baselineTrace().size() == beforeTrace + 3U);
    assert(runtime.submittedOperationCount() == beforeTen + i + 1U);
  }
  checkpoint("resize/maximize/minimize/restore/ten-strokes");
  forceNoHistory = true;
  touch.touchMask = TOUCH_MASK_PRESSURE | TOUCH_MASK_CONTACTAREA;
  touchAt(7U, 5100U, 100, 180, WM_POINTERDOWN);
  assert(runtime.platformKey(reusedKey.source, 7U));
  touchAt(7U, 5101U, 100, 180, WM_POINTERUPDATE, POINTER_FLAG_CANCELED);
  assert(!runtime.platformKey(reusedKey.source, 7U) && state.nativeContacts.empty());
  forceNoHistory = false;
  checkpoint("typed-current-fallback/native-cancel");

  // A rejected Down must release ingress and dispatcher before a tool switch.
  touch.pressure = 2048U;
  touchAt(7U, 5200U, 100, 180, WM_POINTERDOWN);
  assert(!runtime.platformKey(reusedKey.source, 7U) && state.nativeContacts.empty());
  assert(runtime.selectCreationPreset(canvas::interaction::CreationPreset::kRectangle));
  assert(runtime.selectTool(canvas::interaction::CanvasEditingRuntime::ToolMode::kBrush));
  touch.pressure = 512U;
  const auto afterFailedDown = runtime.submittedOperationCount();
  touchAt(7U, 5201U, 100, 180, WM_POINTERDOWN);
  touchAt(7U, 5202U, 180, 180, WM_POINTERUP);
  assert(runtime.submittedOperationCount() == afterFailedDown + 1U);
  checkpoint("failed-down/tool-switch/next-stroke");

  nativePointer.pointerType = PT_PEN;
  nativePointer.pointerId = 19U;
  nativePointer.dwTime = 5300U;
  nativePointer.pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT;
  POINTER_PEN_INFO pen{};
  pen.pointerInfo = nativePointer;
  pen.penFlags = PEN_FLAG_ERASER;
  pen.penMask = PEN_MASK_PRESSURE | PEN_MASK_TILT_X | PEN_MASK_TILT_Y;
  pen.pressure = 256U; pen.tiltX = 15; pen.tiltY = -5;
  penHistory = {pen};
  SendMessageW(window, WM_POINTERDOWN, 19U, 0);
  const auto penKey = runtime.baselineTrace().back().key;
  assert(penKey.source != reusedKey.source && penKey.pointer == 19U);
  assert(runtime.baselineTrace().back().pressure == .25F);
  SendMessageW(window, WM_POINTERCAPTURECHANGED, 19U, 0);
  assert(!runtime.platformKey(penKey.source, 19U));
  checkpoint("pen-pressure/tool-capability");
  assert(std::fseek(state.inputLog.get(), 0, SEEK_SET) == 0);
  std::string diagnostics;
  char buffer[4096];
  while (const auto bytes = std::fread(buffer, 1U, sizeof(buffer), state.inputLog.get())) diagnostics.append(buffer, bytes);
  assert(diagnostics.find("pressure_available=1 tool=1 geometry=(6.000,6.000,0.000,1)") != std::string::npos);
  assert(diagnostics.find("pressure_available=0 tool=1 geometry=(0.000,0.000,0.000,0)") != std::string::npos);
  std::printf("%s", diagnostics.c_str());
  std::fflush(stdout);
  assert(composition.loseSurface());
  SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(100, 100));
  assert(GetCapture() != window);
  assert(composition.rebindSurface({640.0F, 480.0F, 640U, 480U, 1.0F, 1.0F}));
  SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(100, 100));
  assert(state.mouseKey);
  SendMessageW(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(640, 480));
  assert(!state.mouseKey && GetCapture() != window);
  checkpoint("failed-mouse-down/resize-release-mouse-capture");
  assert(DestroyWindow(window));
  return 0;
}
