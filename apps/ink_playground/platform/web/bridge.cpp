#include "ink_playground_host.hpp"
#include "canvas/render/webgl_surface_backend.hpp"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>

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
  std::unique_ptr<canvas::render::WebGlSurfaceBackend> renderer;
#endif
};
std::unordered_map<Handle, BrushState>& brushes() {
  static std::unordered_map<Handle, BrushState> values;
  return values;
}
bool initBrushes(Handle handle) { (void)handle; return true; }

int submitPlatformBatch(Handle value, std::uint32_t source, std::uint32_t pointer,
                        std::uint64_t sequence, std::uint64_t timestampNs,
                        float x, float y, float pressure, int phase, int family,
                        float contentX, float contentY) {
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
  if (phase == 0 && !target->beginBrushSession(pointer, static_cast<std::uint32_t>(family))) return 0;
  if (phase != 0 && !target->viewportGestureClaimed() &&
      !target->appendBrushSample(pointer, contentX, contentY, pressure, sequence)) return 0;
  if (phase == 2) {
    if (!target->finishBrushSession(pointer)) return 0;
    state.digest = target->brushDigest();
    state.primitiveCount = target->brushPrimitiveCount();
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
EMSCRIPTEN_KEEPALIVE std::uint32_t axiom_ink_create() {
  static Handle nextHandle = 1U;
  while (nextHandle == 0U || hosts().contains(nextHandle)) ++nextHandle;
  const Handle value = nextHandle++;
  hosts().emplace(value, std::make_unique<Host>());
  if (!initBrushes(value)) { hosts().erase(value); return 0; }
  return value;
}
EMSCRIPTEN_KEEPALIVE void axiom_ink_destroy(std::uint32_t value) { brushes().erase(value); hosts().erase(value); }
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
  const auto points = host(value)->brushRenderPoints();
  return state.renderer->submitBrushPoints(points).code ==
      canvas::render::BackendSubmissionCode::kAccepted ? 1 : 0;
#else
  return 0;
#endif
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
  state.renderer = std::make_unique<canvas::render::WebGlSurfaceBackend>(
      canvas::render::WebGlSurfaceConfig{"#ink", width, height});
  if (!state.renderer->ready()) return 0;
#endif
  return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_resize_surface(
    std::uint32_t value, std::uint32_t width, std::uint32_t height) {
  return host(value) != nullptr && host(value)->resizeSurface(width, height);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_lose_surface(std::uint32_t value) {
  return host(value) != nullptr && host(value)->loseSurface();
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
