#include "ink_playground_host.hpp"
#include "platform_brush_baseline_observation.hpp"
#include "canvas/render/webgl_surface_backend.hpp"
#include "canvas/render/skia_renderer.hpp"
#if AXIOM_WEB_DEBUG_UI
#include "canvas/debug_ui/controller.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/debug_ui/input_capture.hpp"
#include "imgui.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#endif

#include <emscripten/emscripten.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

EM_JS(void, axiom_web_preview_visibility, (int visible), {
  const canvas = document.getElementById("arcPreview");
  if (canvas) canvas.style.visibility = visible ? "visible" : "hidden";
});

namespace {
using Host = canvas::ink_playground::InkPlaygroundHost;
using Handle = std::uint32_t;

std::unordered_map<Handle, std::unique_ptr<Host>>& hosts() {
  static std::unordered_map<Handle, std::unique_ptr<Host>> values;
  return values;
}

Host* host(Handle value) {
  const auto it = hosts().find(value);
  return it == hosts().end() ? nullptr : it->second.get();
}

struct BrushState final {
  std::uint64_t serial = 0;
  std::uint64_t digest = 0;
  std::uint64_t primitiveCount = 0;
  std::uint32_t family = 1;
  int representation = 1;
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::SkiaRenderer> renderer;
#endif
};
std::unordered_map<Handle, BrushState>& brushes() {
  static std::unordered_map<Handle, BrushState> values;
  return values;
}

#if AXIOM_WEB_DEBUG_UI
class WebRuntimeFacade final : public canvas::runtime::RuntimeFacade {
 public:
  explicit WebRuntimeFacade(Host& host) : host_(host) {}
  [[nodiscard]] canvas::runtime::RuntimeDiagnosticsSnapshot readDiagnostics() const noexcept override {
    const auto view = host_.viewportGesture();
    return {1U, host_.semanticGeneration().value(),
            static_cast<std::uint64_t>(host_.submittedOperationCount()), 1U,
            static_cast<std::uint32_t>(host_.toolMode()), host_.surface().generation,
            view.scale, view.translationX, view.translationY,
            host_.canUndo(), host_.canRedo(),
            0U, 0U, static_cast<std::uint64_t>(host_.submittedOperationCount()), 0U};
  }
  [[nodiscard]] canvas::runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override {
    const auto diagnostics = readDiagnostics();
    const auto& profile = host_.selectedBrushProfile();
    return {diagnostics.runtimeGeneration, diagnostics.documentGeneration,
            diagnostics.documentRevision, diagnostics.viewGeneration,
            diagnostics.surfaceGeneration, diagnostics.toolId,
            profile == "vector-solid-v1" ? 1U : profile == "marker-flat-v1" ? 2U
            : profile == "chalk-grain-v1" ? 3U : 4U,
            host_.selectedBrushRevision(),
            host_.toolMode() == Host::ToolMode::kObjectEraser ? 1U
            : host_.toolMode() == Host::ToolMode::kPartialEraser ? 2U : 0U,
            diagnostics.cameraScale, diagnostics.cameraTranslationX,
            diagnostics.cameraTranslationY, diagnostics.canUndo, diagnostics.canRedo};
  }
  [[nodiscard]] canvas::runtime::ProductControlReceipt submitProductControl(
      const canvas::runtime::ProductControlRequest& request) noexcept override {
    canvas::runtime::ProductControlReceipt receipt{request.requestId,
        canvas::runtime::ProductControlState::kRejected, 1U};
    if (request.runtimeGeneration != 0U && request.runtimeGeneration != 1U) return receipt;
    if (request.action == canvas::runtime::ProductControlAction::kSetBrush) {
      static constexpr const char* profiles[] = {
          "vector-solid-v1", "marker-flat-v1", "chalk-grain-v1", "membrane-v1"};
      if (request.brushId < 1U || request.brushId > 4U) return receipt;
      const auto revision = request.brushRevision == 0U
          ? (request.brushId == 3U ? 4U : 1U) : request.brushRevision;
      if (host_.selectTool(Host::ToolMode::kBrush) &&
          host_.selectBrushProfile(profiles[request.brushId - 1U], revision)) {
        receipt.state = canvas::runtime::ProductControlState::kApplied;
      }
      return receipt;
    }
    if (request.action == canvas::runtime::ProductControlAction::kSetEraser) {
      const auto mode = request.eraserId == 1U ? Host::ToolMode::kObjectEraser
          : request.eraserId == 2U ? Host::ToolMode::kPartialEraser : Host::ToolMode::kBrush;
      if (request.eraserId != 0U && host_.selectTool(mode)) {
        receipt.state = canvas::runtime::ProductControlState::kApplied;
      }
      return receipt;
    }
    if (request.action == canvas::runtime::ProductControlAction::kUndo ||
        request.action == canvas::runtime::ProductControlAction::kRedo) {
      const bool applied = request.action == canvas::runtime::ProductControlAction::kUndo
          ? host_.undo() : host_.redo();
      receipt.state = applied ? canvas::runtime::ProductControlState::kApplied
                              : canvas::runtime::ProductControlState::kRejected;
      return receipt;
    }
    if (request.action == canvas::runtime::ProductControlAction::kSetCamera) {
      canvas::interaction::ViewportNavigationSample navigation{};
      navigation.anchorX = request.anchorX; navigation.anchorY = request.anchorY;
      navigation.deltaX = request.deltaX; navigation.deltaY = request.deltaY;
      navigation.scaleDelta = request.scaleDelta;
      navigation.kind = request.cameraAction == 2U
          ? canvas::interaction::ViewportNavigationKind::kBrowserGesture
          : canvas::interaction::ViewportNavigationKind::kWheelPan;
      receipt.state = host_.applyViewportNavigation(navigation)
          ? canvas::runtime::ProductControlState::kApplied
          : canvas::runtime::ProductControlState::kRejected;
      return receipt;
    }
    receipt.state = canvas::runtime::ProductControlState::kUnsupported;
    return receipt;
  }
 private:
  Host& host_;
};

struct DebugUiState final {
  std::unique_ptr<canvas::render::WebGlSurfaceProvider> provider;
  std::unique_ptr<WebRuntimeFacade> runtime;
  sk_sp<SkImage> fontTexture;
  ImGuiContext* context = nullptr;
  bool visible = false;
  bool initialized = false;
  int width = 0;
  int height = 0;
  std::uint32_t selectedTool = 4101;
  bool ctrlDown = false;
  std::uint64_t nextRequestId = 1;
  canvas::debug_ui::InputCaptureGate capture;
};
std::unordered_map<Handle, DebugUiState>& debugUi() {
  static std::unordered_map<Handle, DebugUiState> values;
  return values;
}

canvas::debug_ui::DebugSnapshot debugSnapshot(Host& target, DebugUiState& state) {
  canvas::debug_ui::DebugSnapshot snapshot{};
  const auto runtime = state.runtime->readRuntimeState();
  snapshot.stamp.sequence = target.hud().batch;
  snapshot.stamp.snapshotSequence = snapshot.stamp.sequence;
  snapshot.stamp.frameId = target.canonicalFrameCount();
  snapshot.stamp.runtimeGeneration = runtime.runtimeGeneration;
  snapshot.stamp.documentGeneration = runtime.documentGeneration;
  snapshot.stamp.viewGeneration = runtime.viewGeneration;
  snapshot.stamp.surfaceGeneration = runtime.surfaceGeneration;
  snapshot.stamp.generation = runtime.surfaceGeneration;
  snapshot.canonicalSurfaceGeneration = runtime.surfaceGeneration;
  snapshot.previewSurfaceGeneration = target.previewSurfaceGeneration();
  snapshot.canonicalRevision = target.submittedOperationCount();
  snapshot.previewRevision = target.previewPresentCount();
  snapshot.inputBatchCount = target.hud().batch;
  snapshot.handoffCount = target.hud().pendingHandoffCount;
  snapshot.presentCount = target.canonicalFrameCount();
  snapshot.surfaceLostCount = target.surfaceLostCount();
  snapshot.sampleHz = target.hud().sampleHz;
  snapshot.frameMs = target.hud().frameMs;
  snapshot.queueAgeMs = target.hud().queueAgeMs;
  snapshot.surfaceAvailable = target.surface().available;
  snapshot.arcPresenterActive = target.previewActive();
  if (target.toolMode() == Host::ToolMode::kObjectEraser) {
    snapshot.selectedTool = 4105;
  } else if (target.toolMode() == Host::ToolMode::kPartialEraser) {
    snapshot.selectedTool = 4106;
  } else {
    const auto& profile = target.selectedBrushProfile();
    snapshot.selectedTool = profile == "vector-solid-v1" ? 4101U
        : profile == "marker-flat-v1" ? 4102U
        : profile == "chalk-grain-v1" ? 4103U : 4104U;
  }
  snapshot.canUndo = runtime.canUndo;
  snapshot.canRedo = runtime.canRedo;
  snapshot.capabilities.fill(canvas::debug_ui::CapabilityState::kUnavailable);
  snapshot.capabilities[static_cast<unsigned int>(canvas::debug_ui::Capability::kInput)] =
      canvas::debug_ui::CapabilityState::kAvailable;
  snapshot.capabilities[static_cast<unsigned int>(canvas::debug_ui::Capability::kCanonicalSurface)] =
      canvas::debug_ui::CapabilityState::kAvailable;
  snapshot.capabilities[static_cast<unsigned int>(canvas::debug_ui::Capability::kArcPreviewSurface)] =
      target.previewSurfaceProvider() == nullptr ? canvas::debug_ui::CapabilityState::kUnavailable
                                                  : canvas::debug_ui::CapabilityState::kAvailable;
  snapshot.capabilities[static_cast<unsigned int>(canvas::debug_ui::Capability::kTelemetry)] =
      canvas::debug_ui::CapabilityState::kAvailable;
  return snapshot;
}

bool initializeDebugUi(Handle value, Host& target, int width, int height) {
  auto& state = debugUi()[value];
  state.runtime = std::make_unique<WebRuntimeFacade>(target);
  state.provider = std::make_unique<canvas::render::WebGlSurfaceProvider>(
      canvas::render::WebGlSurfaceConfig{"#debugUi", static_cast<std::uint32_t>(width),
                                         static_cast<std::uint32_t>(height), 0});
  if (!state.provider || !state.provider->ready()) return false;
  IMGUI_CHECKVERSION();
  state.context = ImGui::CreateContext();
  if (!state.context) return false;
  ImGui::SetCurrentContext(state.context);
  ImGui::StyleColorsDark();
  auto& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
  unsigned char* pixels = nullptr; int fontWidth = 0; int fontHeight = 0;
  io.Fonts->GetTexDataAsAlpha8(&pixels, &fontWidth, &fontHeight);
  if (!pixels || fontWidth <= 0 || fontHeight <= 0) return false;
  const auto info = SkImageInfo::MakeA8(fontWidth, fontHeight);
  state.fontTexture = SkImages::RasterFromPixmapCopy(
      SkPixmap(info, pixels, info.minRowBytes()));
  state.width = width; state.height = height;
  state.initialized = state.fontTexture != nullptr;
  return state.initialized;
}

void destroyDebugUi(Handle value) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end()) return;
  if (it->second.context) {
    ImGui::SetCurrentContext(it->second.context);
    ImGui::DestroyContext(it->second.context);
  }
  debugUi().erase(it);
}

