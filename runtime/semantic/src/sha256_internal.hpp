#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace canvas::semantic::internal {

[[nodiscard]] std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> bytes) noexcept;

}
