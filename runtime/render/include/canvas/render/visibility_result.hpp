#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/semantic_generation.hpp"
#include "canvas/scene/scene_types.hpp"

#include <cstdint>
#include <vector>

namespace canvas::render {

struct VisibilityChunkSelection final {
    foundation::ObjectId objectId{};
    std::vector<std::uint32_t> chunkIndices;
    bool fullObject = true;

    bool operator==(const VisibilityChunkSelection&) const = default;
};

struct VisibilityResult final {
    semantic::SemanticGeneration sceneGeneration{};
    foundation::SceneRevision sceneReadToken{};
    foundation::WorldRect queryWorldRect{};
    std::uint64_t candidatesExamined = 0;
    std::uint64_t visibleRecords = 0;
    std::vector<foundation::ObjectId> backToFront;
    // Render-owned locality metadata. Object order remains canonical; chunk
    // selections only narrow optional geometry work and may fall back.
    std::vector<VisibilityChunkSelection> chunkSelections{};
};

} // namespace canvas::render
