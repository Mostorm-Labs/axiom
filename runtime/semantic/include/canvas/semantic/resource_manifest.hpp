#pragma once

#include "canvas/semantic/semantic_id.hpp"
#include "canvas/semantic/operation_id.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace canvas::semantic {

enum class ResourceKind : std::uint32_t { kImage=1, kFont=2, kBrush=3 };
using ResourceRevision = std::uint64_t;
using ContentHash = std::string;

enum class ResourceExpectationState : std::uint32_t { kNeverUsed=0, kActive=1, kRetired=2 };

struct ResourceManifestEntry final {
    ResourceId resource_id{};
    ResourceKind resource_kind = ResourceKind::kImage;
    ResourceRevision resource_revision = 0;
    ContentHash content_hash{};
    std::uint64_t byte_length = 0;
    std::uint32_t decode_version = 0;
    std::uint32_t layout_version = 0;
    std::vector<std::uint8_t> metadata{};
    bool operator==(const ResourceManifestEntry&) const = default;
};

struct ResourceManifestTombstone final {
    ResourceId resource_id{};
    ResourceRevision retired_revision = 0;
    ContentHash last_content_hash{};
    OperationId removal_operation_id{};
    ResourceManifestEntry preimage{};
    std::vector<std::uint8_t> lineage_token{};
    bool operator==(const ResourceManifestTombstone&) const = default;
};

struct ResourceManifest final {
    std::uint32_t schema_version = 1;
    std::vector<ResourceManifestEntry> entries{};
    ResourceRevision manifest_revision = 0;
    std::vector<ResourceManifestTombstone> retired_entries{};
    bool operator==(const ResourceManifest&) const = default;
};

struct ResourceExpectation final {
    ResourceId resource_id{};
    ResourceExpectationState state = ResourceExpectationState::kNeverUsed;
    ResourceRevision revision = 0;
    bool operator==(const ResourceExpectation&) const = default;
};

struct ReactivateResource final {
    ResourceManifestEntry entry{};
    OperationId expected_removal_operation_id{};
    ResourceManifestEntry expected_preimage{};
    std::vector<std::uint8_t> expected_lineage_token{};
    bool operator==(const ReactivateResource&) const = default;
};

[[nodiscard]] bool validateResourceManifestEntry(const ResourceManifestEntry&) noexcept;
[[nodiscard]] bool normalizeResourceManifestEntry(ResourceManifestEntry&) noexcept;
[[nodiscard]] bool validateResourceManifest(const ResourceManifest&) noexcept;
[[nodiscard]] bool normalizeResourceManifest(ResourceManifest&) noexcept;
[[nodiscard]] bool resourceKindMetadataValid(ResourceKind, const std::vector<std::uint8_t>&) noexcept;
[[nodiscard]] bool contentHashValid(const ContentHash&) noexcept;
[[nodiscard]] std::vector<std::uint8_t> resourceManifestLineageToken(
    const canvas::foundation::ObjectId& document_id,
    const ResourceManifestTombstone&) noexcept;

} // namespace canvas::semantic

