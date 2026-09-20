#pragma once
#include "canvas/input/pointer_capabilities.hpp"
#include "canvas/input/pointer_phase.hpp"
#include "canvas/input/pointer_key.hpp"

#include <cstdint>

namespace canvas::input {
struct PointerSample final {
  std::uint64_t sequence = 0;
  std::uint64_t timestampNs = 0;
  float x = 0;
  float y = 0;
  float pressure = 0;
  bool predicted = false;
  PointerKey key{};
  ContactGeometry contact{};
  PointerCapabilities capabilities{};
  PointerPhase phase = PointerPhase::kMove;
};
}  // namespace canvas::input
