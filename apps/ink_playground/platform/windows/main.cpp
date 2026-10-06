#include "ink_playground_host.hpp"
#include "platform_brush_baseline_observation.hpp"
#include "arc/arc.hpp"
#include "windows_pointer_utils.hpp"
#include "windows_input_diagnostics.hpp"
#include "windows_smoke_evidence.hpp"
#include "canvas/render/canonical_handoff.hpp"
#include "canvas/ink/arc_runtime_sinks.hpp"
#include "windows_d3d12_skia_surface_provider.hpp"
#include "canvas/debug_ui/windows_host.hpp"
#include "canvas/debug_ui/surface_debug_queue.hpp"
#include "canvas/debug_ui/debug_command_queue.hpp"
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
#include <condition_variable>
#include <atomic>
#include <memory>
#include <string>
#include <sstream>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
using canvas::ink_playground::InkPlaygroundHost;
using canvas::ink_playground::PreviewPresentationCapture;
class WindowsArcRuntimeSinks;
class WindowsRuntimeFacade;
class WindowsAxiomDebugControl;
class WindowsPlatformDebugControl;
class WindowsArcDiagnostics;
class WindowsPlatformDiagnostics;
class WindowsTelemetry;

struct State { HWND window = nullptr; std::unique_ptr<InkPlaygroundHost> host;
  static constexpr int kToolbarHeight = 56;
  static constexpr UINT kRenderTimerId = 0xA710;
  static constexpr UINT kRenderIntervalMs = 16;
  static constexpr UINT kOverlaySyncMessage = WM_APP + 0x31;
  static constexpr UINT kDeferredResizeMessage = WM_APP + 0x32;
  static constexpr UINT kRenderWakeMessage = WM_APP + 0x33;
  static constexpr UINT kPreviewPresentedMessage = WM_APP + 0x34;
  static constexpr UINT kResizeDebounceTimerId = 0xA711;
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
  canvas::debug_ui::InputCaptureGate inputCapture;
  std::uint64_t inputSequence = 0;
  std::unordered_map<std::uint64_t, canvas::debug_ui::DebugInputSequence>
      canvasInputSequences;
  std::vector<canvas::ink_playground::windows_input::PointerEvidenceSample> trace;
  std::size_t maxConcurrentPointers = 0;
  std::uint64_t cpuCopyCount = 0;
  std::uint64_t presentCount = 0;
  bool runtimePreviewVisible = true;
  // Presentation gate for the Windows-only overlay/fallback. Runtime keeps
  // the preview session alive until CanonicalVisible, but the platform
  // presentation must disappear immediately on the final pointer-up so a
  // slow canonical raster/readback cannot leave a stale preview on screen.
  std::atomic_bool previewPresentationEnabled{true};
  std::atomic_bool previewDirty{false};
  std::atomic_bool renderWakePosted{false};
  bool evidenceDirty = false;
  std::atomic_bool canonicalPending{false};
  std::mutex previewPumpMutex;
  std::condition_variable previewPumpCv;
  bool previewPumpPending = false;
  bool previewPumpStop = false;
  std::thread previewPump;
  std::mutex previewCompletionMutex;
  std::optional<PreviewPresentationCapture> previewCompletion;
  std::uint64_t previewCompletionPresentCount = 0;
  bool debugF12Down = false;
  canvas::runtime::SurfaceMode canonicalSurfaceMode =
      canvas::runtime::SurfaceMode::kPlatformDefault;
  std::unique_ptr<WindowsArcRuntimeSinks> runtimeSinks;
  bool canonicalFrameReady = false;
  bool resizePosted = false;
  bool resizeInProgress = false;
  std::uint32_t pendingWidth = 0;
  std::uint32_t pendingHeight = 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::SkiaRenderer> canonicalRenderer;
  canvas::ink_playground::WindowsD3D12SkiaSurfaceProvider* canonicalProvider = nullptr;
  canvas::ink_playground::WindowsD3D12SkiaSurfaceProvider* previewProvider = nullptr;
#endif
  std::unique_ptr<canvas::debug_ui::WindowsDebugUiHost> debugUi;
  canvas::debug_ui::MutexCopySnapshotChannel debugSnapshots;
  std::unique_ptr<WindowsRuntimeFacade> runtimeFacade;
  std::unique_ptr<WindowsAxiomDebugControl> axiomDebugControl;
  std::unique_ptr<WindowsPlatformDebugControl> platformDebugControl;
  std::unique_ptr<WindowsArcDiagnostics> arcDiagnostics;
  std::unique_ptr<WindowsPlatformDiagnostics> platformDiagnostics;
  std::unique_ptr<WindowsTelemetry> telemetry;
  canvas::runtime::SurfaceModeReceipt lastSurfaceReceipt{};
  bool hasSurfaceReceipt = false;
  canvas::runtime::ProductControlReceipt lastProductReceipt{};
  bool hasProductReceipt = false;
  int selectedTool = 4101;
  int selectionPreviousTool = 4101;
  std::unordered_set<std::uint64_t> selectionPointers;
};
void requestPreviewRender(State& value) noexcept;
enum : int {
  kToolVector = 4101, kToolMarker, kToolChalk, kToolMembrane,
  kToolObjectEraser, kToolPartialEraser, kToolSelection
};

bool selectWindowsTool(State& value, int command) {
  bool selected = false;
  if (command == kToolVector || command == kToolMarker ||
      command == kToolChalk || command == kToolMembrane) {
    const char* profile = command == kToolVector ? "vector-solid-v1" :
        command == kToolMarker ? "marker-flat-v1" :
        command == kToolChalk ? "chalk-grain-v1" : "membrane-v1";
    const auto revision = command == kToolChalk ? 4U : 1U;
    selected = value.host->setSelectionMode(false) &&
               value.host->selectTool(InkPlaygroundHost::ToolMode::kBrush) &&
               value.host->selectBrushProfile(profile, revision);
  } else if (command == kToolObjectEraser) {
    selected = value.host->setSelectionMode(false) &&
               value.host->selectTool(InkPlaygroundHost::ToolMode::kObjectEraser);
  } else if (command == kToolPartialEraser) {
    selected = value.host->setSelectionMode(false) &&
               value.host->selectTool(InkPlaygroundHost::ToolMode::kPartialEraser);
  } else if (command == kToolSelection) {
    if (!value.host->selectionMode()) value.selectionPreviousTool = value.selectedTool;
    selected = value.host->setSelectionMode(true);
  } else {
    return false;
  }
  if (!selected) return false;
  value.selectedTool = command;
  for (const auto& [id, button] : value.toolButtons) {
    SendMessageW(button, BM_SETCHECK, id == command ? BST_CHECKED : BST_UNCHECKED, 0);
  }
  InvalidateRect(value.window, nullptr, FALSE);
  return true;
}

bool beginCanvasInput(State& value, std::uint64_t pointerId) noexcept {
  if (value.canvasInputSequences.contains(pointerId)) return true;
  const canvas::debug_ui::DebugInputSequence sequence{pointerId, ++value.inputSequence};
  if (value.inputCapture.begin(sequence, canvas::debug_ui::DebugInputOwner::kCanvas) !=
      canvas::debug_ui::DebugInputOwner::kCanvas) {
    return false;
  }
  value.canvasInputSequences.emplace(pointerId, sequence);
  return true;
}

bool canvasOwnsInput(const State& value, std::uint64_t pointerId) noexcept {
  const auto it = value.canvasInputSequences.find(pointerId);
  if (it == value.canvasInputSequences.end()) return false;
  const auto owner = value.inputCapture.owner(it->second);
  return owner.has_value() && *owner == canvas::debug_ui::DebugInputOwner::kCanvas;
}

