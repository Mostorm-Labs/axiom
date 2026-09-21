#pragma once

#include <cstdint>

namespace canvas::interaction {
enum class MultiContactPolicy : std::uint8_t { kAutoIntent, kMultiInk, kGesturePriority };
}
