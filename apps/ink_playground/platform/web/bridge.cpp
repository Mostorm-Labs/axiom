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
EMSCRIPTEN_KEEPALIVE int axiom_ink_commit(
    std::uintptr_t value, std::uint64_t strokeId, std::uint64_t operationId) {
  return host(value) != nullptr && host(value)->commitStroke(strokeId, operationId);
}
}
