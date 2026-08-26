#pragma once

#include <cmath>

namespace canvas::semantic {

[[nodiscard]] constexpr double canonicalizeNegativeZero(double value) noexcept {
    return value == 0.0 ? 0.0 : value;
}

[[nodiscard]] inline bool isCanonicalFinite(double value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] inline bool normalizeFinite(double input, double& output) noexcept {
    if (!isCanonicalFinite(input)) {
        return false;
    }
    output = canonicalizeNegativeZero(input);
    return true;
}

} // namespace canvas::semantic
