#include "ink_playground_host.hpp"

#include <emscripten/emscripten.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace {
using Host = canvas::ink_playground::InkPlaygroundHost;
Host* host(std::uintptr_t value) { return reinterpret_cast<Host*>(value); }
}

extern "C" {
EMSCRIPTEN_KEEPALIVE std::uintptr_t axiom_ink_create() {
  return reinterpret_cast<std::uintptr_t>(new Host());
}
EMSCRIPTEN_KEEPALIVE void axiom_ink_destroy(std::uintptr_t value) { delete host(value); }
EMSCRIPTEN_KEEPALIVE int axiom_ink_begin(std::uintptr_t value, std::uint64_t strokeId) {
  return host(value) != nullptr && host(value)->beginStroke(strokeId);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_batch(
    std::uintptr_t value, const canvas::input::PointerSample* samples,
    std::size_t count, std::uint64_t observationTimeNs) {
  if (host(value) == nullptr || samples == nullptr || count == 0) return 0;
  canvas::input::PointerSampleBatch batch;
  batch.samples.assign(samples, samples + count);
  return host(value)->accept(batch, observationTimeNs);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_sample(
    std::uintptr_t value, std::uint64_t sequence, std::uint64_t timestampNs,
    float x, float y, float pressure, int predicted, std::uint64_t observationTimeNs) {
  if (host(value) == nullptr) return 0;
  canvas::input::PointerSampleBatch batch;
  batch.samples.push_back({sequence, timestampNs, x, y, pressure, predicted != 0});
  return host(value)->accept(batch, observationTimeNs);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_commit(
    std::uintptr_t value, std::uint64_t strokeId, std::uint64_t operationId) {
  return host(value) != nullptr && host(value)->commitStroke(strokeId, operationId);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_bind_surface(
    std::uintptr_t value, std::uint32_t width, std::uint32_t height) {
  return host(value) != nullptr && host(value)->bindSurface(width, height);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_resize_surface(
    std::uintptr_t value, std::uint32_t width, std::uint32_t height) {
  return host(value) != nullptr && host(value)->resizeSurface(width, height);
}
EMSCRIPTEN_KEEPALIVE int axiom_ink_lose_surface(std::uintptr_t value) {
  return host(value) != nullptr && host(value)->loseSurface();
}
EMSCRIPTEN_KEEPALIVE std::size_t axiom_ink_point_count(std::uintptr_t value) {
  return host(value) == nullptr ? 0U : host(value)->previewPoints().size();
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_point_x(std::uintptr_t value, std::size_t index) {
  const auto points = host(value)->previewPoints();
  return index < points.size() ? points[index].x : 0.0F;
}
EMSCRIPTEN_KEEPALIVE float axiom_ink_point_y(std::uintptr_t value, std::size_t index) {
  const auto points = host(value)->previewPoints();
  return index < points.size() ? points[index].y : 0.0F;
}
}
