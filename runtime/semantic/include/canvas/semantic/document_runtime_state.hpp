#pragma once

#include <cstdint>

namespace canvas::semantic {

enum class DocumentRuntimeState : std::uint8_t {
    kConstructed = 0,
    kLoading,
    kReady,
    kSuspended,
    kClosing,
    kClosed,
    kFailed,
};

} // namespace canvas::semantic
