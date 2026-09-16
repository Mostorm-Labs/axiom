#pragma once

#include "canvas/semantic/semantic_read_view.hpp"
#include "canvas/foundation/world_geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace canvas::testing {

struct FullOracleRecord final {
    semantic::ObjectId objectId{};
    semantic::ObjectKind kind = semantic::ObjectKind::kShape;
    std::uint32_t kindVersion = 0;
    semantic::Placement placement{};
    semantic::Transform2D transform{};
    semantic::PropertyBag properties{};
    semantic::ObjectContent content{};
    std::vector<semantic::EraseMaskRecord> eraseMasks;
    foundation::WorldRect geometryBounds{};
    foundation::WorldRect visualBounds{};
    foundation::WorldRect worldBounds{};
    std::string referenceGeometryDigest;
    std::vector<semantic::ObjectId> directDependencies;
};

struct FullOracleScene final {
    semantic::SemanticGeneration generation{};
    std::vector<FullOracleRecord> records;
    bool valid = false;
};

[[nodiscard]] FullOracleScene compileFullOracle(
    const semantic::SemanticReadView& view) noexcept;

} // namespace canvas::testing
