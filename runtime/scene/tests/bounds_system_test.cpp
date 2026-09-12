#include "canvas/scene/bounds_system.hpp"
#include "canvas/foundation/object_id.hpp"

#include <cassert>
#include <cmath>

int main() {
    using namespace canvas;
    semantic::ObjectRecord record;
    record.id = foundation::ObjectId::fromUint64(1);
    record.kind = semantic::ObjectKind::kShape;
    record.kind_version = 1;
    record.content = semantic::ShapeContent{1, 10.0, 20.0};
    record.transform.tx = 5.0;
    record.transform.ty = -2.0;
    record.properties.entries.push_back({0x101U, semantic::SolidStroke{
        .color = {0.0F, 0.0F, 0.0F, 1.0F}, .width = 4.0}});

    const auto result = scene::computeBounds(record);
    assert(result.geometry.isFiniteAndOrdered());
    assert(result.visual.left < result.geometry.left);
    assert(result.visual.right > result.geometry.right);
    assert(result.world.left == result.visual.left + 5.0F);
    assert(result.world.top == result.visual.top - 2.0F);

    record.kind = semantic::ObjectKind::kGroup;
    record.content = semantic::GroupContent{};
    const auto group = scene::computeBounds(record);
    assert(group.visual.left == 0.0F && group.visual.right == 0.0F);
    return 0;
}