void endCanvasInput(State& value, std::uint64_t pointerId) noexcept {
  const auto it = value.canvasInputSequences.find(pointerId);
  if (it == value.canvasInputSequences.end()) return;
  (void)value.inputCapture.terminal(it->second);
  value.canvasInputSequences.erase(it);
}

void clearCanvasInput(State& value) noexcept {
  for (const auto& [pointerId, sequence] : value.canvasInputSequences) {
    (void)pointerId;
    (void)value.inputCapture.terminal(sequence);
  }
  value.canvasInputSequences.clear();
  value.selectionPointers.clear();
}

class WindowsRuntimeFacade final : public canvas::runtime::RuntimeFacade {
 public:
  explicit WindowsRuntimeFacade(State& state) : state_(state) {}
  [[nodiscard]] canvas::runtime::RuntimeDiagnosticsSnapshot readDiagnostics() const noexcept override {
    const auto viewport = state_.host->viewportGesture();
    return {1U, state_.host->semanticGeneration().value(),
            static_cast<std::uint64_t>(state_.host->submittedOperationCount()), 1U,
            static_cast<std::uint32_t>(state_.selectedTool),
            state_.host->surface().generation, viewport.scale,
            viewport.translationX, viewport.translationY,
            state_.host->canUndo(), state_.host->canRedo(),
            0U, 0U, static_cast<std::uint64_t>(state_.host->submittedOperationCount()), 0U,
            state_.host->selectionMode(),
            static_cast<std::uint32_t>(state_.host->selectedObjectCount()),
            state_.host->selectedPrimaryObjectValue()};
  }
  [[nodiscard]] canvas::runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override {
    const auto diagnostics = readDiagnostics();
    return {diagnostics.runtimeGeneration, diagnostics.documentGeneration,
            diagnostics.documentRevision, diagnostics.viewGeneration,
            diagnostics.surfaceGeneration, diagnostics.toolId,
            state_.host->selectedBrushProfile() == "vector-solid-v1" ? 1U :
                state_.host->selectedBrushProfile() == "marker-flat-v1" ? 2U :
                state_.host->selectedBrushProfile() == "chalk-grain-v1" ? 3U : 4U,
            state_.host->selectedBrushRevision(),
            state_.host->toolMode() == InkPlaygroundHost::ToolMode::kObjectEraser ? 1U :
                state_.host->toolMode() == InkPlaygroundHost::ToolMode::kPartialEraser ? 2U : 0U,
            diagnostics.cameraScale, diagnostics.cameraTranslationX,
            diagnostics.cameraTranslationY, diagnostics.canUndo, diagnostics.canRedo,
            diagnostics.selectionMode, diagnostics.selectedObjectCount,
            diagnostics.selectedPrimaryObject};
  }
  [[nodiscard]] canvas::runtime::ProductControlReceipt submitProductControl(
      const canvas::runtime::ProductControlRequest& request) noexcept override {
    canvas::runtime::ProductControlReceipt receipt{request.requestId,
        canvas::runtime::ProductControlState::kRejected, 1U};
    if (request.runtimeGeneration != 0U && request.runtimeGeneration != 1U) return receipt;
    if (request.action == canvas::runtime::ProductControlAction::kSetBrush) {
      static constexpr const char* kProfiles[] = {
          "vector-solid-v1", "marker-flat-v1", "chalk-grain-v1", "membrane-v1"};
      const auto profile = request.brushId >= 1U && request.brushId <= 4U
          ? kProfiles[request.brushId - 1U] : nullptr;
      const auto revision = request.brushRevision == 0U
          ? (request.brushId == 3U ? 4U : 1U) : request.brushRevision;
      if (profile != nullptr && state_.host->setSelectionMode(false) &&
          state_.host->selectTool(InkPlaygroundHost::ToolMode::kBrush) &&
          state_.host->selectBrushProfile(profile, revision)) {
        state_.selectedTool = static_cast<int>(request.toolId);
        receipt.state = canvas::runtime::ProductControlState::kApplied;
      }
      return receipt;
    }
    if (request.action == canvas::runtime::ProductControlAction::kSetCamera) {
      canvas::interaction::ViewportNavigationSample navigation{};
      navigation.anchorX = request.anchorX;
      navigation.anchorY = request.anchorY;
      navigation.deltaX = request.deltaX;
      navigation.deltaY = request.deltaY;
      navigation.scaleDelta = request.scaleDelta;
      navigation.kind = request.cameraAction == 2U
          ? canvas::interaction::ViewportNavigationKind::kBrowserGesture
          : canvas::interaction::ViewportNavigationKind::kWheelPan;
      if (state_.host->applyViewportNavigation(navigation)) {
        receipt.state = canvas::runtime::ProductControlState::kApplied;
      }
      return receipt;
    }
    if (request.action == canvas::runtime::ProductControlAction::kSetSelectionMode) {
      if (request.selectionMode && !state_.host->selectionMode()) {
        state_.selectionPreviousTool = state_.selectedTool;
      }
      if (state_.host->setSelectionMode(request.selectionMode)) {
        if (request.selectionMode) {
          state_.selectedTool = kToolSelection;
        } else if (state_.selectedTool == kToolSelection) {
          state_.selectedTool = state_.selectionPreviousTool;
        }
        state_.selectionPointers.clear();
        clearCanvasInput(state_);
        state_.canonicalFrameReady = false;
        receipt.state = canvas::runtime::ProductControlState::kApplied;
        state_.lastProductReceipt = receipt;
        state_.hasProductReceipt = true;
        InvalidateRect(state_.window, nullptr, FALSE);
      }
      return receipt;
    }
    if (request.action == canvas::runtime::ProductControlAction::kUndo ||
        request.action == canvas::runtime::ProductControlAction::kRedo) {
      const bool applied = request.action == canvas::runtime::ProductControlAction::kUndo
          ? state_.host->undo() : state_.host->redo();
      receipt.state = applied ? canvas::runtime::ProductControlState::kApplied
                              : canvas::runtime::ProductControlState::kRejected;
      state_.lastProductReceipt = receipt;
      state_.hasProductReceipt = true;
      if (applied) {
        state_.canonicalFrameReady = false;
        InvalidateRect(state_.window, nullptr, FALSE);
      }
      return receipt;
    }
    if (request.action == canvas::runtime::ProductControlAction::kSetEraser) {
      const auto mode = request.eraserId == 1U
          ? InkPlaygroundHost::ToolMode::kObjectEraser
          : request.eraserId == 2U ? InkPlaygroundHost::ToolMode::kPartialEraser
                                   : InkPlaygroundHost::ToolMode::kBrush;
      if (request.eraserId != 0U && state_.host->setSelectionMode(false) &&
          state_.host->selectTool(mode)) {
        state_.selectedTool = static_cast<int>(request.toolId);
        for (const auto& [id, button] : state_.toolButtons) {
          SendMessageW(button, BM_SETCHECK, id == request.toolId ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        InvalidateRect(state_.window, nullptr, FALSE);
        receipt.state = canvas::runtime::ProductControlState::kApplied;
      }
      return receipt;
    }
    if (request.action != canvas::runtime::ProductControlAction::kSetTool) {
      receipt.state = canvas::runtime::ProductControlState::kUnsupported;
      return receipt;
    }
    if (!selectWindowsTool(state_, static_cast<int>(request.toolId))) return receipt;
    receipt.state = canvas::runtime::ProductControlState::kApplied;
    return receipt;
  }
 private:
  State& state_;
};

class WindowsArcDiagnostics final : public canvas::runtime::ArcDiagnostics {
 public:
  explicit WindowsArcDiagnostics(const State& state) : state_(state) {}
  [[nodiscard]] canvas::runtime::ArcDiagnosticsSnapshot readArcDiagnostics() const noexcept override {
    return {state_.host->previewPresentCount(),
            static_cast<std::uint64_t>(state_.activeKeys.size()),
            state_.host->hud().batch,
            state_.host->hud().pendingHandoffCount,
            state_.host->previewActive()};
  }
 private:
  const State& state_;
};

class WindowsPlatformDiagnostics final : public canvas::runtime::PlatformDiagnostics {
 public:
  explicit WindowsPlatformDiagnostics(const State& state) : state_(state) {}
  [[nodiscard]] canvas::runtime::PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept override {
    const auto* canonical = state_.host->activeSurfaceProvider();
    const auto* preview = state_.host->previewSurfaceProvider();
    const auto& binding = state_.host->surface();
    return {canonical != nullptr ? canonical->generation() : 0U,
            preview != nullptr ? preview->generation() : 0U,
            binding.width, binding.height, 1.0F,
            state_.presentCount, state_.host->surfaceLostCount(), binding.available};
  }
 private:
  const State& state_;
};

class WindowsTelemetry final : public canvas::runtime::Telemetry {
 public:
  explicit WindowsTelemetry(const State& state) : state_(state) {}
  [[nodiscard]] canvas::runtime::TelemetrySnapshot readTelemetry() const noexcept override {
    const auto& hud = state_.host->hud();
    return {state_.pointerSampleSequence,
            static_cast<std::uint64_t>(state_.host->hud().batch),
            state_.host->previewPresentCount(),
            state_.host->canonicalFrameCount(),
            hud.inkMs};
  }
 private:
  const State& state_;
};

class WindowsAxiomDebugControl final : public canvas::runtime::AxiomDebugControl {
 public:
  explicit WindowsAxiomDebugControl(State& state) : state_(state), queue_() {}
  [[nodiscard]] canvas::runtime::AxiomDebugCommandReceipt enqueue(
      const canvas::runtime::AxiomDebugCommand& command) noexcept override {
    return queue_.enqueue(command, 1U, state_.host->semanticGeneration().value(),
                          state_.pointerSampleSequence);
  }
  [[nodiscard]] canvas::runtime::AxiomDebugCommandReceipt receipt(
      std::uint64_t requestId) const noexcept override {
    const auto result = queue_.receipt(requestId);
    return result.value_or(canvas::runtime::AxiomDebugCommandReceipt{
        requestId, canvas::runtime::AxiomDebugCommandState::kRejected});
  }
  void process() noexcept {
    const auto command = queue_.take(1U, state_.host->semanticGeneration().value(),
                                     state_.pointerSampleSequence);
    if (!command) return;
    const bool supported = command->kind ==
        canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw ||
        command->kind == canvas::runtime::AxiomDebugCommandKind::kResetRollingMetrics;
    if (supported) InvalidateRect(state_.window, nullptr, FALSE);
    (void)queue_.complete(command->requestId,
        supported ? canvas::runtime::AxiomDebugCommandState::kApplied
                  : canvas::runtime::AxiomDebugCommandState::kUnsupported,
        state_.presentCount, 1U, state_.host->semanticGeneration().value());
  }
 private:
  State& state_;
  canvas::runtime::BoundedAxiomDebugCommandQueue queue_;
};

class WindowsPlatformDebugControl final : public canvas::runtime::PlatformDebugControl {
 public:
  explicit WindowsPlatformDebugControl(State& state) : state_(state), queue_() {}
  [[nodiscard]] canvas::runtime::SurfaceModeReceipt enqueueSurfaceMode(
      const canvas::runtime::SurfaceModeRequest& request) noexcept override {
    const auto generation = state_.host->activeSurfaceProvider() == nullptr
        ? 0U : state_.host->activeSurfaceProvider()->generation();
    return queue_.enqueue(request, generation, state_.pointerSampleSequence);
  }
  void processPendingSurfaceModes() noexcept override {
    const auto generation = state_.host->activeSurfaceProvider() == nullptr
        ? 0U : state_.host->activeSurfaceProvider()->generation();
    const auto request = queue_.take(generation, state_.pointerSampleSequence);
    if (!request) {
      if (const auto latest = queue_.latestReceipt()) {
        state_.lastSurfaceReceipt = *latest;
        state_.hasSurfaceReceipt = true;
      }
      return;
    }
    const auto receipt = requestSurfaceMode(*request);
    state_.canonicalSurfaceMode = receipt.state == canvas::runtime::SurfaceControlState::kApplied
        ? request->mode : state_.canonicalSurfaceMode;
    state_.lastSurfaceReceipt = queue_.complete(request->requestId, receipt.state, receipt.generation);
    state_.hasSurfaceReceipt = true;
  }
  [[nodiscard]] canvas::runtime::SurfaceModeReceipt requestSurfaceMode(
      const canvas::runtime::SurfaceModeRequest& request) noexcept override {
    canvas::runtime::SurfaceModeReceipt receipt{};
    receipt.requestId = request.requestId;
    receipt.target = request.target;
    receipt.mode = request.mode;
    if (request.target != canvas::runtime::SurfaceRole::kCanonicalCanvas) {
      receipt.state = canvas::runtime::SurfaceControlState::kUnsupported;
      return receipt;
    }
    const auto currentGeneration = state_.host->activeSurfaceProvider() == nullptr
        ? 0U : state_.host->activeSurfaceProvider()->generation();
    receipt.generation = currentGeneration;
    if (currentGeneration == 0U || request.expectedGeneration != currentGeneration) {
      receipt.state = canvas::runtime::SurfaceControlState::kStaleGeneration;
      return receipt;
    }
    const char* profile = request.mode == canvas::runtime::SurfaceMode::kCpuReference
        ? "cpu-raster" : "windows-d3d12-canonical";
    const auto format = request.mode == canvas::runtime::SurfaceMode::kCpuReference
        ? canvas::render::RenderTargetFormat::kRgba8888
        : canvas::render::RenderTargetFormat::kBgra8888;
    if (!state_.host->selectCanonicalSurfaceProfile(profile, request.expectedGeneration, format)) {
      receipt.state = canvas::runtime::SurfaceControlState::kUnavailable;
      return receipt;
    }
    if (state_.host->activeSurfaceProvider() == nullptr) {
      receipt.state = canvas::runtime::SurfaceControlState::kUnavailable;
      return receipt;
    }
    receipt.state = canvas::runtime::SurfaceControlState::kApplied;
    receipt.generation = state_.host->activeSurfaceProvider()->generation();
    return receipt;
  }
 private:
  State& state_;
  canvas::runtime::BoundedSurfaceModeQueue queue_;
};

void hidePreviewPresentation(State& value) noexcept {
  value.previewPresentationEnabled = false;
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (value.host != nullptr) value.host->setPreviewOverlayVisible(false);
#endif
  if (value.debugUi != nullptr) value.debugUi->reposition();
}

// Stop accepting new preview frames for a completed stroke, but keep the
// already-presented overlay visible until CanonicalVisible retires it. Hiding
// at pointer-up exposes the old canonical surface for one message-loop turn
// while the committed frame is still being rendered, which produces a flash.
void stopPreviewPresentation(State& value) noexcept {
  value.previewPresentationEnabled = false;
  if (value.debugUi != nullptr) value.debugUi->reposition();
}

void retireVisiblePreviewPresentation(State& value) noexcept {
  // Call only after the canonical frame has actually reached the owner HWND.
  hidePreviewPresentation(value);
}

void enablePreviewPresentation(State& value) noexcept {
  value.previewPresentationEnabled = true;
  if (value.debugUi != nullptr) value.debugUi->reposition();
}

void wakeRenderPump(State& value) noexcept {
  requestPreviewRender(value);
  if (value.window == nullptr || value.renderWakePosted.exchange(true)) return;
  (void)PostMessageW(value.window, State::kRenderWakeMessage, 0, 0);
}

void startPreviewRenderPump(State& value);
void stopPreviewRenderPump(State& value) noexcept;

void createWindowsToolPalette(State& value, HINSTANCE instance) {
  constexpr int kButtonWidth = 132;
  constexpr int kButtonGap = 4;
  struct Tool { int id; const wchar_t* label; };
  constexpr Tool tools[] = {
      {kToolVector, L"Vector"}, {kToolMarker, L"Marker"},
      {kToolChalk, L"Chalk"}, {kToolMembrane, L"Membrane"},
      {kToolObjectEraser, L"Object Eraser"}, {kToolPartialEraser, L"Partial Eraser"},
      {kToolSelection, L"Selection"}};
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

void requestPreviewRender(State& value) noexcept {
  {
    std::lock_guard lock(value.previewPumpMutex);
    value.previewPumpPending = true;
  }
  value.previewPumpCv.notify_one();
}

void startPreviewRenderPump(State& value) {
  value.previewPump = std::thread([&value] {
    for (;;) {
      std::unique_lock lock(value.previewPumpMutex);
      value.previewPumpCv.wait(lock, [&] {
        return value.previewPumpStop || value.previewPumpPending;
      });
      if (value.previewPumpStop) return;
      value.previewPumpPending = false;
      lock.unlock();
      PreviewPresentationCapture capture{};
      std::uint64_t presentCount = 0;
      const bool gateOpen = value.host != nullptr && !value.canonicalPending &&
          value.previewPresentationEnabled;
      const bool captured = gateOpen && value.host->capturePreviewPresentation(capture);
      const bool rendered = captured && value.host->renderPreviewPresentation(capture, &presentCount);
      if (rendered) {
        {
          std::lock_guard completionLock(value.previewCompletionMutex);
          value.previewCompletion = capture;
          value.previewCompletionPresentCount = presentCount;
        }
        if (value.window != nullptr) {
          (void)PostMessageW(value.window, State::kPreviewPresentedMessage, 0, 0);
        }
      }
    }
  });
}

void stopPreviewRenderPump(State& value) noexcept {
  {
    std::lock_guard lock(value.previewPumpMutex);
    value.previewPumpStop = true;
    value.previewPumpPending = true;
  }
  value.previewPumpCv.notify_one();
  if (value.previewPump.joinable()) value.previewPump.join();
}

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
    // Runtime pointer identities may be reused after a stroke retires (the
    // mouse adapter intentionally uses one stable pointer id).  ARC keeps
    // retired stroke tombstones, so its lifecycle id must be fresh per
    // session rather than being the platform pointer id.
    const auto strokeId = beginArcStroke(identity.session);
    arc_preview_begin_v0 begin{};
    begin.struct_size = sizeof(begin); begin.abi_version = ARC_ABI_VERSION;
    begin.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; begin.stroke_id = strokeId;
    begin.view_id = 1U; begin.viewport_revision = identity.sessionGeneration;
    begin.target_generation = state_.previewGeneration;
    begin.brush.struct_size = sizeof(begin.brush); begin.brush.abi_version = ARC_ABI_VERSION;
    const auto status = state_.previewBridge->Begin(begin);
    if (state_.pointerDiagnostic) state_.pointerDiagnostic << "arc begin session="
        << identity.session << " stroke=" << strokeId << " status="
        << static_cast<std::uint32_t>(status) << '\n';
    if (status != arc::Status::kOk) {
      arcStrokeIds_.erase(identity.session);
      revisions_.erase(identity.session);
    }
    return status == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::PreviewSubmitResult update(
      const canvas::ink::PreviewIdentity& identity,
      const canvas::ink::BrushPreviewDelta&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::PreviewSubmitResult::kCanonicalOnly;
    const auto strokeId = arcStrokeId(identity.session);
    arc_preview_update_v0 update{};
    update.struct_size = sizeof(update); update.abi_version = ARC_ABI_VERSION;
    update.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; update.stroke_id = strokeId;
    update.preview_revision = ++revisions_[identity.session];
    update.view_id = 1U;
    update.viewport_revision = identity.sessionGeneration;
    update.target_generation = state_.previewGeneration;
    // The ARC protocol is lifecycle-only on Windows. Runtime's Skia preview
    // provider owns and renders BrushPreviewDelta.outline.
    const auto status = state_.previewBridge->Push(update);
    if (state_.pointerDiagnostic && status != arc::Status::kOk) {
      state_.pointerDiagnostic << "arc push session=" << identity.session
          << " stroke=" << strokeId << " revision=" << update.preview_revision
          << " status=" << static_cast<std::uint32_t>(status) << '\n';
    }
    return status == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::PreviewSubmitResult cancel(
      const canvas::ink::PreviewIdentity& identity) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::PreviewSubmitResult::kCanonicalOnly;
    const auto strokeId = arcStrokeId(identity.session);
    arc_preview_cancel_v0 cancel{sizeof(cancel), ARC_ABI_VERSION, strokeId,
                                 state_.previewGeneration, 1U, 0U};
    const auto status = state_.previewBridge->Cancel(cancel);
    arcStrokeIds_.erase(identity.session);
    revisions_.erase(identity.session);
    return status == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::HandoffResult canonicalCommitted(
      const canvas::ink::CanonicalHandoffIdentity& identity,
      const canvas::semantic::CanonicalCommitRecord&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::HandoffResult::kIgnored;
    const auto strokeId = arcStrokeId(identity.session);
    const auto revision = revisions_[identity.session];
    if (revision == 0U) return canvas::ink::HandoffResult::kRejected;
    arc_preview_seal_v0 seal{sizeof(seal), ARC_ABI_VERSION, strokeId,
                             revision, state_.previewGeneration};
    const auto sealStatus = state_.previewBridge->SealInput(seal);
    if (state_.pointerDiagnostic) state_.pointerDiagnostic << "arc seal session="
        << identity.session << " revision=" << revision << " status="
        << static_cast<std::uint32_t>(sealStatus) << '\n';
    if (sealStatus != arc::Status::kOk) {
      return canvas::ink::HandoffResult::kIgnored;
    }
    arc_canonical_commit_v0 commit{sizeof(commit), ARC_ABI_VERSION, strokeId,
                                   revision, identity.commit.ordinal.value(),
                                   state_.previewGeneration, {identity.commit.runtime_epoch.value(),
                                                              identity.commit.ordinal.value()}};
    const auto commitStatus = state_.previewBridge->CanonicalCommitted(commit);
    if (state_.pointerDiagnostic) state_.pointerDiagnostic << "arc committed session="
        << identity.session << " revision=" << revision << " document="
        << identity.commit.ordinal.value() << " status="
        << static_cast<std::uint32_t>(commitStatus) << '\n';
    return commitStatus == arc::Status::kOk ? canvas::ink::HandoffResult::kAccepted
                                            : canvas::ink::HandoffResult::kIgnored;
  }
  canvas::ink::HandoffResult canonicalVisible(
      const canvas::ink::CanonicalHandoffIdentity& identity,
      const canvas::render::FrameState&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::HandoffResult::kIgnored;
    const auto strokeId = arcStrokeId(identity.session);
    arc_canonical_visible_v0 visible{};
    visible.struct_size = sizeof(visible); visible.abi_version = ARC_ABI_VERSION;
    visible.stroke_id = strokeId; visible.document_revision = identity.commit.ordinal.value();
    visible.target_generation = identity.surfaceGeneration.value();
    visible.handoff_token = {identity.commit.runtime_epoch.value(), identity.commit.ordinal.value()};
    visible.receipt.struct_size = sizeof(visible.receipt);
    visible.receipt.abi_version = ARC_ABI_VERSION;
    visible.receipt.evidence = ARC_EVIDENCE_DETERMINISTIC_ORACLE;
    visible.receipt.status = ARC_STATUS_OK;
    visible.receipt.target_generation = identity.surfaceGeneration.value();
    visible.receipt.presentation_id = identity.commit.ordinal.value();
    const auto result = state_.previewBridge->CanonicalVisible(visible);
    if (state_.pointerDiagnostic) state_.pointerDiagnostic << "arc visible session="
        << identity.session << " document=" << identity.commit.ordinal.value()
        << " generation=" << identity.surfaceGeneration.value() << " status="
        << static_cast<std::uint32_t>(result) << '\n';
    if (result != arc::Status::kOk) {
      return canvas::ink::HandoffResult::kIgnored;
    }
    // This callback acknowledges one session's handoff.  The preview HWND is
    // shared by all active sessions, so visibility is retired by the render
    // pump only after Runtime has retired every matching session.
    arcStrokeIds_.erase(identity.session);
    revisions_.erase(identity.session);
    return canvas::ink::HandoffResult::kAccepted;
  }
 private:
  std::uint64_t beginArcStroke(std::uint64_t session) {
    const auto id = nextStrokeId_++;
    arcStrokeIds_[session] = id;
    revisions_[session] = 0U;
    return id;
  }
  std::uint64_t arcStrokeId(std::uint64_t session) const {
    auto it = arcStrokeIds_.find(session);
    return it == arcStrokeIds_.end() ? 0U : it->second;
  }
  State& state_;
  std::uint64_t nextStrokeId_ = 1U;
  std::unordered_map<std::uint64_t, std::uint64_t> arcStrokeIds_;
  std::unordered_map<std::uint64_t, std::uint64_t> revisions_;
};

bool renderCanonical(State& value) {
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (value.canonicalRenderer == nullptr || value.host == nullptr) return false;
  // While a mouse/pointer is down, the canonical redraw is only a backing
  // surface update. The matching CanonicalVisible handoff must happen after
  // release, otherwise WM_PAINT clears the live Arc preview before the user
  // can see it.
  const auto accepted = value.host->presentCanonicalFrame(
      value.host->canonicalFrameCount() + 1U, 0.0,
      value.activeKeys.empty() || value.host->pendingCanonicalHandoffCount() != 0U);
  if (value.pointerDiagnostic) {
    value.pointerDiagnostic << "canonical-present accepted=" << accepted
        << " provider-generation="
        << (value.canonicalProvider == nullptr ? 0ULL : value.canonicalProvider->generation())
        << " frames=" << value.host->canonicalFrameCount()
        << " objects=" << value.host->submittedOperationCount() << '\n';
  }
  // A final pointer-up gates the preview worker until every matching
  // CanonicalVisible receipt has retired.  Only clear that gate after the
  // host reports no pending handoffs; otherwise an in-flight worker
  // completion can make the just-retired overlay visible again.
  if (accepted && value.host->pendingCanonicalHandoffCount() == 0U) {
    value.canonicalPending = false;
  }
  if (accepted && !value.host->previewActive()) {
    retireVisiblePreviewPresentation(value);
  }
  return accepted;
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
    if (value.activeKeys.contains(kMousePointerId) ||
        value.selectionPointers.contains(kMousePointerId)) return false;
    begin = true;
    phase = ARC_POINTER_PHASE_DOWN;
  } else if (message == WM_LBUTTONUP) {
    if (value.host->selectionMode()) {
      if (!value.selectionPointers.contains(kMousePointerId)) return false;
    } else if (!value.activeKeys.contains(kMousePointerId)) return false;
    end = true;
    phase = ARC_POINTER_PHASE_UP;
  } else if ((wParam & MK_LBUTTON) == 0U || !value.activeKeys.contains(kMousePointerId)) {
    if (!value.host->selectionMode() || !value.selectionPointers.contains(kMousePointerId)) {
      return false;
    }
  }
  if (begin) {
    if (!beginCanvasInput(value, kMousePointerId)) return false;
    ++value.stroke;
    enablePreviewPresentation(value);
    SetCapture(window);
  } else if (!canvasOwnsInput(value, kMousePointerId)) {
    return false;
  }

  const auto x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
  const auto y = static_cast<float>(static_cast<short>(HIWORD(lParam)) - State::kToolbarHeight);
  if (begin && y < 0.0F) return false;
  const bool selectionMode = value.host->selectionMode();
  if (selectionMode && begin) {
    value.selectionPointers.insert(kMousePointerId);
    (void)value.host->selectAtViewPoint(x, y);
    InvalidateRect(window, nullptr, FALSE);
    return true;
  }
  if (selectionMode) {
    if (end) {
      value.selectionPointers.erase(kMousePointerId);
      endCanvasInput(value, kMousePointerId);
      if (GetCapture() == window) ReleaseCapture();
    }
    return true;
  }
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
        << " timestamp=" << value.lastMouseTimestampNs << '\n';
  }
  if (!accepted) {
    if (begin) endCanvasInput(value, kMousePointerId);
    return false;
  }
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
  if (end) value.evidenceDirty = true;
  if (end) {
    value.activeKeys.erase(kMousePointerId);
    value.pointerStrokes.erase(kMousePointerId);
    endCanvasInput(value, kMousePointerId);
    if (GetCapture() == window) ReleaseCapture();
    // Canonical presentation is scheduled by the render pump. Waiting for a
    // D3D12 fence here would delay the next pointer message.
    if (value.activeKeys.empty()) {
      value.canonicalFrameReady = false;
      value.canonicalPending = true;
      // Stop submitting newer preview frames at the input boundary. Keep the
      // last presented overlay until the canonical frame retires the session,
      // so the compositor never exposes an intermediate blank/old frame.
      stopPreviewPresentation(value);
    }
  }
  value.previewDirty = true;
  // Skia/D3D12 preview work is intentionally deferred to the render pump.
  wakeRenderPump(value);
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
    if (begin && !beginCanvasInput(value, pointerId)) continue;
    if (!begin && !canvasOwnsInput(value, pointerId)) continue;
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
          << " accepted=" << accepted << '\n';
    }
    if (!accepted) {
      if (begin) endCanvasInput(value, pointerId);
      continue;
    }
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
      endCanvasInput(value, pointerId);
      endCanvasInput(value, pointerId);
      if (value.activeKeys.empty() && GetCapture() == window) ReleaseCapture();
    }
    value.previewDirty = true;
    if (end && value.activeKeys.empty()) {
      value.canonicalFrameReady = false;
      value.canonicalPending = true;
      stopPreviewPresentation(value);
    }
  }
  if (acceptedAny) wakeRenderPump(value);
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
  endCanvasInput(value, pointerId);
  value.evidenceDirty = true;
  wakeRenderPump(value);
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
  clearCanvasInput(value);
  if (GetCapture() == value.window) ReleaseCapture();
  value.evidenceDirty = true;
  wakeRenderPump(value);
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

