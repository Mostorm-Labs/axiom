#pragma once

#include "canvas/semantic/semantic_read_view.hpp"

#include <cstdint>

namespace canvas::testing {

struct FullOracleDigest final {
    std::uint64_t generation = 0;
    std::uint64_t objectCount = 0;
    std::uint64_t digest = 0;
    bool valid = false;
};

[[nodiscard]] FullOracleDigest compileFullOracle(
    const semantic::SemanticReadView& view) noexcept;

} // namespace canvas::testing
