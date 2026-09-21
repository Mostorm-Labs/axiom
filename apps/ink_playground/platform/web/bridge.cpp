#include "ink_playground_host.hpp"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <memory>
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
}

extern "C" {
EMSCRIPTEN_KEEPALIVE std::uint32_t axiom_ink_create() {
  static Handle nextHandle = 1U;
  while (nextHandle == 0U || hosts().contains(nextHandle)) ++nextHandle;
  const Handle value = nextHandle++;
  hosts().emplace(value, std::make_unique<Host>());
  return value;
}
EMSCRIPTEN_KEEPALIVE void axiom_ink_destroy(std::uint32_t value) { hosts().erase(value); }
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
  return host(value) != nullptr && host(value)->bindSurface(width, height);
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
