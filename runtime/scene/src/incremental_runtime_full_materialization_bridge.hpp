#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/semantic_generation.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

namespace canvas::internal {

struct FullMaterializedScene final {
    semantic::SemanticGeneration generation{};
    struct Record final {
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
    std::vector<Record> records;
};

foundation::Result<FullMaterializedScene> materializeFullScene(
    const semantic::SemanticReadView& view);

} // namespace canvas::internal
