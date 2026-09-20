#pragma once

#include "canvas/ink/ink_engine.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace canvas::ink {

struct MultiPointerReplayResult final {
  std::vector<StrokeRecord> committed;
  std::vector<input::PointerKey> cancelled;
};

MultiPointerReplayResult replayMultiPointer(
    std::span<const std::pair<input::PointerKey, std::vector<input::PointerSample>>> streams) noexcept;

}  // namespace canvas::ink
