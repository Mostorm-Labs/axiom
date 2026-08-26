#pragma once

#include <cstdint>

namespace canvas::semantic {

enum class SemanticError : std::uint8_t {
    kNone = 0,
    kTruncatedWire,
    kUnsupportedVersion,
    kUnknownOperation,
    kDuplicateCanonicalKey,
    kNonCanonicalOrder,
    kNonFiniteValue,
    kLimitExceeded,
    kMalformedWire,
};

} // namespace canvas::semantic
