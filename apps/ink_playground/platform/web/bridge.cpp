#include "ink_playground_host.hpp"
#include "canvas/ink/programmable_brush.hpp"
#include "canvas/ink/reference_brush_catalog.hpp"
#include "canvas/render/webgl_surface_backend.hpp"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <cmath>
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
  canvas::ink::ReferenceBrushCatalog catalog =
      canvas::ink::makeReferenceBrushCatalog();
  canvas::ink::BrushRuntime runtime{catalog.resources};
  std::unordered_map<std::uint32_t, std::shared_ptr<const canvas::ink::BrushProgram>> programs;
  std::unordered_map<std::uint32_t, std::uint64_t> sessions;
  std::unordered_map<std::uint32_t, std::vector<canvas::ink::BrushPrimitive>> primitives;
  std::vector<canvas::ink::BrushPrimitive> committedPrimitives;
  std::uint64_t serial = 0;
  std::uint64_t digest = 0;
  std::uint64_t primitiveCount = 0;
  std::uint32_t family = 1;
  int representation = 1;
  canvas::render::CanonicalViewportTransform renderViewport{};
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::WebGlSurfaceBackend> renderer;
#endif
};
std::unordered_map<Handle, BrushState>& brushes() {
  static std::unordered_map<Handle, BrushState> values;
  return values;
}
canvas::ink::BrushDefinition brushDefinition(
    const canvas::ink::ReferenceBrushCatalog& catalog, std::uint32_t family) {
  if (family == 2U) {
    auto definition = catalog.presets[0].definition;
    definition.family = canvas::ink::BrushFamily::kPencil;
    return definition;
  }
  if (family == 3U) {
    auto definition = catalog.presets[2].definition;
    definition.family = canvas::ink::BrushFamily::kChalk;
    return definition;
  }
  if (family == 4U) {
    auto definition = catalog.presets[1].definition;
    definition.family = canvas::ink::BrushFamily::kMarker;
    return definition;
  }
  if (family == 5U) {
    auto definition = catalog.presets[3].definition;
    definition.family = canvas::ink::BrushFamily::kWaterColorLite;
    return definition;
  }
  canvas::ink::BrushDefinition definition;
  definition.definitionId = family;
  definition.family = static_cast<canvas::ink::BrushFamily>(family);
  definition.version = 2;
  definition.nominalSize = family == 6 ? 18.0F : family == 7 ? 7.0F : 6.0F;
  definition.opacity = family == 6 ? 0.34F : 0.8F;
  definition.spacing = family == 6 ? 0.18F : 0.08F;
  return definition;
}
bool initBrushes(Handle handle) {
  auto& state = brushes()[handle];
  for (std::uint32_t family = 1; family <= 7; ++family) {
    const auto result = canvas::ink::BrushCompiler{}.compile(
        brushDefinition(state.catalog, family), {.pressure = true, .tilt = true,
          .shapeResource = true, .grainResource = true, .temporalTransient = true});
    if (!result) return false;
    state.programs.emplace(family, result.program);
  }
  return true;
}
}