bool renderDebugUi(Handle value) {
  const auto hostIt = hosts().find(value);
  const auto uiIt = debugUi().find(value);
  if (hostIt == hosts().end() || uiIt == debugUi().end() ||
      !uiIt->second.initialized || !uiIt->second.visible) return false;
  auto& state = uiIt->second;
  ImGui::SetCurrentContext(state.context);
  auto& io = ImGui::GetIO();
  io.DeltaTime = 1.0F / 60.0F;
  io.DisplaySize = ImVec2(static_cast<float>(state.width), static_cast<float>(state.height));
  const auto drawFrame = [&](const canvas::debug_ui::DebugSnapshot& snapshot) {
    state.selectedTool = snapshot.selectedTool == 0U ? state.selectedTool : snapshot.selectedTool;
    const bool submitted = canvas::debug_ui::buildImGuiPanels(
        snapshot, static_cast<int>(state.selectedTool), state.runtime.get(), nullptr, nullptr);
    ImGui::Render();
    const auto acquired = state.provider->acquire();
    if (acquired.code != canvas::render::SkiaSurfaceAcquireCode::kAcquired) return std::pair<bool, bool>{false, submitted};
    const bool rendered = canvas::debug_ui::ImGuiSkiaRenderer{}.render(
        ImGui::GetDrawData(), acquired.frame.surface, state.fontTexture.get());
    state.provider->release();
    if (!rendered || state.provider->present().code != canvas::render::BackendSubmissionCode::kAccepted) {
      return std::pair<bool, bool>{false, submitted};
    }
    return std::pair<bool, bool>{true, submitted};
  };
  ImGui::NewFrame();
  const auto first = drawFrame(debugSnapshot(*hostIt->second, state));
  if (!first.first) return false;
  // Product controls are committed while building the first frame. Redraw
  // once from the owner snapshot so a brush/eraser click is visible without
  // waiting for a canvas stroke or the next unrelated animation tick.
  if (first.second) {
    ImGui::NewFrame();
    const auto second = drawFrame(debugSnapshot(*hostIt->second, state));
    if (!second.first) return false;
  }
  return true;
}
#endif

