#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace canvas::ink_playground {

// Android pointer ids are zero-based, while PointerKey reserves zero as invalid.
[[nodiscard]] inline std::optional<std::uint64_t> androidPointerIdentity(
    std::uint64_t platformPointerId) noexcept {
  if (platformPointerId >
      static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
    return std::nullopt;
  }
  return platformPointerId + 1U;
}

}  // namespace canvas::ink_playground
