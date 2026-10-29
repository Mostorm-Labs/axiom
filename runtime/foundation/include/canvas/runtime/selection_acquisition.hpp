#pragma once
#include <cstdint>

namespace canvas::runtime {
enum class CanvasSelectionMode : std::uint8_t { kPoint, kRectangle, kFreeform };
enum class SelectionQueryStatus : std::uint8_t { kIdle, kComplete, kPartial, kStale, kFailed, kInvalid, kCancelled };
} // namespace canvas::runtime
