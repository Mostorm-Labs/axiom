#pragma once

#include "canvas/semantic/canonical_commit_record.hpp"

#include <cstdint>

namespace canvas::semantic {

enum class LocalBridgePublicationDisposition : std::uint8_t {
    kEligible = 0,
    kNoEcho,
};

[[nodiscard]] constexpr LocalBridgePublicationDisposition
localBridgePublicationDisposition(const CanonicalCommitRecord& record) noexcept {
    switch (record.source) {
    case ApplySource::kLocalInteraction:
    case ApplySource::kLocalCommand:
    case ApplySource::kUndoRedo:
    case ApplySource::kLocalAIImport:
        return LocalBridgePublicationDisposition::kEligible;
    case ApplySource::kRestoreReplay:
    case ApplySource::kRemoteSync:
    default:
        return LocalBridgePublicationDisposition::kNoEcho;
    }
}

} // namespace canvas::semantic