bool initBrushes(Handle handle) { (void)handle; return true; }

int submitPlatformBatch(Handle value, std::uint32_t source, std::uint32_t pointer,
                        std::uint64_t sequence, std::uint64_t timestampNs,
                        float x, float y, float pressure, int phase, int family,
                        float contentX, float contentY) {
  (void)contentX;
  (void)contentY;
  auto* target = host(value);
  const auto brushIt = brushes().find(value);
  if (target == nullptr || brushIt == brushes().end()) return 0;
  canvas::input::PlatformPointerBatch batch;
  batch.samples.push_back({source, pointer, sequence, timestampNs, x, y, pressure,
                           0.0F, 0.0F, {}, {},
                           canvas::input::SampleProvenance::kConfirmedCurrent,
                           phase == 0 ? canvas::input::PointerPhase::kDown
                           : phase == 2 ? canvas::input::PointerPhase::kUp
                           : phase == 3 ? canvas::input::PointerPhase::kCancel
                                        : canvas::input::PointerPhase::kMove});
  if (!target->acceptPlatformBatch(batch, timestampNs)) return 0;
  auto& state = brushIt->second;
  if (phase == 2) {
    state.digest = target->brushDigest();
    state.primitiveCount = target->brushPrimitiveCount();
    if (!target->presentCanonicalFrame(target->canonicalFrameCount() + 1U, 0.0)) return 0;
  } else if (phase == 3) {
    (void)target->cancelBrushSession(pointer);
  }
  return 1;
}
}

