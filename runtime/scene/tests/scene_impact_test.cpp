#include "canvas/scene/scene_impact.hpp"
#include "canvas/foundation/object_id.hpp"

#include <cassert>

int main() {
    using namespace canvas;
    semantic::ObjectSemanticChange transform{
        .object_id = foundation::ObjectId::fromUint64(1),
        .flags = semantic::SemanticChangeFlags::kTransform};
    const auto impact = scene::classifyImpact(transform);
    assert(impact.local_geometry == scene::DirtyState::kReuse);
    assert(impact.world_bounds == scene::DirtyState::kDirty);
    assert(impact.spatial == scene::DirtyState::kDirty);

    semantic::ObjectSemanticChange opacity{
        .object_id = foundation::ObjectId::fromUint64(2),
        .flags = semantic::SemanticChangeFlags::kProperties,
        .changed_fields = {3U}};
    const auto opacityImpact = scene::classifyImpact(opacity);
    assert(opacityImpact.local_geometry == scene::DirtyState::kReuse);
    assert(opacityImpact.visual_bounds == scene::DirtyState::kReuse);
    assert(opacityImpact.world_bounds == scene::DirtyState::kReuse);
    return 0;
}
