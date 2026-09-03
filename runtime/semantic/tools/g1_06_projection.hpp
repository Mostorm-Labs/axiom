#pragma once

#include "canvas/semantic/object_store.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace canvas::verification::g1_06 {

struct ProjectionDocumentId final {
    std::array<std::uint8_t, 16> bytes{};

    bool operator==(const ProjectionDocumentId&) const = default;
};

struct SemanticProjection final {
    ProjectionDocumentId document_id{};
    std::uint32_t schema_version = 1;
    std::vector<canvas::semantic::ObjectRecord> objects;

    bool operator==(const SemanticProjection&) const = default;
};

[[nodiscard]] SemanticProjection projectDocument(
    ProjectionDocumentId document_id,
    std::uint32_t schema_version,
    const canvas::semantic::ObjectStore& store);

[[nodiscard]] std::string writeCanonicalProjectionJson(
    const SemanticProjection& projection);

} // namespace canvas::verification::g1_06