extern "C" {
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
  if (found == brushes().end() || !found->second.programs.contains(family)) return 0;
  auto& state = found->second;
  const auto session = ++state.serial;
  if (!state.runtime.begin({session}, *state.programs.at(family), 0x4500ULL + family + session)) return 0;
  state.sessions[pointer] = session;
  state.family = family;
  return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_sample(std::uint32_t value, std::uint32_t pointer,
                                                std::uint32_t sequence, float x, float y, float pressure) {
  const auto found = brushes().find(value);
  if (found == brushes().end()) return 0;
  auto& state = found->second;
  const auto session = state.sessions.find(pointer);
  if (session == state.sessions.end()) return 0;
  const canvas::ink::BrushInputSample sample{x, y, pressure, 0.0F, 0.0F, sequence};
  const auto result = state.runtime.append({session->second}, std::span<const canvas::ink::BrushInputSample>(&sample, 1));
  if (result && !result.preview.primitives.empty()) {
    state.primitives[pointer] = result.preview.primitives;
    state.representation = static_cast<int>(result.preview.primitives.back().representation);
  }
  return result ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_finish(std::uint32_t value, std::uint32_t pointer) {
  const auto found = brushes().find(value);
  if (found == brushes().end()) return 0;
  auto& state = found->second;
  const auto session = state.sessions.find(pointer);
  if (session == state.sessions.end()) return 0;
  const auto result = state.runtime.finish({session->second});
  if (!result) return 0;
  state.digest = result.commit.digest;
  state.primitiveCount = result.commit.primitives.size();
  state.committedPrimitives.insert(state.committedPrimitives.end(),
                                   result.commit.primitives.begin(),
                                   result.commit.primitives.end());
  state.primitives.erase(pointer);
  state.sessions.erase(session);
  return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_cancel(std::uint32_t value, std::uint32_t pointer) {
  const auto found = brushes().find(value);
  if (found == brushes().end()) return 0;
  auto& state = found->second;
  const auto session = state.sessions.find(pointer);
  if (session == state.sessions.end()) return 0;
  const bool cancelled = state.runtime.cancel({session->second});
  state.sessions.erase(session);
  state.primitives.erase(pointer);
  return cancelled ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_render(std::uint32_t value) {
  const auto found = brushes().find(value);
  if (found == brushes().end()) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto& state = found->second;
  if (!state.renderer) return 0;
  std::vector<canvas::ink::BrushPrimitive> primitives;
  primitives = state.committedPrimitives;
  primitives.reserve(primitives.size() + state.primitives.size());
  for (const auto& [pointer, preview] : state.primitives) {
    (void)pointer;
    primitives.insert(primitives.end(), preview.begin(), preview.end());
  }
  const auto runtimeViewport = host(value)->viewportGesture();
  const auto viewport = canvas::render::CanonicalViewportTransform{
      runtimeViewport.scale * state.renderViewport.scale,
      runtimeViewport.translationX * state.renderViewport.scale + state.renderViewport.translationX,
      runtimeViewport.translationY * state.renderViewport.scale + state.renderViewport.translationY};
  return state.renderer->submitBrushPrimitives(
      primitives, canvas::render::CanonicalViewportTransform{
          viewport.scale, viewport.translationX, viewport.translationY}).code ==
      canvas::render::BackendSubmissionCode::kAccepted ? 1 : 0;
#else
  return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_set_render_viewport(
    std::uint32_t value, float scale, float translationX, float translationY) {
  const auto found = brushes().find(value);
  if (found == brushes().end() || !std::isfinite(scale) || scale <= 0.0F ||
      !std::isfinite(translationX) || !std::isfinite(translationY)) return 0;
  found->second.renderViewport = {scale, translationX, translationY};
  return 1;
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_brush_size(std::uint32_t value, std::uint32_t pointer) {
  const auto state = brushes().find(value); if (state == brushes().end()) return 6.0F;
  const auto primitive = state->second.primitives.find(pointer);
  return primitive == state->second.primitives.end() || primitive->second.empty() ? 6.0F : primitive->second.back().size;
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_brush_opacity(std::uint32_t value, std::uint32_t pointer) {
  const auto state = brushes().find(value); if (state == brushes().end()) return 1.0F;
  const auto primitive = state->second.primitives.find(pointer);
  return primitive == state->second.primitives.end() || primitive->second.empty() ? 1.0F : primitive->second.back().opacity;
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_brush_representation(std::uint32_t value, std::uint32_t pointer) {
  const auto state = brushes().find(value); if (state == brushes().end()) return 1;
  const auto primitive = state->second.primitives.find(pointer);
  return primitive == state->second.primitives.end() || primitive->second.empty() ? 1 : static_cast<int>(primitive->second.back().representation);
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
