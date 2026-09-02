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
    kRuntimeUnavailable,
    // Local semantic implementation diagnostic; not a protocol outcome.
    kInvalidSemanticValue,
};

} // namespace canvas::semantic
