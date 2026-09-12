#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace canvas::scene {

struct InspectionRecord final {
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
    bool operator==(const InspectionRecord&) const = default;
};

struct RuntimeSceneProjection final {
    semantic::SemanticGeneration generation{};
    std::vector<InspectionRecord> records;
    bool operator==(const RuntimeSceneProjection&) const = default;
    [[nodiscard]] const InspectionRecord* find(semantic::ObjectId id) const noexcept;
};

[[nodiscard]] foundation::Result<RuntimeSceneProjection> projectSemanticScene(
    const semantic::SemanticReadView& view);

} // namespace canvas::scene

namespace canvas { using scene::InspectionRecord; using scene::RuntimeSceneProjection; using scene::projectSemanticScene; }