canvas::debug_ui::DebugSnapshot buildDebugSnapshot(State& value) {
  canvas::debug_ui::DebugSnapshot snapshot{};
  snapshot.stamp.sequence = value.pointerSampleSequence;
  snapshot.stamp.snapshotSequence = value.pointerSampleSequence;
  snapshot.stamp.monotonicTimeNs = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
  snapshot.stamp.frameId = value.presentCount;
#if defined(CANVAS_RENDER_HAS_SKIA)
  snapshot.canonicalSurfaceGeneration = value.host->activeSurfaceProvider() != nullptr
      ? value.host->activeSurfaceProvider()->generation() : 0;
  snapshot.previewSurfaceGeneration = value.previewProvider != nullptr ? value.previewProvider->generation() : 0;
#else
  snapshot.canonicalSurfaceGeneration = 0;
  snapshot.previewSurfaceGeneration = 0;
#endif
  snapshot.canonicalRevision = value.host->submittedOperationCount();
  snapshot.previewRevision = value.host->previewPresentCount();
  snapshot.activePointerCount = static_cast<std::uint32_t>(value.activeKeys.size());
  snapshot.selectedTool = static_cast<std::uint32_t>(value.selectedTool);
  snapshot.stamp.runtimeGeneration = 1U;
  snapshot.stamp.documentGeneration = value.host->semanticGeneration().value();
  snapshot.stamp.viewGeneration = 1U;
  snapshot.stamp.surfaceGeneration = snapshot.canonicalSurfaceGeneration;
  snapshot.stamp.generation = snapshot.stamp.surfaceGeneration;
  if (value.runtimeFacade != nullptr) {
    const auto runtimeState = value.runtimeFacade->readRuntimeState();
    snapshot.stamp.runtimeGeneration = runtimeState.runtimeGeneration;
    snapshot.stamp.documentGeneration = runtimeState.documentGeneration;
    snapshot.stamp.viewGeneration = runtimeState.viewGeneration;
    snapshot.selectedTool = runtimeState.toolId != 0U
        ? runtimeState.toolId : snapshot.selectedTool;
    snapshot.canUndo = runtimeState.canUndo;
    snapshot.canRedo = runtimeState.canRedo;
    snapshot.selectionMode = runtimeState.selectionMode;
    snapshot.selectedObjectCount = runtimeState.selectedObjectCount;
    snapshot.selectedPrimaryObject = runtimeState.selectedPrimaryObject;
    if (value.hasProductReceipt) {
      snapshot.productControlRequestId = value.lastProductReceipt.requestId;
      snapshot.productControlState = value.lastProductReceipt.state;
    }
  }
  snapshot.canonicalSurfaceMode = value.canonicalSurfaceMode;
  if (value.hasSurfaceReceipt) {
    snapshot.surfaceControlRequestId = value.lastSurfaceReceipt.requestId;
    snapshot.surfaceControlState = value.lastSurfaceReceipt.state;
    snapshot.surfaceControlGeneration = value.lastSurfaceReceipt.generation;
  }
  snapshot.arcPresenterActive = value.host->previewActive();
  snapshot.traceEnabled = false;
  snapshot.capabilities.fill(canvas::debug_ui::CapabilityState::kUnavailable);
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kInput)] =
      canvas::debug_ui::CapabilityState::kAvailable;
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kCanonicalSurface)] =
      value.host->surface().available ? canvas::debug_ui::CapabilityState::kAvailable
                                      : canvas::debug_ui::CapabilityState::kDegraded;
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kArcPreviewSurface)] =
      value.previewProvider != nullptr ? canvas::debug_ui::CapabilityState::kAvailable
                                       : canvas::debug_ui::CapabilityState::kUnavailable;
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kSurfaceMode)] =
      value.platformDebugControl != nullptr ? canvas::debug_ui::CapabilityState::kAvailable
                                             : canvas::debug_ui::CapabilityState::kUnavailable;
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kTelemetry)] =
      value.telemetry != nullptr ? canvas::debug_ui::CapabilityState::kAvailable
                                 : canvas::debug_ui::CapabilityState::kUnavailable;
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kInspection)] = canvas::debug_ui::CapabilityState::kUnavailable;
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kTrace)] = canvas::debug_ui::CapabilityState::kUnavailable;
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kGpuTiming)] = canvas::debug_ui::CapabilityState::kUnavailable;
  snapshot.inputBatchCount = value.host->hud().batch;
  snapshot.handoffCount = value.host->hud().pendingHandoffCount;
  snapshot.presentCount = value.presentCount;
  snapshot.surfaceLostCount = value.host->surfaceLostCount();
  snapshot.sampleHz = value.host->hud().sampleHz;
  snapshot.frameMs = value.host->hud().frameMs;
  snapshot.queueAgeMs = value.host->hud().queueAgeMs;
  snapshot.surfaceAvailable = value.host->surface().available;
  if (value.arcDiagnostics != nullptr) {
    const auto arc = value.arcDiagnostics->readArcDiagnostics();
    snapshot.previewRevision = arc.previewRevision;
    snapshot.activePointerCount = static_cast<std::uint32_t>(arc.activePointerCount);
    snapshot.inputBatchCount = arc.inputBatchCount;
    snapshot.handoffCount = arc.handoffCount;
    snapshot.arcPresenterActive = arc.previewActive;
  }
  if (value.platformDiagnostics != nullptr) {
    const auto platform = value.platformDiagnostics->readPlatformDiagnostics();
    snapshot.canonicalSurfaceGeneration = platform.canonicalSurfaceGeneration;
    snapshot.previewSurfaceGeneration = platform.previewSurfaceGeneration;
    snapshot.stamp.surfaceGeneration = platform.canonicalSurfaceGeneration;
    snapshot.stamp.generation = platform.canonicalSurfaceGeneration;
    snapshot.surfaceAvailable = platform.surfaceAvailable;
    snapshot.presentCount = platform.presentCount;
    snapshot.surfaceLostCount = platform.lostCount;
  }
  return snapshot;
}

