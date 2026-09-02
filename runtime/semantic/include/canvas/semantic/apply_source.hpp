#pragma once

#include <cstdint>

namespace canvas::semantic {

// Runtime provenance only. A3 records this context but deliberately does not
// decide publication or no-echo policy; those are an A4 concern.
enum class ApplySource : std::uint8_t {
    kLocalInteraction = 0,
    kLocalCommand,
    kUndoRedo,
    kLocalAIImport,
    kRestoreReplay,
    kRemoteSync,
};

} // namespace canvas::semantic
