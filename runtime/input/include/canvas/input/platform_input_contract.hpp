#pragma once

#include "canvas/input/pointer_capabilities.hpp"
#include "canvas/input/pointer_key.hpp"
#include "canvas/input/pointer_phase.hpp"
#include "canvas/input/sample_provenance.hpp"

#include <cstdint>
#include <vector>

namespace canvas::input {

struct PlatformPointerSample final {
  InputSourceId source = 0;
  PointerId pointer = 0;
  std::uint64_t sequence = 0;
  std::uint64_t timestampNs = 0;
  float x = 0.0F;
  float y = 0.0F;
  float pressure = 0.0F;
  float tiltX = 0.0F;
  float tiltY = 0.0F;
  ContactGeometry contact{};
  PointerCapabilities capabilities{};
  SampleProvenance provenance = SampleProvenance::kConfirmedCurrent;
  PointerPhase phase = PointerPhase::kMove;
};

struct PlatformPointerBatch final {
  std::vector<PlatformPointerSample> samples;
  bool terminalCancel = false;
  bool sourceLost = false;
};

}  // namespace canvas::input
