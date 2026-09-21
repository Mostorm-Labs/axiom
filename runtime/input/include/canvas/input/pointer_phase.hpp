#pragma once

#include <cstdint>

namespace canvas::input {
enum class PointerPhase : std::uint8_t { kDown, kMove, kUp, kCancel };
}
