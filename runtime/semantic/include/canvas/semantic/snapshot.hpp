#pragma once

#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/document_id.hpp"
#include "canvas/semantic/object_record.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::semantic {

enum class SnapshotCapability : std::uint8_t {
    kLegacyV1 = 0,
    kShape2Triangle = 1,
};

struct SemanticSnapshot final {
    DocumentId document_id{};
    std::uint32_t schema_version = 0;
    std::vector<ObjectRecord> objects;

    bool operator==(const SemanticSnapshot&) const = default;
};

struct SnapshotDecodeResult final {
    SemanticError error = SemanticError::kNone;
    std::optional<SemanticSnapshot> snapshot;

    [[nodiscard]] bool ok() const noexcept {
        return error == SemanticError::kNone && snapshot.has_value();
    }
};

class SnapshotCodec final {
  public:
    [[nodiscard]] static CodecResult encode(const SemanticSnapshot& snapshot, SnapshotCapability capability = SnapshotCapability::kLegacyV1);
    [[nodiscard]] static SnapshotDecodeResult decode(const std::vector<std::uint8_t>& bytes, SnapshotCapability capability = SnapshotCapability::kLegacyV1);
};

} // namespace canvas::semantic