void paint(HWND window, State& value) {
  if (value.debugUi != nullptr) {
    const auto snapshot = buildDebugSnapshot(value);
    value.debugSnapshots.publish(snapshot);
    // Do not rebuild the full ImGui/Skia overlay from a canvas paint while a
    // pointer is active. WM_PAINT shares the input thread and can otherwise
    // delay coalesced pointer delivery. The pointer-up path and the next idle
    // timer refresh the panel after the stroke is complete.
    if (value.activeKeys.empty() && !value.resizeInProgress) {
      value.debugUi->frame(snapshot);
    }
  }
  PAINTSTRUCT ps{};
  HDC dc = BeginPaint(window, &ps);
  RECT rect{};
  GetClientRect(window, &rect);
  FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  if (!value.resizeInProgress && value.activeKeys.empty() &&
      (value.host->canonicalPresentationDirty() || !value.canonicalFrameReady)) {
    // A resize can invalidate the D3D12 surface before the deferred resize
    // message has rebuilt it.  Only mark the canonical frame ready after the
    // provider has actually accepted and presented the frame; otherwise one
    // transient resize failure permanently suppresses all later redraws.
    value.canonicalFrameReady = renderCanonical(value);
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
  if (value.debugUi != nullptr) value.debugUi->paint(dc);
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
  if (value != nullptr && value->debugUi != nullptr &&
      message != WM_KEYDOWN && message != WM_KEYUP &&
      value->debugUi->handleMessage(window, message, wParam, lParam)) {
    return 0;
  }
  if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_MOUSEMOVE) {
    if (value != nullptr) {
      if (value->pointerDiagnostic && message != WM_MOUSEMOVE) {
        value->pointerDiagnostic << "mouse phase=" << message << " x="
            << static_cast<short>(LOWORD(lParam)) << " y="
            << static_cast<short>(HIWORD(lParam)) << '\n';
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
  if (message == State::kRenderWakeMessage && value != nullptr) {
    value->renderWakePosted = false;
  }
  if (message == State::kPreviewPresentedMessage && value != nullptr) {
    std::optional<PreviewPresentationCapture> capture;
    std::uint64_t presentCount = 0;
    {
      std::lock_guard completionLock(value->previewCompletionMutex);
      capture = value->previewCompletion;
      presentCount = value->previewCompletionPresentCount;
      value->previewCompletion.reset();
    }
    if (!value->canonicalPending && capture.has_value() && value->host != nullptr &&
        value->host->acknowledgePreviewPresentation(*capture, presentCount)) {
      value->previewDirty = false;
    } else if (value->previewDirty) {
      requestPreviewRender(*value);
    }
    if (value->debugUi != nullptr) value->debugUi->raise();
    RECT canvasRect{}; GetClientRect(window, &canvasRect);
    canvasRect.top = State::kToolbarHeight;
    InvalidateRect(window, &canvasRect, FALSE);
    return 0;
  }
  if ((message == WM_TIMER && value != nullptr && wParam == State::kRenderTimerId) ||
      (message == State::kRenderWakeMessage && value != nullptr)) {
    if (value->axiomDebugControl != nullptr) value->axiomDebugControl->process();
    if (value->platformDebugControl != nullptr) {
      value->platformDebugControl->processPendingSurfaceModes();
    }
    // Debug state must advance independently of canvas WM_PAINT. This keeps
    // receipts and product-control mutations visible while the app is idle.
    // The canvas and the Debug UI share this Win32 message thread. A full
    // ImGui→Skia overlay rebuild can allocate and rasterize for several
    // milliseconds, which would delay WM_POINTER/WM_MOUSEMOVE dispatch and
    // make coalesced input appear to have a lower sample rate. During an
    // active stroke, let input and preview presentation own the thread; the
    // next idle tick and the pointer-up invalidation refresh diagnostics.
    if (value->debugUi != nullptr && value->debugUi->visible() &&
        value->activeKeys.empty() && !value->resizeInProgress) {
      value->debugUi->refresh();
    }
    const bool f12Down = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
    if (f12Down && !value->debugF12Down && value->debugUi != nullptr) {
      value->debugUi->toggle();
      if (value->pointerDiagnostic) {
        value->pointerDiagnostic << "debug-ui-poll visible="
            << value->debugUi->visible() << '\n';
      }
      InvalidateRect(window, nullptr, FALSE);
      UpdateWindow(window);
    }
    value->debugF12Down = f12Down;
    if (value->evidenceDirty && value->activeKeys.empty()) {
      persistEvidence(*value);
      value->evidenceDirty = false;
    }
    if (!value->resizeInProgress &&
        (value->activeKeys.empty() ||
         value->host->pendingCanonicalHandoffCount() != 0U ||
         value->host->canonicalPresentationDirty()) &&
        (value->host->canonicalPresentationDirty() || !value->canonicalFrameReady)) {
      value->canonicalFrameReady = renderCanonical(*value);
    }
    if (value->previewDirty && value->previewPresentationEnabled) requestPreviewRender(*value);
    return 0;
  }
  if (message == WM_SIZE && value != nullptr) {
#if defined(CANVAS_RENDER_HAS_SKIA)
    const auto width = static_cast<std::uint32_t>(LOWORD(lParam));
    const auto canvasHeight = static_cast<int>(HIWORD(lParam)) - State::kToolbarHeight;
    const auto height = static_cast<std::uint32_t>((std::max)(canvasHeight, 0));
    if (value->pointerDiagnostic) {
      value->pointerDiagnostic << "wm-size width=" << width << " height=" << height
          << " wparam=" << wParam << '\n';
    }
    if (width != 0U && height != 0U && value->previewProvider != nullptr) {
      value->pendingWidth = width;
      value->pendingHeight = height;
      value->resizeInProgress = true;
      // The old canonical surface may no longer match the client metrics.
      // Keep retrying until a frame from the new generation is actually
      // presented instead of treating the old ready bit as authoritative.
      value->canonicalFrameReady = false;
#if defined(CANVAS_RENDER_HAS_SKIA)
      // Do not detach the owner-attached canonical visual here.  During a
      // maximize Windows can deliver several WM_SIZE messages in one turn;
      // SetContent(nullptr)+Commit on every intermediate size races the
      // DComp/D3D12 queue and has caused driver crashes and black frames.
      // The deferred resize path replaces the surface once, then presents a
      // fresh canonical frame before the new generation is considered ready.
  if (value->host != nullptr) value->host->setPreviewOverlayVisible(false);
#endif
      // Coalesce the whole maximize animation.  Every WM_SIZE resets this
      // short debounce; only the final client size rebuilds the D3D12
      // surfaces, so an intermediate black backbuffer is never presented.
      SetTimer(window, State::kResizeDebounceTimerId, 120, nullptr);
    }
#else
    (void)lParam;
#endif
    return 0;
  }
  if (message == WM_TIMER && value != nullptr &&
      wParam == State::kResizeDebounceTimerId) {
    if (value->pointerDiagnostic) value->pointerDiagnostic << "resize-debounce-fired" << '\n';
    KillTimer(window, State::kResizeDebounceTimerId);
    if (!value->resizePosted) {
      value->resizePosted = true;
      PostMessageW(window, State::kDeferredResizeMessage, 0, 0);
    }
    return 0;
  }
  if (message == State::kDeferredResizeMessage && value != nullptr) {
    value->resizePosted = false;
#if defined(CANVAS_RENDER_HAS_SKIA)
    RECT client{};
    if (!GetClientRect(window, &client)) return 0;
    const auto width = static_cast<std::uint32_t>(client.right);
    const auto height = static_cast<std::uint32_t>(
        (std::max)(static_cast<int>(client.bottom) - State::kToolbarHeight, 0));
    if (value->pointerDiagnostic) {
      value->pointerDiagnostic << "deferred-resize width=" << width << " height=" << height
          << " begin" << '\n';
    }
    bool resized = false;
    const bool hostResized = width != 0U && height != 0U &&
        value->previewProvider != nullptr && value->host->resizeSurface(width, height);
    const bool targetAttached = hostResized && attachPreviewTarget(*value, width, height);
    if (hostResized && targetAttached) {
      // The canonical D3D12 provider is rebuilt together with the preview
      // provider.  Present one fresh frame immediately after the new
      // generation is bound; otherwise the new owner-attached backbuffer is
      // visible as its driver-default black contents until the next stroke.
      value->canonicalFrameReady = renderCanonical(*value);
      // resizeSurface rebinds the preview controller to the new generation;
      // render it before allowing the provider to show its popup again.
      if (value->host->previewActive()) value->previewDirty = true;
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
      resized = true;
    }
    if (value->pointerDiagnostic) {
      value->pointerDiagnostic << "deferred-resize result host=" << hostResized
          << " target=" << targetAttached << " canonical="
          << value->canonicalFrameReady << '\n';
    }
    value->resizeInProgress = false;
    if (!resized) {
      // Keep the retry bounded to the message loop.  A transient DXGI/DComp
      // rejection during a maximize must not leave canonicalFrameReady false
      // forever (which used to make the canonical document appear to vanish).
      value->canonicalFrameReady = false;
      InvalidateRect(window, nullptr, FALSE);
    }
#endif
    return 0;
  }
  if ((message == WM_MOVE || message == WM_WINDOWPOSCHANGED) && value != nullptr) {
    // USER32 sends these notifications before the owner has finished moving.
    // Defer popup placement until the new client-to-screen origin is stable.
    const auto result = DefWindowProcW(window, message, wParam, lParam);
    PostMessageW(window, State::kOverlaySyncMessage, 0, 0);
    return result;
  }
  if (message == State::kOverlaySyncMessage && value != nullptr) {
#if defined(CANVAS_RENDER_HAS_SKIA)
    if (value->previewProvider != nullptr) value->previewProvider->reposition();
#endif
    if (value->debugUi != nullptr) value->debugUi->reposition();
    return 0;
  }
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP) {
    if (value == nullptr) return 0;
    const UINT pointerId = GET_POINTERID_WPARAM(wParam);
    POINTER_INFO info{};
    if (!GetPointerInfo(pointerId, &info)) {
      if (value->pointerDiagnostic) {
        value->pointerDiagnostic << "pointer-info-failed phase=" << message
            << " id=" << pointerId << " error=" << GetLastError() << '\n';
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
          << " active=" << value->activeKeys.size() << '\n';
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
    if (value->host->selectionMode() &&
        (message == WM_POINTERDOWN || message == WM_POINTERUP)) {
      POINT clientPoint = info.ptPixelLocation;
      if (ScreenToClient(window, &clientPoint) == FALSE) return 0;
      const float x = static_cast<float>(clientPoint.x);
      const float y = static_cast<float>(clientPoint.y - State::kToolbarHeight);
      if (message == WM_POINTERDOWN) {
        if (!beginCanvasInput(*value, pointerId)) return 0;
        value->selectionPointers.insert(pointerId);
        if (y >= 0.0F) (void)value->host->selectAtViewPoint(x, y);
        InvalidateRect(window, nullptr, FALSE);
      } else if (value->selectionPointers.erase(pointerId) != 0U) {
        endCanvasInput(*value, pointerId);
        InvalidateRect(window, nullptr, FALSE);
      }
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
    if (!begin && !canvasOwnsInput(*value, pointerId)) return 0;
    if (begin) {
      if (!beginCanvasInput(*value, pointerId)) return 0;
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
          << " accepted=" << accepted << '\n';
    }
    if (!accepted) {
      if (begin) endCanvasInput(*value, pointerId);
      return 0;
    }
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
        value->pointerDiagnostic << "" << '\n';
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
          << " canonical_strokes=" << value->host->submittedOperationCount() << '\n';
      for (const auto& [contactId, contactKey] : value->activeKeys) {
        value->pointerDiagnostic << "active-contact id=" << contactId
            << " source=" << contactKey.source << " generation=" << contactKey.generation
            << " disposition=" << static_cast<int>(value->host->pointerDisposition(contactKey))
            << '\n';
      }
    }
    // Runtime owns viewport arbitration and typed ARC preview publication.
    if (end) value->evidenceDirty = true;
    if (end) {
      value->activeKeys.erase(pointerId);
      value->pointerStrokes.erase(pointerId);
      endCanvasInput(*value, pointerId);
      if (value->activeKeys.empty()) {
        if (GetCapture() == window) ReleaseCapture();
        value->canonicalFrameReady = false;
        value->canonicalPending = true;
        stopPreviewPresentation(*value);
      }
    }
    value->previewDirty = true;
    // The render timer presents the retained preview after this message.
    wakeRenderPump(*value);
    if (end) {
      RECT canvasRect{}; GetClientRect(window, &canvasRect);
      canvasRect.top = State::kToolbarHeight;
      InvalidateRect(window, &canvasRect, FALSE);
    }
    return 0;
  }
  if (message == WM_TOUCH) {
    if (value != nullptr && value->pointerDiagnostic) {
      value->pointerDiagnostic << "wm-touch count=" << LOWORD(wParam) << '\n';
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
            << value->activeKeys.size() << '\n';
      }
      // Capture loss is terminal for a Debug-owned sequence, but a native
      // Canvas pointer remains governed by its existing Runtime cancel path.
      // Clear only the shared ownership records here; the normal platform
      // cancel handler below still retires active Canvas sessions.
      if (value->activeKeys.empty()) clearCanvasInput(*value);
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
    if (value != nullptr) clearCanvasInput(*value);
    if (message == WM_DESTROY) {
      PostQuitMessage(0);
      return 0;
    }
    return 0;
  }
  if (message == WM_PAINT) { if (value != nullptr) paint(window, *value); return 0; }
  if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
      (wParam == L'Z' || wParam == L'Y')) {
    if (value != nullptr && value->runtimeFacade != nullptr &&
        (lParam & (1LL << 30)) == 0) {
      const auto generation = value->runtimeFacade->readRuntimeState().runtimeGeneration;
      static std::uint64_t nextHistoryRequestId = 0x100000U;
      const auto receipt = wParam == L'Z'
          ? value->runtimeFacade->undo(nextHistoryRequestId++, generation)
          : value->runtimeFacade->redo(nextHistoryRequestId++, generation);
      if (receipt.state == canvas::runtime::ProductControlState::kApplied) {
        value->canonicalFrameReady = false;
        value->previewPresentationEnabled = true;
        InvalidateRect(window, nullptr, FALSE);
      }
    }
    return 0;
  }
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
          << " diag=" << value.pointerDiagnosticPath.string() << '\n';
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
  value.debugUi = std::make_unique<canvas::debug_ui::WindowsDebugUiHost>();
  if (!value.debugUi->initialize(value.window)) return 1;
  value.debugUi->setInputCaptureGate(&value.inputCapture);
  value.runtimeFacade = std::make_unique<WindowsRuntimeFacade>(value);
  value.axiomDebugControl = std::make_unique<WindowsAxiomDebugControl>(value);
  value.platformDebugControl = std::make_unique<WindowsPlatformDebugControl>(value);
  value.arcDiagnostics = std::make_unique<WindowsArcDiagnostics>(value);
  value.platformDiagnostics = std::make_unique<WindowsPlatformDiagnostics>(value);
  value.telemetry = std::make_unique<WindowsTelemetry>(value);
  value.debugUi->setRuntimeFacade(value.runtimeFacade.get());
  value.debugUi->setDiagnostics(value.runtimeFacade.get());
  value.debugUi->setAxiomDebugControl(value.axiomDebugControl.get());
  value.debugUi->setPlatformDebugControl(value.platformDebugControl.get());
  value.debugUi->setArcDiagnostics(value.arcDiagnostics.get());
  value.debugUi->setPlatformDiagnostics(value.platformDiagnostics.get());
  value.debugUi->setTelemetry(value.telemetry.get());
  value.debugUi->setSnapshotRefresh([&value]() {
    const auto snapshot = buildDebugSnapshot(value);
    value.debugSnapshots.publish(snapshot);
    return snapshot;
  });
  const BOOL touchRegistered = RegisterTouchWindow(value.window, TWF_WANTPALM);
  const BOOL pointerTouchRegistered = RegisterPointerInputTarget(value.window, PT_TOUCH);
  const BOOL pointerPenRegistered = RegisterPointerInputTarget(value.window, PT_PEN);
  if (value.pointerDiagnostic) {
    value.pointerDiagnostic << "input-registration touch=" << touchRegistered
        << " pointer-touch=" << pointerTouchRegistered
        << " pointer-pen=" << pointerPenRegistered
        << " digitizer=" << GetSystemMetrics(SM_DIGITIZER)
        << " max-touches=" << GetSystemMetrics(SM_MAXIMUMTOUCHES)
        << " error=" << GetLastError() << '\n';
  }
  if (value.pointerDiagnostic) {
    value.pointerDiagnostic << "window-created hwnd="
        << reinterpret_cast<std::uintptr_t>(value.window) << '\n';
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
  value.host->setPlatformPresentationDeferred(true);
#endif
  startPreviewRenderPump(value);
  // Some launchers (notably cmd/start wrappers) pass nCmdShow=0 even when
  // the user expects a normal interactive window.  Passing that value
  // through would leave the process alive with a hidden main HWND, which
  // looks like a failed launch.  Preserve an explicit minimized request but
  // normalize the unspecified/hidden value to a visible interactive window.
  const int effectiveShow = (show == 0 || show == SW_HIDE) ? SW_SHOWNORMAL : show;
  ShowWindow(value.window, effectiveShow);
  UpdateWindow(value.window);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  stopPreviewRenderPump(value);
  return static_cast<int>(message.wParam);
}
#else
int main() { return 2; }
#endif
