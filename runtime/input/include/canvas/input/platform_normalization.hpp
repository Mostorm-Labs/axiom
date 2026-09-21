#pragma once

#include "canvas/input/pointer_capabilities.hpp"
#include "canvas/input/pointer_key.hpp"
#include "canvas/input/pointer_phase.hpp"

#include <cstdint>

namespace canvas::input {

struct NormalizedPlatformPointer final {
  PointerKey key{};
  PointerCapabilities capabilities{};
  ContactGeometry contact{};
  PointerPhase phase = PointerPhase::kMove;
};

[[nodiscard]] inline NormalizedPlatformPointer normalizePlatformPointer(
    InputSourceId source, PointerId pointer, PointerGeneration generation,
    PointerTool tool, bool pressure, bool eraser, ContactGeometry contact,
    PointerPhase phase = PointerPhase::kMove) noexcept {
  return {{source, pointer, generation},
          {tool, pressure, false, eraser, true, contact.available}, contact, phase};
}

}  // namespace canvas::input
