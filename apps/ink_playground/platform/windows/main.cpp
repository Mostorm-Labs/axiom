#include "ink_playground_host.hpp"
#include "platform_brush_baseline_observation.hpp"
#include "arc/arc.hpp"
#include "windows_pointer_utils.hpp"
#include "windows_input_diagnostics.hpp"
#include "windows_smoke_evidence.hpp"
#include "canvas/render/canonical_handoff.hpp"
#include "canvas/ink/arc_runtime_sinks.hpp"
#include "windows_d3d12_skia_surface_provider.hpp"
#if defined(CANVAS_RENDER_HAS_SKIA)
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <sstream>
#include <span>
#include <unordered_map>
#include <vector>

namespace {
using canvas::ink_playground::InkPlaygroundHost;
class WindowsArcRuntimeSinks;

struct State { HWND window = nullptr; std::unique_ptr<InkPlaygroundHost> host;
  static constexpr int kToolbarHeight = 56;
  static constexpr UINT kRenderTimerId = 0xA710;
  static constexpr UINT kRenderIntervalMs = 16;
  HWND toolbarLabel = nullptr;
  std::unordered_map<int, HWND> toolButtons;
  std::unique_ptr<arc::Bridge> previewBridge; std::uint64_t previewGeneration = 1;
  std::uint64_t stroke = 0;
  // GetMessageTime() has millisecond resolution and several mouse messages
  // can arrive in one tick. BrushSession requires strictly increasing
  // confirmed sample sequences, so keep an independent process-local clock.
  std::uint64_t mouseSampleSequence = 0;
  std::uint64_t lastMouseTimestampNs = 0;
  // WM_POINTER_INFO.dwTime is allowed to be zero on the first contact and is
  // not a sample identity. Runtime requires a non-zero, strictly increasing
  // sequence per pointer, so Windows owns an independent process-local clock
  // exactly like the Android/Web adapters.
  std::uint64_t pointerSampleSequence = 0;
  std::uint64_t lastPointerTimestampNs = 0;
  std::ofstream pointerDiagnostic;
  std::filesystem::path pointerDiagnosticPath;
  bool nativeTouchChannelSeen = false;
  std::uint64_t deviceId = 0;
  std::unordered_map<std::uint64_t, canvas::input::PointerKey> activeKeys;
  std::unordered_map<std::uint64_t, std::uint64_t> pointerStrokes;
  std::vector<canvas::ink_playground::windows_input::PointerEvidenceSample> trace;
  std::size_t maxConcurrentPointers = 0;
  std::uint64_t cpuCopyCount = 0;
  std::uint64_t presentCount = 0;
  bool runtimePreviewVisible = true;
  // Presentation gate for the Windows-only overlay/fallback. Runtime keeps
  // the preview session alive until CanonicalVisible, but the platform
  // presentation must disappear immediately on the final pointer-up so a
  // slow canonical raster/readback cannot leave a stale preview on screen.
  bool previewPresentationEnabled = true;
  bool previewDirty = false;
  std::unique_ptr<WindowsArcRuntimeSinks> runtimeSinks;
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::SkiaRenderer> canonicalRenderer;
  canvas::ink_playground::WindowsD3D12SkiaSurfaceProvider* canonicalProvider = nullptr;
  canvas::ink_playground::WindowsD3D12SkiaSurfaceProvider* previewProvider = nullptr;
  bool canonicalFrameReady = false;
#endif
};
enum : int {
  kToolVector = 4101, kToolMarker, kToolChalk, kToolMembrane,
  kToolObjectEraser, kToolPartialEraser
};

bool selectWindowsTool(State& value, int command) {
  bool selected = false;
  if (command == kToolVector || command == kToolMarker ||
      command == kToolChalk || command == kToolMembrane) {
    const char* profile = command == kToolVector ? "vector-solid-v1" :
        command == kToolMarker ? "marker-flat-v1" :
        command == kToolChalk ? "chalk-grain-v1" : "membrane-v1";
    const auto revision = command == kToolChalk ? 4U : 1U;
    selected = value.host->selectTool(InkPlaygroundHost::ToolMode::kBrush) &&
               value.host->selectBrushProfile(profile, revision);
  } else if (command == kToolObjectEraser) {
    selected = value.host->selectTool(InkPlaygroundHost::ToolMode::kObjectEraser);
  } else if (command == kToolPartialEraser) {
    selected = value.host->selectTool(InkPlaygroundHost::ToolMode::kPartialEraser);
  } else {
    return false;
  }
  if (!selected) return false;
  for (const auto& [id, button] : value.toolButtons) {
    SendMessageW(button, BM_SETCHECK, id == command ? BST_CHECKED : BST_UNCHECKED, 0);
  }
  InvalidateRect(value.window, nullptr, FALSE);
  return true;
}

void hidePreviewPresentation(State& value) noexcept {
  value.previewPresentationEnabled = false;
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (value.previewProvider != nullptr) {
    value.previewProvider->setOverlayVisible(false);
  }
#endif
}

void retireVisiblePreviewPresentation(State& value) noexcept {
  // Call only after the canonical frame has actually reached the owner HWND.
  hidePreviewPresentation(value);
}

void enablePreviewPresentation(State& value) noexcept {
  value.previewPresentationEnabled = true;
}

void createWindowsToolPalette(State& value, HINSTANCE instance) {
  constexpr int kButtonWidth = 132;
  constexpr int kButtonGap = 4;
  struct Tool { int id; const wchar_t* label; };
  constexpr Tool tools[] = {
      {kToolVector, L"Vector"}, {kToolMarker, L"Marker"},
      {kToolChalk, L"Chalk"}, {kToolMembrane, L"Membrane"},
      {kToolObjectEraser, L"Object Eraser"}, {kToolPartialEraser, L"Partial Eraser"}};
  value.toolbarLabel = CreateWindowExW(0, L"STATIC", L"Brush / Eraser:", WS_CHILD | WS_VISIBLE,
                                       10, 8, 105, 24, value.window, nullptr, instance, nullptr);
  for (std::size_t index = 0; index < std::size(tools); ++index) {
    const auto& tool = tools[index];
    const auto button = CreateWindowExW(0, L"BUTTON", tool.label,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_TABSTOP,
        120 + static_cast<int>(index) * (kButtonWidth + kButtonGap), 6,
        kButtonWidth, 30, value.window, reinterpret_cast<HMENU>(tool.id), instance, nullptr);
    value.toolButtons.emplace(tool.id, button);
  }
  (void)selectWindowsTool(value, kToolVector);
}
struct NativePointerSample final {
  std::uint64_t pointer_id = 0;
  std::uint64_t sample_sequence = 0;
  std::uint64_t timestamp_us = 0;
  float x = 0.0F;
  float y = 0.0F;
  float pressure = 0.0F;
  std::uint32_t phase = ARC_POINTER_PHASE_MOVE;
  std::uint32_t provenance = ARC_SAMPLE_CONFIRMED_CURRENT;
};
State* state(HWND window) { return reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA)); }
void persistEvidence(const State& value);

