#pragma once

#include <cstdint>

namespace canvas::input {
enum class PointerModifiers : std::uint8_t { kNone = 0, kShift = 1 };
[[nodiscard]] constexpr bool hasShift(PointerModifiers modifiers) noexcept {
    return (static_cast<std::uint8_t>(modifiers) & 1U) != 0U;
}
} // namespace canvas::input
