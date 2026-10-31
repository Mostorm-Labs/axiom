#pragma once

#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/document_id.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/resource_manifest.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::semantic {

enum class SnapshotCapability : std::uint8_t {
    kLegacyV1 = 0,
    kShape2Triangle = 1,
    // Explicit capability bits; 0/1 retain their released identities.
    kRichTextFixedWidthV2 = 2,
    kShape2TriangleAndRichTextFixedWidthV2 = 3,
    kRichTextFormattingV3 = 6,
    kShape2TriangleAndRichTextFormattingV3 = 7,
    kShape2TriangleRichTextFormattingAndConnectorAttachmentV1 = 15,
    // Section/1 requires an explicit reader capability and a v3 manifest.
    kSectionV1 = 16,
    kShape2TriangleRichTextFormattingConnectorAndSectionV1 = 31,
    kAllWithBackgroundV1 = 63,
};

struct SemanticSnapshot final {
    DocumentId document_id{};
    std::uint32_t schema_version = 0;
    std::vector<ObjectRecord> objects;
    ResourceManifest manifest{};
    bool manifest_present = false;
    std::uint64_t document_revision = 0;
    std::vector<std::uint8_t> recovery_frontier;
    std::uint32_t digest_algorithm = 0;
    std::vector<std::uint8_t> document_digest;
    std::vector<std::uint8_t> capability_requirements;
    std::vector<std::uint8_t> checkpoint_digest;
    foundation::DocumentBackground background{};

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
    // Captured objects/manifest/revision/frontier must already share one read view.
    // Derives the canonical requirements from the captured object values.
    [[nodiscard]] static bool populateRequirements(SemanticSnapshot& snapshot);
    // Computes the accepted algorithm1 projections; does not invent a frontier.
    [[nodiscard]] static bool bindDigests(SemanticSnapshot&, SnapshotCapability capability = SnapshotCapability::kLegacyV1);
    [[nodiscard]] static CodecResult encode(const SemanticSnapshot& snapshot, SnapshotCapability capability = SnapshotCapability::kLegacyV1);
    [[nodiscard]] static SnapshotDecodeResult decode(const std::vector<std::uint8_t>& bytes, SnapshotCapability capability = SnapshotCapability::kLegacyV1);
};

} // namespace canvas::semantic