bool attachPreviewTarget(State& value, std::uint32_t width,
                         std::uint32_t height) noexcept {
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (value.previewBridge == nullptr || value.previewProvider == nullptr ||
      width == 0U || height == 0U) return false;
  value.previewGeneration = value.previewProvider->generation();
  arc_preview_target_v0 target{};
  target.struct_size = sizeof(target);
  target.abi_version = ARC_ABI_VERSION;
  target.platform_kind = ARC_PLATFORM_WINDOWS;
  target.target_id = 1U;
  target.target_generation = value.previewGeneration;
  target.width_pixels = width;
  target.height_pixels = height;
  target.device_pixel_ratio = 1.0F;
  target.opaque_platform_handle = reinterpret_cast<std::uint64_t>(value.previewProvider);
  return value.previewBridge->Attach(target) == arc::Status::kOk;
#else
  (void)value;
  (void)width;
  (void)height;
  return false;
#endif
}

// Windows is an ARC realization only. Runtime owns preview/session semantics;
// this adapter translates the typed runtime seam into the ARC ABI and keeps
// platform preview state out of the canonical document/render path.
class WindowsArcRuntimeSinks final : public canvas::ink::ArcPreviewSink,
                                     public canvas::ink::CanonicalVisibilitySink {
 public:
  explicit WindowsArcRuntimeSinks(State& state) : state_(state) {}
  canvas::ink::PreviewSubmitResult begin(
      const canvas::ink::PreviewIdentity& identity,
      const canvas::ink::BrushPreviewDelta&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::PreviewSubmitResult::kCanonicalOnly;
    arc_preview_begin_v0 begin{};
    begin.struct_size = sizeof(begin); begin.abi_version = ARC_ABI_VERSION;
    begin.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; begin.stroke_id = identity.session;
    begin.view_id = 1U; begin.viewport_revision = identity.sessionGeneration;
    begin.target_generation = state_.previewGeneration;
    begin.brush.struct_size = sizeof(begin.brush); begin.brush.abi_version = ARC_ABI_VERSION;
    return state_.previewBridge->Begin(begin) == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::PreviewSubmitResult update(
      const canvas::ink::PreviewIdentity& identity,
      const canvas::ink::BrushPreviewDelta&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::PreviewSubmitResult::kCanonicalOnly;
    arc_preview_update_v0 update{};
    update.struct_size = sizeof(update); update.abi_version = ARC_ABI_VERSION;
    update.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; update.stroke_id = identity.session;
    update.preview_revision = ++revisions_[identity.session];
    update.view_id = 1U;
    update.viewport_revision = identity.sessionGeneration;
    update.target_generation = state_.previewGeneration;
    // The ARC protocol is lifecycle-only on Windows. Runtime's Skia preview
    // provider owns and renders BrushPreviewDelta.outline.
    return state_.previewBridge->Push(update) == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::PreviewSubmitResult cancel(
      const canvas::ink::PreviewIdentity& identity) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::PreviewSubmitResult::kCanonicalOnly;
    arc_preview_cancel_v0 cancel{sizeof(cancel), ARC_ABI_VERSION, identity.session,
                                 state_.previewGeneration, 1U, 0U};
    return state_.previewBridge->Cancel(cancel) == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::HandoffResult canonicalCommitted(
      const canvas::ink::CanonicalHandoffIdentity& identity,
      const canvas::semantic::CanonicalCommitRecord&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::HandoffResult::kIgnored;
    const auto revision = revisions_[identity.session];
    if (revision == 0U) return canvas::ink::HandoffResult::kRejected;
    arc_preview_seal_v0 seal{sizeof(seal), ARC_ABI_VERSION, identity.session,
                             revision, state_.previewGeneration};
    if (state_.previewBridge->SealInput(seal) != arc::Status::kOk) {
      return canvas::ink::HandoffResult::kIgnored;
    }
    arc_canonical_commit_v0 commit{sizeof(commit), ARC_ABI_VERSION, identity.session,
                                   revision, identity.commit.ordinal.value(),
                                   state_.previewGeneration, {identity.commit.runtime_epoch.value(),
                                                              identity.commit.ordinal.value()}};
    return state_.previewBridge->CanonicalCommitted(commit) == arc::Status::kOk
        ? canvas::ink::HandoffResult::kAccepted
        : canvas::ink::HandoffResult::kIgnored;
  }
  canvas::ink::HandoffResult canonicalVisible(
      const canvas::ink::CanonicalHandoffIdentity& identity,
      const canvas::render::FrameState&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::HandoffResult::kIgnored;
    arc_canonical_visible_v0 visible{};
    visible.struct_size = sizeof(visible); visible.abi_version = ARC_ABI_VERSION;
    visible.stroke_id = identity.session; visible.document_revision = identity.commit.ordinal.value();
    visible.target_generation = identity.surfaceGeneration.value();
    visible.handoff_token = {identity.commit.runtime_epoch.value(), identity.commit.ordinal.value()};
    visible.receipt.struct_size = sizeof(visible.receipt);
    visible.receipt.abi_version = ARC_ABI_VERSION;
    visible.receipt.evidence = ARC_EVIDENCE_DETERMINISTIC_ORACLE;
    visible.receipt.status = ARC_STATUS_OK;
    visible.receipt.target_generation = identity.surfaceGeneration.value();
    visible.receipt.presentation_id = identity.commit.ordinal.value();
    const auto result = state_.previewBridge->CanonicalVisible(visible);
    if (result != arc::Status::kOk) {
      return canvas::ink::HandoffResult::kIgnored;
    }
    // This is the only platform-side retirement point.  The runtime has
    // already validated the matching handoff token and the D3D12 canonical
    // provider has presented the frame before invoking this callback.
    hidePreviewPresentation(state_);
    return canvas::ink::HandoffResult::kAccepted;
  }
 private:
  State& state_;
  std::unordered_map<std::uint64_t, std::uint64_t> revisions_;
};

bool renderCanonical(State& value) {
#if defined(CANVAS_RENDER_HAS_SKIA)
  return value.canonicalRenderer != nullptr &&
         // While a mouse/pointer is down, the canonical redraw is only a
         // backing-surface update. The matching CanonicalVisible handoff must
         // happen after release, otherwise WM_PAINT clears the live Arc
         // preview before the user can see it.
         value.host->presentCanonicalFrame(value.host->canonicalFrameCount() + 1U, 0.0,
                                           value.activeKeys.empty());
#else
  return false;
#endif
}

bool submitMouseSample(HWND window, State& value, UINT message, WPARAM wParam, LPARAM lParam) {
  constexpr UINT kMousePointerId = 0xFFFFFFFFU;
  const auto tickNs = static_cast<std::uint64_t>(
      (std::max)(GetTickCount64(), static_cast<ULONGLONG>(1))) * 1'000'000ULL;
  value.lastMouseTimestampNs = (std::max)(value.lastMouseTimestampNs + 1ULL, tickNs);
  bool begin = false;
  bool end = false;
  std::uint32_t phase = ARC_POINTER_PHASE_MOVE;
  if (message == WM_LBUTTONDOWN) {
    // A system configured with mouse-in-pointer can deliver the same physical
    // click through both WM_LBUTTON* and WM_POINTER/PT_MOUSE.  The first
    // lifecycle message owns the session; a duplicate down must not create a
    // second Runtime pointer session.
    if (value.activeKeys.contains(kMousePointerId)) return false;
    begin = true;
    phase = ARC_POINTER_PHASE_DOWN;
  } else if (message == WM_LBUTTONUP) {
    if (!value.activeKeys.contains(kMousePointerId)) return false;
    end = true;
    phase = ARC_POINTER_PHASE_UP;
  } else if ((wParam & MK_LBUTTON) == 0U || !value.activeKeys.contains(kMousePointerId)) {
    return false;
  }
  if (begin) {
    ++value.stroke;
    enablePreviewPresentation(value);
    SetCapture(window);
  }

  const auto x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
  const auto y = static_cast<float>(static_cast<short>(HIWORD(lParam)) - State::kToolbarHeight);
  if (begin && y < 0.0F) return false;
  NativePointerSample sample{};
  sample.pointer_id = kMousePointerId;
  sample.sample_sequence = ++value.mouseSampleSequence;
  sample.timestamp_us = value.lastMouseTimestampNs / 1000U;
  sample.x = x;
  sample.y = y;
  sample.pressure = 0.5F;
  sample.phase = phase;
  sample.provenance = ARC_SAMPLE_CONFIRMED_CURRENT;
  canvas::input::PlatformPointerBatch common;
  common.samples.push_back({1U, kMousePointerId, sample.sample_sequence,
      sample.timestamp_us * 1000U, sample.x, sample.y, sample.pressure, 0.0F, 0.0F,
      {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
      phase == ARC_POINTER_PHASE_DOWN ? canvas::input::PointerPhase::kDown
      : phase == ARC_POINTER_PHASE_UP ? canvas::input::PointerPhase::kUp
                                      : canvas::input::PointerPhase::kMove});
  const bool accepted = value.host->acceptPlatformBatch(common, value.lastMouseTimestampNs);
  if (value.pointerDiagnostic) {
    value.pointerDiagnostic << "mouse accepted=" << accepted << " phase=" << message
        << " sequence=" << sample.sample_sequence
        << " timestamp=" << value.lastMouseTimestampNs << std::endl;
  }
  if (!accepted) return false;
  if (begin) {
    const auto key = value.host->platformKey(1U, kMousePointerId);
    if (!key) return false;
    value.activeKeys[kMousePointerId] = *key;
    value.pointerStrokes[kMousePointerId] = value.host->platformStrokeId(*key).value_or(value.stroke);
    value.maxConcurrentPointers = (std::max)(value.maxConcurrentPointers, value.activeKeys.size());
  }
  value.deviceId = kMousePointerId;
  const auto key = value.activeKeys.at(kMousePointerId);
  value.trace.push_back({key.source, kMousePointerId, key.generation,
                          value.lastMouseTimestampNs / 1'000'000ULL, "mouse",
                          phase == ARC_POINTER_PHASE_DOWN ? "down" :
                          (phase == ARC_POINTER_PHASE_UP ? "up" : "move"),
                          x, y, sample.pressure, 1U, 0.0F, 0.0F});
  if (end) persistEvidence(value);
  if (end) {
    value.activeKeys.erase(kMousePointerId);
    value.pointerStrokes.erase(kMousePointerId);
    if (GetCapture() == window) ReleaseCapture();
    // Complete the canonical-visible receipt on pointer-up. Waiting for a
    // later WM_PAINT leaves the first preview session active when the paint
    // message is delayed behind input or composition work.
    if (value.activeKeys.empty()) {
      if (!value.host->presentCanonicalFrame(
              value.host->canonicalFrameCount() + 1U, 0.0, true)) {
        return false;
      }
      // Canonical presentation has completed and the runtime handoff has
      // been offered. Keep the visual retirement synchronous with this input
      // boundary; the runtime still owns the semantic session until its
      // matching CanonicalVisible receipt.
      // The canonical provider has presented this frame. Hide the platform
      // overlay immediately even if Runtime keeps the session pending while
      // it finishes the matching token receipt.
      hidePreviewPresentation(value);
      value.canonicalFrameReady = true;
    }
  }
  value.previewDirty = true;
  if (!end && value.previewPresentationEnabled) {
    // Submit the latest preview in the input message itself. The timer remains
    // only as a recovery path; normal drawing must not wait behind WM_TIMER.
    (void)value.host->presentBrushPreview();
    value.previewDirty = false;
  }
  if (end) {
    RECT canvasRect{}; GetClientRect(window, &canvasRect);
    canvasRect.top = State::kToolbarHeight;
    InvalidateRect(window, &canvasRect, FALSE);
  }
  return true;
}

bool submitTouchInput(HWND window, State& value, HTOUCHINPUT touchHandle,
                      UINT touchCount) {
  if (touchHandle == nullptr || touchCount == 0U) return false;
  std::vector<TOUCHINPUT> inputs(touchCount);
  if (GetTouchInputInfo(touchHandle, touchCount, inputs.data(), sizeof(TOUCHINPUT)) == FALSE) {
    return false;
  }
  bool acceptedAny = false;
  for (const auto& touch : inputs) {
    const auto pointerId = static_cast<std::uint64_t>(touch.dwID);
    const bool begin = (touch.dwFlags & TOUCHEVENTF_DOWN) != 0U;
    const bool end = (touch.dwFlags & TOUCHEVENTF_UP) != 0U;
    if (!begin && !end && (touch.dwFlags & TOUCHEVENTF_MOVE) == 0U) continue;
    if (begin && value.activeKeys.contains(pointerId)) continue;
    if (!begin && !value.activeKeys.contains(pointerId)) continue;
    POINT point{static_cast<LONG>(touch.x / 100), static_cast<LONG>(touch.y / 100)};
    if (ScreenToClient(window, &point) == FALSE) continue;
    const auto sequence = ++value.pointerSampleSequence;
    const auto tickNs = static_cast<std::uint64_t>(
        (std::max)(GetTickCount64(), static_cast<ULONGLONG>(1))) * 1'000'000ULL;
    value.lastPointerTimestampNs = (std::max)(value.lastPointerTimestampNs + 1ULL, tickNs);
    const auto phase = begin ? canvas::input::PointerPhase::kDown
        : end ? canvas::input::PointerPhase::kUp : canvas::input::PointerPhase::kMove;
    canvas::input::PlatformPointerBatch batch;
    batch.samples.push_back({1U, pointerId, sequence, value.lastPointerTimestampNs,
        static_cast<float>(point.x),
        static_cast<float>(point.y - State::kToolbarHeight),
        0.5F, static_cast<float>(touch.cxContact / 100),
        static_cast<float>(touch.cyContact / 100), {}, {},
        canvas::input::SampleProvenance::kConfirmedCurrent, phase});
    const bool accepted = value.host->acceptPlatformBatch(batch, value.lastPointerTimestampNs);
    if (value.pointerDiagnostic) {
      value.pointerDiagnostic << "touch id=" << pointerId << " flags=" << touch.dwFlags
          << " x=" << point.x << " y=" << point.y << " sequence=" << sequence
          << " accepted=" << accepted << std::endl;
    }
    if (!accepted) continue;
    acceptedAny = true;
    if (begin) {
      const auto key = value.host->platformKey(1U, pointerId);
      if (!key) continue;
      value.activeKeys[pointerId] = *key;
      value.pointerStrokes[pointerId] = value.host->platformStrokeId(*key).value_or(++value.stroke);
      enablePreviewPresentation(value);
    }
    if (end) {
      value.activeKeys.erase(pointerId);
      value.pointerStrokes.erase(pointerId);
      if (value.activeKeys.empty() && GetCapture() == window) ReleaseCapture();
    }
    value.previewDirty = true;
    if (!end && value.previewPresentationEnabled) {
      (void)value.host->presentBrushPreview();
      value.previewDirty = false;
    }
    if (end && value.activeKeys.empty()) {
      (void)value.host->presentCanonicalFrame(value.host->canonicalFrameCount() + 1U, 0.0, true);
      hidePreviewPresentation(value);
    }
  }
  return acceptedAny;
}

void cancelPointer(State& value, std::uint64_t pointerId, std::uint64_t timestampMs) {
  const auto keyEntry = value.activeKeys.find(pointerId);
  if (keyEntry == value.activeKeys.end()) return;
  const auto key = keyEntry->second;
  const auto strokeEntry = value.pointerStrokes.find(pointerId);
  if (strokeEntry != value.pointerStrokes.end()) {
    (void)value.host->cancelBrushSession(pointerId);
  }
  value.trace.push_back({key.source, static_cast<std::uint32_t>(pointerId), key.generation,
                         timestampMs, "unknown", "cancel", 0.0F, 0.0F, 0.0F, 0U,
                         0.0F, 0.0F});
  value.activeKeys.erase(pointerId);
  value.pointerStrokes.erase(pointerId);
  persistEvidence(value);
}

void cancelAllPointers(State& value, std::uint64_t timestampMs) {
  for (const auto& [pointerId, key] : value.activeKeys) {
    value.trace.push_back({key.source, static_cast<std::uint32_t>(pointerId), key.generation,
                           timestampMs, "unknown", "cancel", 0.0F, 0.0F, 0.0F, 0U,
                           0.0F, 0.0F});
  }
  value.host->cancelAllPointers();
  value.activeKeys.clear();
  value.pointerStrokes.clear();
  if (GetCapture() == value.window) ReleaseCapture();
  persistEvidence(value);
}

void persistEvidence(const State& value) {
  wchar_t buffer[32768]{};
  const DWORD length = GetEnvironmentVariableW(L"AXIOM_INK_EVIDENCE_DIR", buffer,
                                                static_cast<DWORD>(std::size(buffer)));
  if (length == 0 || length >= std::size(buffer)) return;
  const std::filesystem::path directory(buffer);
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) return;
  std::ofstream traceFile(directory / "pointer-trace.json", std::ios::binary | std::ios::trunc);
  traceFile << canvas::ink_playground::windows_input::serializePointerTrace(value.trace,
                                                                            value.deviceId);
  const auto& hud = value.host->hud();
  std::ofstream hudFile(directory / "hud-telemetry.json", std::ios::binary | std::ios::trunc);
  hudFile << "{\n  \"schema_version\": \"0.1\",\n"
          << "  \"sample_hz\": " << hud.sampleHz << ",\n"
          << "  \"batch_size\": " << hud.batch << ",\n"
          << "  \"queue_age_ms\": " << hud.queueAgeMs << ",\n"
          << "  \"ink_processing_ms\": " << hud.inkMs << ",\n"
          << "  \"preview_revision\": " << hud.previewRevision << ",\n"
          << "  \"prediction_depth\": " << hud.predictionDepth << ",\n"
          << "  \"presentation_evidence_kind\": \"" << hud.presentEvidenceKind << "\",\n"
          << "  \"pending_handoff_count\": " << hud.pendingHandoffCount << ",\n"
          << "  \"frame_time_ms\": " << hud.frameMs << ",\n"
          << "  \"active_pointer_count\": " << value.activeKeys.size() << ",\n"
          << "  \"max_concurrent_pointers\": " << value.maxConcurrentPointers << ",\n"
          << "  \"multi_contact_policy\": \""
          << canvas::ink_playground::windows_input::multiContactPolicyNameUtf8(
                 value.host->multiContactPolicy()) << "\",\n"
          << "  \"viewport_scale\": " << value.host->viewportGesture().scale << ",\n"
          << "  \"viewport_translation_x\": "
          << value.host->viewportGesture().translationX << ",\n"
          << "  \"viewport_translation_y\": "
          << value.host->viewportGesture().translationY << ",\n"
          << "  \"render_path\": {\n"
          << "    \"renderer\": \"Skia Ganesh D3D12\",\n"
          << "    \"surface_type\": \"D3D12 composition swap chain\",\n"
#if defined(CANVAS_RENDER_HAS_SKIA)
          << "    \"submission_count\": "
          << (value.canonicalRenderer ? value.canonicalRenderer->submissionCount() : 0) << ",\n"
          << "    \"readback_count\": "
          << (value.canonicalProvider ? value.canonicalProvider->readbackCount() : 0) << ",\n"
#else
          << "    \"submission_count\": 0,\n    \"readback_count\": 0,\n"
#endif
          << "    \"cpu_copy_count\": " << value.cpuCopyCount << ",\n"
          << "    \"present_count\": " << value.presentCount << ",\n"
          << "    \"viewport_transform_applied\": \"true\"\n  }\n}\n";
  std::ofstream baselineFile(directory / "observation.json", std::ios::binary | std::ios::trunc);
  wchar_t modulePath[MAX_PATH]{};
  const DWORD moduleLength = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  std::error_code artifactError;
  const auto artifact = moduleLength == 0 ? std::string{} :
      std::filesystem::path(modulePath).filename().string() + ":" +
      std::to_string(std::filesystem::file_size(modulePath, artifactError));
#if defined(CANVAS_RENDER_HAS_SKIA)
  const auto submissions = value.canonicalRenderer ? value.canonicalRenderer->submissionCount() : 0;
  const auto readbacks = value.canonicalProvider ? value.canonicalProvider->readbackCount() : 0;
  const auto copies = value.cpuCopyCount +
      (value.canonicalProvider ? value.canonicalProvider->cpuCopyCount() : 0);
#else
  const std::uint64_t submissions = 0;
  const std::uint64_t readbacks = 0;
  const auto copies = value.cpuCopyCount;
#endif
  canvas::ink_playground::BaselineRenderPath renderPath{
      "WM_POINTER history→PlatformPointerBatch", "C++ InkPlaygroundHost", "Skia Ganesh D3D12",
      "D3D12 swap chain→DirectComposition", submissions, readbacks, copies,
      value.presentCount, "true"};
  renderPath.canonicalProviderIdentity = "windows-canonical-d3d12-skia";
  renderPath.previewProviderIdentity = "windows-skia-arc-preview";
  baselineFile << canvas::ink_playground::platformBrushBaselineObservationJson(
      "windows", "PHYSICAL", artifactError ? std::string{} : artifact, *value.host,
      renderPath);
}

void paint(HWND window, State& value) {
  PAINTSTRUCT ps{};
  HDC dc = BeginPaint(window, &ps);
  RECT rect{};
  GetClientRect(window, &rect);
  FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  if (value.activeKeys.empty() && !value.canonicalFrameReady) {
    (void)renderCanonical(value);
    value.canonicalFrameReady = true;
  }
  SetBkMode(dc, TRANSPARENT);
  const auto& hud = value.host->hud();
  std::wstringstream status;
  const wchar_t* qualificationState = value.host->previewActive()
                                          ? L"PREVIEW_OR_OVERLAP"
                                          : L"CANONICAL_ONLY";
  status << L"Axiom Ink Playground | HWND ready | WM_POINTER enabled | samples: "
         << value.trace.size() << L" | batch: " << hud.batch
         << L" | pointers: " << value.activeKeys.size()
         << L" | mode: " << canvas::ink_playground::windows_input::multiContactPolicyName(
                                value.host->multiContactPolicy())
         << L" | viewport: " << value.host->viewportGesture().scale << L"x @ "
         << value.host->viewportGesture().translationX << L","
         << value.host->viewportGesture().translationY
         << L" | preview_present: " << value.host->previewPresentCount()
         << L" | state: " << qualificationState
         << L" | Arc preview: Skia layered"
         << L" | runtime preview: " << (value.runtimePreviewVisible ? L"ON" : L"OFF")
         << L" | SPACE = preview | P = pointer mode";
  const auto text = status.str();
  SetTextColor(dc, RGB(30, 30, 30));
  TextOutW(dc, 16, 34, text.c_str(), static_cast<int>(text.size()));
  EndPaint(window, &ps);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* value = state(window);
  if (message == WM_NCCREATE) {
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(
        static_cast<CREATESTRUCTW*>(reinterpret_cast<void*>(lParam))->lpCreateParams));
    return TRUE;
  }
  if (value != nullptr && value->pointerDiagnostic &&
      (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_MOUSEMOVE ||
       message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP ||
       message == WM_POINTERCAPTURECHANGED || message == WM_TOUCH)) {
    canvas::ink_playground::windows_input::logInputMessage(value->pointerDiagnostic, "owner",
                                                         window, message, wParam);
  }
  if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_MOUSEMOVE) {
    if (value != nullptr) {
      if (value->pointerDiagnostic && message != WM_MOUSEMOVE) {
        value->pointerDiagnostic << "mouse phase=" << message << " x="
            << static_cast<short>(LOWORD(lParam)) << " y="
            << static_cast<short>(HIWORD(lParam)) << std::endl;
      }
      const bool promoted =
          canvas::ink_playground::windows_input::isPromotedPointerMouseMessage(
              GetMessageExtraInfo());
      // Accept promoted mouse messages only until the device proves that it
      // exposes native WM_POINTER/WM_TOUCH contacts. This keeps legacy touch
      // hardware usable without duplicating native multi-contact sessions.
      if (!promoted || !value->nativeTouchChannelSeen) {
        (void)submitMouseSample(window, *value, message, wParam, lParam);
      }
    }
    return 0;
  }
  if (message == WM_COMMAND && value != nullptr && HIWORD(wParam) == BN_CLICKED) {
    selectWindowsTool(*value, LOWORD(wParam));
    return 0;
  }
  if (message == WM_TIMER && value != nullptr && wParam == State::kRenderTimerId) {
    if (value->previewDirty && value->previewPresentationEnabled) {
      (void)value->host->presentBrushPreview();
      value->previewDirty = false;
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
    } else if (value->previewDirty) {
      // Drop queued preview presentation after pointer-up; Runtime retains
      // the session until CanonicalVisible, but no stale frame is submitted.
      value->previewDirty = false;
    }
    return 0;
  }
  if (message == WM_SIZE && value != nullptr) {
#if defined(CANVAS_RENDER_HAS_SKIA)
    const auto width = static_cast<std::uint32_t>(LOWORD(lParam));
    const auto canvasHeight = static_cast<int>(HIWORD(lParam)) - State::kToolbarHeight;
    const auto height = static_cast<std::uint32_t>((std::max)(canvasHeight, 0));
    if (width != 0U && height != 0U && value->previewProvider != nullptr &&
        value->host->resizeSurface(width, height) &&
        attachPreviewTarget(*value, width, height)) {
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
    }
#else
    (void)lParam;
#endif
    return 0;
  }
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP) {
    if (value == nullptr) return 0;
    const UINT pointerId = GET_POINTERID_WPARAM(wParam);
    POINTER_INFO info{};
    if (!GetPointerInfo(pointerId, &info)) {
      if (value->pointerDiagnostic) {
        value->pointerDiagnostic << "pointer-info-failed phase=" << message
            << " id=" << pointerId << " error=" << GetLastError() << std::endl;
      }
      return 0;
    }
    const bool diagnose = static_cast<bool>(value->pointerDiagnostic);
    if (diagnose) {
      value->pointerDiagnostic << "received phase=" << message << " id=" << pointerId
          << " type=" << info.pointerType << " flags=" << info.pointerFlags
          << " time=" << info.dwTime << " device="
          << reinterpret_cast<std::uintptr_t>(info.sourceDevice)
          << " capture=" << reinterpret_cast<std::uintptr_t>(GetCapture())
          << " active=" << value->activeKeys.size() << std::endl;
    }
    if (info.pointerType == PT_MOUSE) {
      // Some Windows configurations expose the mouse through WM_POINTER even
      // when the application opted into button messages.  Route it through
      // the exact same adapter as WM_LBUTTON*, preserving one shared Runtime
      // session and avoiding a platform-specific mouse implementation.
      POINT clientPoint = info.ptPixelLocation;
      if (ScreenToClient(window, &clientPoint) == FALSE) return 0;
      const auto mouseMessage = message == WM_POINTERDOWN ? WM_LBUTTONDOWN
          : message == WM_POINTERUP ? WM_LBUTTONUP : WM_MOUSEMOVE;
      const WPARAM mouseWParam = message == WM_POINTERUP ? 0U : MK_LBUTTON;
      const auto mouseLParam = MAKELPARAM(static_cast<short>(clientPoint.x),
                                          static_cast<short>(clientPoint.y));
      (void)submitMouseSample(window, *value, mouseMessage, mouseWParam, mouseLParam);
      return 0;
    }
    value->nativeTouchChannelSeen = true;
    const auto phase = message == WM_POINTERDOWN ? ARC_POINTER_PHASE_DOWN :
        (message == WM_POINTERUP ? ARC_POINTER_PHASE_UP : ARC_POINTER_PHASE_MOVE);
    bool begin = false;
    bool end = false;
    if (phase == ARC_POINTER_PHASE_DOWN) {
      begin = true;
    } else if (phase == ARC_POINTER_PHASE_UP) {
      end = value->activeKeys.contains(pointerId);
    } else if (!value->activeKeys.contains(pointerId)) {
      return 0;
    }
    if (begin) {
      ++value->stroke;
      // A previous pointer-up retires the transient presentation. Re-enable
      // it for every new native pointer session, just like the mouse path.
      enablePreviewPresentation(*value);
      value->pointerStrokes[pointerId] = value->stroke;
    }
    std::vector<POINTER_INFO> pointerHistory;
    UINT32 pointerHistoryCount = 0;
    // A fresh WM_POINTERDOWN commonly has no coalesced history yet.  That is
    // a valid first sample, not an input failure: Web/Android both submit the
    // current down event immediately and only add history on later moves.
    const auto historyProbe = GetPointerInfoHistory(pointerId, &pointerHistoryCount, nullptr);
    if (historyProbe == FALSE && GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        pointerHistoryCount != 0U) {
      pointerHistory.resize(pointerHistoryCount);
      if (GetPointerInfoHistory(pointerId, &pointerHistoryCount, pointerHistory.data()) != FALSE) {
        pointerHistory.resize(pointerHistoryCount);
      } else {
        pointerHistory.clear();
      }
    }
    if (pointerHistory.empty()) pointerHistory.push_back(info);
    if (phase == ARC_POINTER_PHASE_DOWN) {
      pointerHistory.assign(1U, info);
    } else {
      pointerHistory = canvas::ink_playground::windows_input::normalizePointerHistory(
          pointerHistory, false);
    }
    if (pointerHistory.empty()) pointerHistory.push_back(info);
    std::vector<POINTER_PEN_INFO> penHistory;
    if (info.pointerType == PT_PEN) {
      UINT32 penHistoryCount = 0;
      if (!GetPointerPenInfoHistory(pointerId, &penHistoryCount, nullptr) &&
          GetLastError() != ERROR_INSUFFICIENT_BUFFER) return 0;
      if (penHistoryCount != 0U) {
        penHistory.resize(penHistoryCount);
        if (!GetPointerPenInfoHistory(pointerId, &penHistoryCount, penHistory.data())) return 0;
        penHistory.resize(penHistoryCount);
        std::reverse(penHistory.begin(), penHistory.end());
        if (penHistory.size() > pointerHistory.size()) {
          penHistory.erase(penHistory.begin(),
                           penHistory.end() - static_cast<std::ptrdiff_t>(pointerHistory.size()));
        }
      }
    }
    std::vector<NativePointerSample> samples;
    samples.reserve(pointerHistory.size());
    float contactWidth = 0.0F;
    float contactHeight = 0.0F;
    if (info.pointerType == PT_TOUCH) {
      POINTER_TOUCH_INFO touchInfo{};
      if (GetPointerTouchInfo(pointerId, &touchInfo) != FALSE) {
        contactWidth = static_cast<float>(
            std::abs(touchInfo.rcContact.right - touchInfo.rcContact.left));
        contactHeight = static_cast<float>(
            std::abs(touchInfo.rcContact.bottom - touchInfo.rcContact.top));
      }
    }
    for (std::size_t index = 0; index < pointerHistory.size(); ++index) {
      const auto& history = pointerHistory[index];
      const auto historyPhase = index == 0U && phase == ARC_POINTER_PHASE_DOWN
                                    ? ARC_POINTER_PHASE_DOWN
                                    : (index + 1U == pointerHistory.size() && phase == ARC_POINTER_PHASE_UP
                                           ? ARC_POINTER_PHASE_UP
                                           : ARC_POINTER_PHASE_MOVE);
      float pressure = 0.5F;
      if (index < penHistory.size()) pressure = static_cast<float>(penHistory[index].pressure) / 1024.0F;
      NativePointerSample sample{};
      sample.pointer_id = pointerId;
      sample.sample_sequence = ++value->pointerSampleSequence;
      const auto wallTimestampNs = static_cast<std::uint64_t>(
          (std::max)(GetTickCount64(), static_cast<ULONGLONG>(1))) * 1'000'000ULL;
      const auto historyTimestampNs = static_cast<std::uint64_t>(history.dwTime) * 1'000'000ULL;
      const auto candidateTimestampNs = (std::max)(wallTimestampNs, historyTimestampNs);
      value->lastPointerTimestampNs =
          (std::max)(value->lastPointerTimestampNs + 1ULL, candidateTimestampNs);
      sample.timestamp_us = value->lastPointerTimestampNs / 1000ULL;
      POINT clientPoint = history.ptPixelLocation;
      if (ScreenToClient(window, &clientPoint) == FALSE) return 0;
      sample.x = static_cast<float>(clientPoint.x);
      sample.y = static_cast<float>(clientPoint.y - State::kToolbarHeight);
      sample.pressure = std::clamp(pressure, 0.0F, 1.0F);
      sample.phase = historyPhase;
      sample.provenance = ARC_SAMPLE_CONFIRMED_CURRENT;
      samples.push_back(sample);
    }
    value->deviceId = canvas::ink_playground::windows_input::functionalDeviceId(
        info.sourceDevice, static_cast<std::uint64_t>(pointerId));
    canvas::input::PlatformPointerBatch common;
    common.samples.reserve(samples.size());
    for (const auto& sample : samples) {
      const auto historyPhase = sample.phase == ARC_POINTER_PHASE_DOWN
          ? canvas::input::PointerPhase::kDown
          : sample.phase == ARC_POINTER_PHASE_UP ? canvas::input::PointerPhase::kUp
          : canvas::input::PointerPhase::kMove;
    common.samples.push_back({value->deviceId, pointerId, sample.sample_sequence,
          sample.timestamp_us * 1000U, sample.x, sample.y, sample.pressure, 0.0F, 0.0F,
          {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent, historyPhase});
    }
    const bool accepted = value->host->acceptPlatformBatch(common,
        value->lastPointerTimestampNs);
    if (diagnose) {
      value->pointerDiagnostic << "runtime phase=" << message << " id=" << pointerId
          << " source=" << value->deviceId << " samples=" << common.samples.size()
          << " sequence=" << common.samples.front().sequence
          << " timestamp=" << common.samples.front().timestampNs
          << " accepted=" << accepted << std::endl;
    }
    if (!accepted) return 0;
    if (begin) {
      const auto key = value->host->platformKey(value->deviceId, pointerId);
      if (diagnose) {
        value->pointerDiagnostic << "begin-key id=" << pointerId
            << " present=" << key.has_value();
        if (key) {
          value->pointerDiagnostic << " source=" << key->source
              << " generation=" << key->generation
              << " disposition=" << static_cast<int>(value->host->pointerDisposition(*key));
        }
        value->pointerDiagnostic << "" << std::endl;
      }
      if (!key) return 0;
      value->activeKeys[pointerId] = *key;
      value->pointerStrokes[pointerId] = value->host->platformStrokeId(*key).value_or(value->stroke);
    }
    // Register a new touch/pen key before recording its coalesced history.
    // WM_POINTERDOWN reaches this adapter before the host has exposed the
    // keyed session, so reading activeKeys.at(pointerId) in the history loop
    // would throw std::out_of_range and terminate the process on the first
    // real touch stroke.
    const auto active = value->activeKeys.find(pointerId);
    if (active == value->activeKeys.end()) return 0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
      const auto& sample = samples[index];
      const auto& history = pointerHistory[index];
      value->trace.push_back({active->second.source, pointerId, active->second.generation,
                              history.dwTime,
                              info.pointerType == PT_PEN ? "pen" :
                              (info.pointerType == PT_TOUCH ? "touch" : "mouse"),
                              sample.phase == ARC_POINTER_PHASE_DOWN ? "down" :
                              (sample.phase == ARC_POINTER_PHASE_UP ? "up" : "move"),
                              sample.x, sample.y, sample.pressure, pointerHistory.size(),
                              contactWidth, contactHeight});
    }
    if (diagnose) {
      const auto activeDisposition = value->host->pointerDisposition(active->second);
      value->pointerDiagnostic << "post-route id=" << pointerId
          << " active=" << value->activeKeys.size()
          << " preview_sessions=" << value->host->brushPreviewOutlines().size()
          << " disposition=" << static_cast<int>(activeDisposition)
          << " policy=" << static_cast<int>(value->host->multiContactPolicy())
          << " viewport=" << value->host->viewportGestureClaimed()
          << " canonical_strokes=" << value->host->submittedOperationCount() << std::endl;
      for (const auto& [contactId, contactKey] : value->activeKeys) {
        value->pointerDiagnostic << "active-contact id=" << contactId
            << " source=" << contactKey.source << " generation=" << contactKey.generation
            << " disposition=" << static_cast<int>(value->host->pointerDisposition(contactKey))
            << std::endl;
      }
    }
    // Runtime owns viewport arbitration and typed ARC preview publication.
    if (end) persistEvidence(*value);
    if (end) {
      value->activeKeys.erase(pointerId);
      value->pointerStrokes.erase(pointerId);
      if (value->activeKeys.empty()) {
        if (GetCapture() == window) ReleaseCapture();
        if (!value->host->presentCanonicalFrame(
                value->host->canonicalFrameCount() + 1U, 0.0, true)) {
          return 0;
        }
        hidePreviewPresentation(*value);
        value->canonicalFrameReady = true;
      }
    }
    value->previewDirty = true;
    if (!end && value->previewPresentationEnabled) {
      (void)value->host->presentBrushPreview();
      value->previewDirty = false;
    }
    if (end) {
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
    }
    return 0;
  }
  if (message == WM_TOUCH) {
    if (value != nullptr && value->pointerDiagnostic) {
      value->pointerDiagnostic << "wm-touch count=" << LOWORD(wParam) << std::endl;
    }
    if (value != nullptr) {
      value->nativeTouchChannelSeen = true;
      (void)submitTouchInput(window, *value,
          reinterpret_cast<HTOUCHINPUT>(lParam), LOWORD(wParam));
      CloseTouchInputHandle(reinterpret_cast<HTOUCHINPUT>(lParam));
      return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
  }
  if (message == WM_POINTERCAPTURECHANGED) {
    if (value != nullptr) {
      // WM_POINTERCAPTURECHANGED is delivered when Windows retargets a
      // pointer, including when another contact joins the same HWND. It is
      // not a semantic cancel and must not terminate the active BrushSession.
      if (value->pointerDiagnostic) {
        value->pointerDiagnostic << "capture-changed id="
            << GET_POINTERID_WPARAM(wParam) << " active="
            << value->activeKeys.size() << std::endl;
      }
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
    }
    return 0;
  }
  if (canvas::ink_playground::windows_input::cancelsAllActivePointers(message)) {
    if (value != nullptr && !value->activeKeys.empty()) {
      cancelAllPointers(*value, static_cast<std::uint64_t>(GetMessageTime()));
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
    }
    if (message == WM_DESTROY) {
      PostQuitMessage(0);
      return 0;
    }
    return 0;
  }
  if (message == WM_PAINT) { if (value != nullptr) paint(window, *value); return 0; }
  if (message == WM_KEYDOWN && wParam == VK_SPACE) {
    if (value != nullptr && (lParam & (1LL << 30)) == 0) {
      value->runtimePreviewVisible = !value->runtimePreviewVisible;
      value->host->setRuntimePreviewVisible(value->runtimePreviewVisible);
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
    }
    return 0;
  }
  if (message == WM_KEYDOWN && wParam == L'P') {
    if (value != nullptr && (lParam & (1LL << 30)) == 0 && value->activeKeys.empty()) {
      (void)value->host->setMultiContactPolicy(
          canvas::ink_playground::windows_input::nextMultiContactPolicy(
              value->host->multiContactPolicy()));
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
    }
    return 0;
  }
  if (message == WM_CHAR && wParam == L' ') {
    // WM_KEYDOWN owns the toggle. Consume the translated character so one
    // physical key press cannot toggle the debug mirror twice.
    return 0;
  }
  if (message == WM_ERASEBKGND) return 1;
  return DefWindowProcW(window, message, wParam, lParam);
}
}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
  WNDCLASSW klass{}; klass.hInstance = instance; klass.lpfnWndProc = WindowProc; klass.lpszClassName = L"AxiomInkPlayground";
  if (RegisterClassW(&klass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
  (void)canvas::ink_playground::windows_input::keepMouseOnButtonMessages();
  State value; value.host = std::make_unique<InkPlaygroundHost>();
  wchar_t diagnosticPath[32768]{};
  const DWORD diagnosticLength = GetEnvironmentVariableW(L"AXIOM_WINDOWS_POINTER_DIAG_FILE",
      diagnosticPath, static_cast<DWORD>(std::size(diagnosticPath)));
  if (diagnosticLength != 0U && diagnosticLength < std::size(diagnosticPath)) {
    value.pointerDiagnosticPath = std::filesystem::path(diagnosticPath);
  } else {
    wchar_t tempPath[MAX_PATH]{};
    const auto tempLength = GetTempPathW(static_cast<DWORD>(std::size(tempPath)), tempPath);
    if (tempLength != 0U && tempLength < std::size(tempPath)) {
      value.pointerDiagnosticPath = std::filesystem::path(tempPath) /
          L"axiom-windows-pointer-diagnostic.log";
    }
  }
  if (!value.pointerDiagnosticPath.empty()) {
    value.pointerDiagnostic.open(value.pointerDiagnosticPath,
                                 std::ios::binary | std::ios::trunc);
    if (!value.pointerDiagnostic) {
      value.pointerDiagnosticPath = std::filesystem::path(L"axiom-windows-pointer-diagnostic.log");
      value.pointerDiagnostic.open(value.pointerDiagnosticPath,
                                   std::ios::binary | std::ios::trunc);
    }
    if (value.pointerDiagnostic) {
      value.pointerDiagnostic << "startup pid=" << GetCurrentProcessId()
          << " diag=" << value.pointerDiagnosticPath.string() << std::endl;
    }
  }
#if defined(CANVAS_RENDER_HAS_SKIA)
  value.canonicalRenderer = std::make_unique<canvas::render::SkiaRenderer>();
#endif
  if (!value.host->bindSurface(1024, 768)) return 1;
#if defined(CANVAS_RENDER_HAS_SKIA)
#endif
  value.window = CreateWindowExW(0, klass.lpszClassName, L"Axiom Ink Playground",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
      CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, nullptr, nullptr, instance, &value);
  if (value.window == nullptr) return 1;
  const BOOL touchRegistered = RegisterTouchWindow(value.window, TWF_WANTPALM);
  const BOOL pointerTouchRegistered = RegisterPointerInputTarget(value.window, PT_TOUCH);
  const BOOL pointerPenRegistered = RegisterPointerInputTarget(value.window, PT_PEN);
  if (value.pointerDiagnostic) {
    value.pointerDiagnostic << "input-registration touch=" << touchRegistered
        << " pointer-touch=" << pointerTouchRegistered
        << " pointer-pen=" << pointerPenRegistered
        << " digitizer=" << GetSystemMetrics(SM_DIGITIZER)
        << " max-touches=" << GetSystemMetrics(SM_MAXIMUMTOUCHES)
        << " error=" << GetLastError() << std::endl;
  }
  if (value.pointerDiagnostic) {
    value.pointerDiagnostic << "window-created hwnd="
        << reinterpret_cast<std::uintptr_t>(value.window) << std::endl;
    SetPropW(value.window, canvas::ink_playground::windows_input::kDiagnosticStreamProperty,
             reinterpret_cast<HANDLE>(static_cast<std::ostream*>(&value.pointerDiagnostic)));
  }
  SetTimer(value.window, State::kRenderTimerId, State::kRenderIntervalMs, nullptr);
  createWindowsToolPalette(value, instance);
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto canonicalProvider = std::make_unique<canvas::ink_playground::WindowsD3D12SkiaSurfaceProvider>(
      value.window, true);
  if (canonicalProvider->resize(1024, 768).code != canvas::render::BackendSubmissionCode::kAccepted) return 1;
  auto* canonicalRaw = canonicalProvider.get();
  if (!value.host->registerSurfaceProvider("windows-d3d12-canonical",
                                           std::move(canonicalProvider))) return 1;
  value.canonicalProvider = canonicalRaw;
  auto previewProvider = std::make_unique<canvas::ink_playground::WindowsD3D12SkiaSurfaceProvider>(value.window);
  previewProvider->setOverlayOffset(0, State::kToolbarHeight);
  RECT client{}; GetClientRect(value.window, &client);
  const auto canvasHeight = static_cast<std::uint32_t>((std::max)(client.bottom - State::kToolbarHeight, 1L));
  if (previewProvider->resize(static_cast<std::uint32_t>(client.right), canvasHeight).code != canvas::render::BackendSubmissionCode::kAccepted) return 1;
  auto* previewRaw = previewProvider.get();
  if (!value.host->registerPreviewSurfaceProvider("windows-skia-arc-preview",
                                                  std::move(previewProvider))) return 1;
  value.previewProvider = previewRaw;
#endif
  value.previewBridge = std::make_unique<arc::Bridge>(arc::CreateNullBackend(), arc::CreateNullBackend());
  value.runtimeSinks = std::make_unique<WindowsArcRuntimeSinks>(value);
  value.host->setArcPreviewSink(value.runtimeSinks.get());
  value.host->setCanonicalVisibilitySink(value.runtimeSinks.get());
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (!value.host->resizeSurface(static_cast<std::uint32_t>(client.right), canvasHeight) ||
      !attachPreviewTarget(value, static_cast<std::uint32_t>(client.right), canvasHeight)) return 1;
#endif
  ShowWindow(value.window, show);
  UpdateWindow(value.window);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  return static_cast<int>(message.wParam);
}
#else
int main() { return 2; }
#endif
