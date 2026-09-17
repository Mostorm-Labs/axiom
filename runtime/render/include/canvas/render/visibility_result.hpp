#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cstdint>
#include <vector>

namespace canvas::render {

struct VisibilityResult final {
    semantic::SemanticGeneration sceneGeneration{};
    foundation::SceneRevision sceneReadToken{};
    foundation::WorldRect queryWorldRect{};
    std::uint64_t candidatesExamined = 0;
    std::uint64_t visibleRecords = 0;
    std::vector<foundation::ObjectId> backToFront;
};

} // namespace canvas::render
