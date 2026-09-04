#pragma once

#include <cstdint>
#include <string_view>

namespace canvas::verification::g1_06 {

[[nodiscard]] std::uint64_t digestCanonicalProjectionBytes(
    std::string_view bytes) noexcept;

} // namespace canvas::verification::g1_06