extern "C" {
EMSCRIPTEN_KEEPALIVE int axiom_ink_platform_batch(
    std::uint32_t value, std::uint32_t source, std::uint32_t pointer,
    std::uint64_t sequence, std::uint64_t timestampNs, float x, float y,
    float pressure, int phase, int family, float contentX, float contentY) {
  return submitPlatformBatch(value, source, pointer, sequence, timestampNs, x, y,
                             pressure, phase, family, contentX, contentY);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_select_brush_profile(
    std::uint32_t value, const char* profile, std::uint32_t revision) {
  return host(value) != nullptr && profile != nullptr &&
         host(value)->selectBrushProfile(profile, revision) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_select_tool(std::uint32_t value, int tool) {
  if (host(value) == nullptr || tool < 0 || tool > 2) return 0;
  return host(value)->selectTool(static_cast<Host::ToolMode>(tool)) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_eraser_begin(std::uint32_t value, std::uint32_t pointer) {
  return host(value) != nullptr && host(value)->eraserBegin(pointer) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_eraser_sample(std::uint32_t value, std::uint32_t pointer,
                                                  float x, float y) {
  return host(value) != nullptr && host(value)->eraserSample(pointer, x, y) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_eraser_finish(std::uint32_t value, std::uint32_t pointer) {
  return host(value) != nullptr && host(value)->eraserFinish(pointer) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_eraser_cancel(std::uint32_t value, std::uint32_t pointer) {
  return host(value) != nullptr && host(value)->eraserCancel(pointer) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE std::uint32_t axiom_ink_create() {
  static Handle nextHandle = 1U;
  while (nextHandle == 0U || hosts().contains(nextHandle)) ++nextHandle;
  const Handle value = nextHandle++;
  hosts().emplace(value, std::make_unique<Host>());
  if (!initBrushes(value)) { hosts().erase(value); return 0; }
  return value;
}
EMSCRIPTEN_KEEPALIVE void axiom_ink_destroy(std::uint32_t value) {
#if AXIOM_WEB_DEBUG_UI
  destroyDebugUi(value);
#endif
  brushes().erase(value);
  hosts().erase(value);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_enabled() { return AXIOM_WEB_DEBUG_UI; }
#if AXIOM_WEB_DEBUG_UI
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_init(std::uint32_t value,
                                                 std::uint32_t width,
                                                 std::uint32_t height) {
  auto* target = host(value);
  return target != nullptr && width != 0U && height != 0U &&
      initializeDebugUi(value, *target, static_cast<int>(width), static_cast<int>(height)) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_toggle(std::uint32_t value) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end() || !it->second.initialized) return 0;
  it->second.visible = !it->second.visible;
  if (it->second.visible) renderDebugUi(value);
  return it->second.visible ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_visible(std::uint32_t value) {
  const auto it = debugUi().find(value);
  return it != debugUi().end() && it->second.visible ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_wants_capture_mouse(std::uint32_t value) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end() || !it->second.initialized || !it->second.visible) return 0;
  ImGui::SetCurrentContext(it->second.context);
  return ImGui::GetIO().WantCaptureMouse ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_wants_capture_keyboard(std::uint32_t value) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end() || !it->second.initialized || !it->second.visible) return 0;
  ImGui::SetCurrentContext(it->second.context);
  return ImGui::GetIO().WantCaptureKeyboard ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_render(std::uint32_t value) {
  return renderDebugUi(value) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_resize(std::uint32_t value,
                                                   std::uint32_t width,
                                                   std::uint32_t height) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end() || width == 0U || height == 0U) return 0;
  // WebGL providers wrap a fixed framebuffer; recreate the independent debug
  // provider while preserving the ImGui context and selected controls.
  it->second.provider.reset();
  it->second.provider = std::make_unique<canvas::render::WebGlSurfaceProvider>(
      canvas::render::WebGlSurfaceConfig{"#debugUi", width, height, 0});
  if (!it->second.provider || !it->second.provider->ready()) return 0;
  it->second.width = static_cast<int>(width);
  it->second.height = static_cast<int>(height);
  return renderDebugUi(value) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_pointer(std::uint32_t value,
                                                    int phase, int pointerId,
                                                    int button, float x, float y) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end() || !it->second.initialized) return 0;
  auto& state = it->second;
  const canvas::debug_ui::DebugInputSequence sequence{
      static_cast<std::uint64_t>(pointerId), 1U};
  const auto owner = phase == 0
      ? state.capture.begin(sequence, canvas::debug_ui::DebugInputOwner::kDebug)
      : state.capture.route(sequence).value_or(canvas::debug_ui::DebugInputOwner::kCanvas);
  if (owner != canvas::debug_ui::DebugInputOwner::kDebug) return 0;
  ImGui::SetCurrentContext(state.context);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(x, y);
  const int mouseButton = button >= 0 && button < 5 ? button : 0;
  if (phase == 0 || phase == 1) io.AddMouseButtonEvent(mouseButton, true);
  if (phase == 2 || phase == 3) io.AddMouseButtonEvent(mouseButton, false);
  if (phase == 2 || phase == 3) (void)state.capture.terminal(sequence);
  return renderDebugUi(value) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_capture_begin(std::uint32_t value,
                                                          std::uint64_t pointerId) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end()) return 0;
  return it->second.capture.begin({pointerId, 1U},
      canvas::debug_ui::DebugInputOwner::kDebug) == canvas::debug_ui::DebugInputOwner::kDebug ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_capture_route(std::uint32_t value,
                                                          std::uint64_t pointerId) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end()) return 0;
  return it->second.capture.route({pointerId, 1U}).value_or(
      canvas::debug_ui::DebugInputOwner::kCanvas) == canvas::debug_ui::DebugInputOwner::kDebug ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_capture_terminal(std::uint32_t value,
                                                             std::uint64_t pointerId) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end()) return 0;
  return it->second.capture.terminal({pointerId, 1U}) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_key(std::uint32_t value,
                                                 int key, int down) {
  const auto it = debugUi().find(value);
  if (it == debugUi().end() || !it->second.initialized) return 0;
  ImGui::SetCurrentContext(it->second.context);
  auto& state = it->second;
  const bool isDown = down != 0;
  const auto imguiKey = static_cast<ImGuiKey>(key);
  if (imguiKey == ImGuiKey_LeftCtrl || imguiKey == ImGuiKey_RightCtrl) {
    state.ctrlDown = isDown;
  }
  ImGui::GetIO().AddKeyEvent(imguiKey, isDown);
  if (isDown && state.ctrlDown && imguiKey == ImGuiKey_Z) {
    (void)state.runtime->undo(state.nextRequestId++, state.runtime->readDiagnostics().runtimeGeneration);
  } else if (isDown && state.ctrlDown && imguiKey == ImGuiKey_Y) {
    (void)state.runtime->redo(state.nextRequestId++, state.runtime->readDiagnostics().runtimeGeneration);
  }
  return renderDebugUi(value) ? 1 : 0;
}
#else
// Keep the exported seam callable in Debug-OFF builds so the shared Web page
// can load the same host contract without pulling ImGui into the binary.
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_init(std::uint32_t, std::uint32_t, std::uint32_t) { return 1; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_toggle(std::uint32_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_visible(std::uint32_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_wants_capture_mouse(std::uint32_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_wants_capture_keyboard(std::uint32_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_render(std::uint32_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_resize(std::uint32_t, std::uint32_t, std::uint32_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_pointer(std::uint32_t, int, int, int, float, float) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_capture_begin(std::uint32_t, std::uint64_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_capture_route(std::uint32_t, std::uint64_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_capture_terminal(std::uint32_t, std::uint64_t) { return 0; }
EMSCRIPTEN_KEEPALIVE int axiom_ink_debug_ui_key(std::uint32_t, int, int) { return 0; }
#endif
EMSCRIPTEN_KEEPALIVE int axiom_ink_selected_tool(std::uint32_t value) {
  const auto* target = host(value);
  if (target == nullptr) return 0;
  if (target->toolMode() == Host::ToolMode::kObjectEraser) return 4105;
  if (target->toolMode() == Host::ToolMode::kPartialEraser) return 4106;
  const auto& profile = target->selectedBrushProfile();
  return profile == "vector-solid-v1" ? 4101
      : profile == "marker-flat-v1" ? 4102
      : profile == "chalk-grain-v1" ? 4103 : 4104;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_begin(std::uint32_t value, std::uint32_t strokeId) {
  return host(value) != nullptr && host(value)->beginStroke(strokeId);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_batch(
    std::uint32_t value, const canvas::input::PointerSample* samples,
    std::uint32_t count, std::uint32_t observationTimeNs) {
  if (host(value) == nullptr || samples == nullptr || count == 0) return 0;
  canvas::input::PointerSampleBatch batch;
  batch.samples.assign(samples, samples + count);
  return host(value)->accept(batch, observationTimeNs);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_sample(
    std::uint32_t value, std::uint32_t sequence, std::uint32_t timestampNs,
    float x, float y, float pressure, int predicted, std::uint32_t observationTimeNs) {
  if (host(value) == nullptr) return 0;
  canvas::input::PointerSampleBatch batch;
  batch.samples.push_back({sequence, timestampNs, x, y, pressure, predicted != 0});
  return host(value)->accept(batch, observationTimeNs);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_begin_pointer(
    std::uint32_t value, std::uint32_t source, std::uint32_t pointer,
    std::uint32_t generation, std::uint32_t strokeId) {
  return host(value) != nullptr &&
         host(value)->beginStroke({source, pointer, generation}, strokeId);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_pointer_sample(
    std::uint32_t value, std::uint32_t source, std::uint32_t pointer,
    std::uint32_t generation, std::uint32_t sequence, std::uint32_t timestampNs,
    float x, float y, float pressure, int predicted, std::uint32_t observationTimeNs) {
  if (host(value) == nullptr) return 0;
  canvas::input::PointerSampleBatch batch;
  batch.samples.push_back({sequence, timestampNs, x, y, pressure, predicted != 0,
                           {source, pointer, generation}});
  return host(value)->accept(batch, observationTimeNs);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_pointer_sample_phase(
    std::uint32_t value, std::uint32_t source, std::uint32_t pointer,
    std::uint32_t generation, std::uint32_t sequence, std::uint32_t timestampNs,
    float x, float y, float pressure, int predicted, int phase,
    std::uint32_t observationTimeNs) {
  if (host(value) == nullptr) return 0;
  canvas::input::PointerSampleBatch batch;
  const auto pointerPhase = phase == 0 ? canvas::input::PointerPhase::kDown
      : phase == 2 ? canvas::input::PointerPhase::kUp
      : phase == 3 ? canvas::input::PointerPhase::kCancel
      : canvas::input::PointerPhase::kMove;
  batch.samples.push_back({sequence, timestampNs, x, y, pressure, predicted != 0,
                           {source, pointer, generation}, {}, {}, pointerPhase});
  return host(value)->accept(batch, observationTimeNs);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_commit_pointer(
    std::uint32_t value, std::uint32_t source, std::uint32_t pointer,
    std::uint32_t generation, std::uint32_t strokeId, std::uint32_t operationId) {
  return host(value) != nullptr &&
         host(value)->commitStroke({source, pointer, generation}, strokeId, operationId);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_cancel_pointer(
    std::uint32_t value, std::uint32_t source, std::uint32_t pointer,
    std::uint32_t generation) {
  return host(value) != nullptr &&
         host(value)->cancelStroke({source, pointer, generation});
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_begin(std::uint32_t value, std::uint32_t pointer,
                                               std::uint32_t family) {
  const auto found = brushes().find(value);
  if (found == brushes().end()) return 0;
  auto& state = found->second;
  state.family = family;
  return host(value) != nullptr && host(value)->beginBrushSession(pointer, family) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_sample(std::uint32_t value, std::uint32_t pointer,
                                                std::uint32_t sequence, float x, float y, float pressure) {
  const auto found = brushes().find(value);
  if (found == brushes().end()) return 0;
  return host(value) != nullptr && host(value)->appendBrushSample(pointer, x, y, pressure, sequence) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_finish(std::uint32_t value, std::uint32_t pointer) {
  const auto found = brushes().find(value);
  if (found == brushes().end()) return 0;
  auto& state = found->second;
  if (host(value) == nullptr || !host(value)->finishBrushSession(pointer)) return 0;
  state.digest = host(value)->brushDigest();
  state.primitiveCount = host(value)->brushPrimitiveCount();
  return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_render(std::uint32_t value) {
  const auto found = brushes().find(value);
  if (found == brushes().end()) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto& state = found->second;
  if (!state.renderer) return 0;
  auto* provider = host(value)->activeSurfaceProvider();
  if (provider == nullptr) return 0;
  return host(value)->presentBrushPreview() ? 1 : 0;
#else
  return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_preview_render(std::uint32_t value) {
  const auto found = brushes().find(value);
  if (found == brushes().end() || host(value) == nullptr) return 0;
  return host(value)->presentBrushPreview() ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_ink_render_submission_count(std::uint32_t value) {
  const auto found = brushes().find(value);
#if defined(CANVAS_RENDER_HAS_SKIA)
  return found == brushes().end() || !found->second.renderer ? 0 : found->second.renderer->submissionCount();
#else
  (void)value; return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_ink_render_flush_count(std::uint32_t value) {
  const auto found = brushes().find(value);
#if defined(CANVAS_RENDER_HAS_SKIA)
  return found == brushes().end() || host(value) == nullptr || host(value)->activeSurfaceProvider() == nullptr
      ? 0 : host(value)->activeSurfaceProvider()->presentCount();
#else
  (void)value; return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE const char* axiom_ink_run_baseline_observation(
    std::uint32_t value, const char* artifactIdentity) {
  static std::string json;
  const auto found = brushes().find(value);
  if (host(value) == nullptr || found == brushes().end()) return nullptr;
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (!found->second.renderer || host(value)->activeSurfaceProvider() == nullptr) return nullptr;
  const auto submissions = found->second.renderer->submissionCount();
  const auto presents = host(value)->activeSurfaceProvider()->presentCount();
#else
  const std::uint64_t submissions = 0;
  const std::uint64_t presents = 0;
#endif
  json = canvas::ink_playground::platformBrushBaselineObservationJson(
      "web", "HOSTED", artifactIdentity == nullptr ? "" : artifactIdentity,
      *host(value),
      {"PointerEvent.coalescedEvents→WASM→PlatformPointerBatch", "C++ InkPlaygroundHost",
       "Skia Ganesh", "WebGL2 canvas framebuffer", submissions, 0, 0, presents, "true"});
  return json.c_str();
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_brush_size(std::uint32_t value, std::uint32_t pointer) {
  const auto state = brushes().find(value); if (state == brushes().end()) return 6.0F;
  (void)pointer; return 1.0F;
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_brush_opacity(std::uint32_t value, std::uint32_t pointer) {
  const auto state = brushes().find(value); if (state == brushes().end()) return 1.0F;
  (void)pointer; return 1.0F;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_representation(std::uint32_t value, std::uint32_t pointer) {
  const auto state = brushes().find(value); if (state == brushes().end()) return 1;
  (void)pointer; return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_viewport_claimed(std::uint32_t value) {
  return host(value) != nullptr && host(value)->viewportGestureClaimed();
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_viewport_scale(std::uint32_t value) {
  return host(value) == nullptr ? 1.0F : host(value)->viewportGesture().scale;
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_viewport_translation_x(std::uint32_t value) {
  return host(value) == nullptr ? 0.0F : host(value)->viewportGesture().translationX;
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_viewport_translation_y(std::uint32_t value) {
  return host(value) == nullptr ? 0.0F : host(value)->viewportGesture().translationY;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_apply_viewport_wheel_pan(
    std::uint32_t value, float deltaX, float deltaY) {
  if (host(value) == nullptr) return 0;
  return host(value)->applyViewportNavigation(
      {canvas::interaction::ViewportNavigationKind::kWheelPan, deltaX, deltaY}) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_apply_viewport_ctrl_wheel_zoom(
    std::uint32_t value, float deltaY, float anchorX, float anchorY) {
  if (host(value) == nullptr) return 0;
  return host(value)->applyViewportNavigation(
      {canvas::interaction::ViewportNavigationKind::kCtrlWheelZoom, 0.0F, deltaY,
       anchorX, anchorY}) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_apply_viewport_gesture(
    std::uint32_t value, float scaleDelta, float anchorX, float anchorY) {
  if (host(value) == nullptr) return 0;
  return host(value)->applyViewportNavigation(
      {canvas::interaction::ViewportNavigationKind::kBrowserGesture, 0.0F, 0.0F,
       anchorX, anchorY, scaleDelta}) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_commit(
    std::uint32_t value, std::uint32_t strokeId, std::uint32_t operationId) {
  return host(value) != nullptr && host(value)->commitStroke(strokeId, operationId);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_bind_surface(
    std::uint32_t value, std::uint32_t width, std::uint32_t height) {
  if (host(value) == nullptr || !host(value)->bindSurface(width, height)) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto& state = brushes()[value];
  // The Web App owns the HTMLCanvasElement and WebGL2 context. The provider
  // only wraps the current app-owned context; it never queries #ink or
  // creates a DOM resource on the WASM side.
  // The Emscripten WebGL registry only knows contexts created through its
  // html5_webgl API.  A raw canvas.getContext() is not discoverable through
  // emscripten_webgl_get_current_context(), so create/register the app-owned
  // canvas context here and keep its lifetime in the provider.
  auto provider = std::make_unique<canvas::render::WebGlSurfaceProvider>(
      canvas::render::WebGlSurfaceConfig{"#ink", width, height, 0});
  if (!provider || !provider->ready()) return 0;
  state.renderer = std::make_unique<canvas::render::SkiaRenderer>();
  if (!host(value)->registerSurfaceProvider("webgl-window", std::move(provider))) return 0;
#endif
  return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_bind_preview_surface(
    std::uint32_t value, std::uint32_t width, std::uint32_t height) {
  if (host(value) == nullptr || width == 0U || height == 0U) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto provider = std::make_unique<canvas::render::WebGlSurfaceProvider>(
      canvas::render::WebGlSurfaceConfig{"#arcPreview", width, height, 0,
          [](bool visible) { axiom_web_preview_visibility(visible ? 1 : 0); }});
  if (!provider || !provider->ready()) {
    if (provider) EM_ASM_({ console.error("ARC WebGL provider: " + UTF8ToString($0)); },
                           provider->error().c_str());
    return 0;
  }
  const bool registered = host(value)->registerPreviewSurfaceProvider(
      "arc-preview-surface", std::move(provider));
  if (!registered) EM_ASM({ console.error("ARC preview provider registration rejected"); });
  return registered ? 1 : 0;
#else
  return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_resize_surface(
    std::uint32_t value, std::uint32_t width, std::uint32_t height) {
  return host(value) != nullptr && host(value)->resizeSurface(width, height);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_lose_surface(std::uint32_t value) {
  return host(value) != nullptr && host(value)->loseSurface();
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_rebind_surface(std::uint32_t value) {
  return host(value) != nullptr && host(value)->rebindSurface();
}
EMSCRIPTEN_KEEPALIVE std::uint32_t axiom_ink_point_count(std::uint32_t value) {
  return host(value) == nullptr ? 0U : host(value)->previewPoints().size();
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_point_x(std::uint32_t value, std::uint32_t index) {
  const auto points = host(value)->previewPoints();
  return index < points.size() ? points[index].x : 0.0F;
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_point_y(std::uint32_t value, std::uint32_t index) {
  const auto points = host(value)->previewPoints();
  return index < points.size() ? points[index].y : 0.0F;
}
}
