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

    record.properties.entries.clear();
    record.transform = semantic::Transform2D{0.0, 1.0, -1.0, 0.0, 7.0, 11.0};
    const auto rotated = scene::computeBounds(record);
    assert(rotated.world.left <= -13.0F && rotated.world.right >= 7.0F);
    assert(rotated.world.top <= 11.0F && rotated.world.bottom >= 21.0F);

    record.kind = semantic::ObjectKind::kVectorStroke;
    record.content = semantic::VectorStrokeContent{semantic::StrokeRecord{
        .brush = semantic::BrushDescriptor{.nominal_size = 10.0},
        .data = semantic::VectorStrokeData{{semantic::StrokeSample{{-2.0, 3.0}},
                                             semantic::StrokeSample{{8.0, 13.0}}}}}};
    const auto stroke = scene::computeBounds(record);
    assert(stroke.geometry.left <= -7.0F && stroke.geometry.right >= 13.0F);

    record.kind = semantic::ObjectKind::kDabStroke;
    semantic::StrokeRecord dabRecord;
    dabRecord.data = semantic::DabStrokeData{std::vector<semantic::DabInstance>{
        semantic::DabInstance{.center = {5.0, 6.0}, .size = 4.0}}};
    record.content = semantic::DabStrokeContent{dabRecord};
    const auto dab = scene::computeBounds(record);
    assert(dab.geometry.left <= 3.0F && dab.geometry.right >= 7.0F);

    record.kind = semantic::ObjectKind::kConnector;
    record.content = semantic::ConnectorContent{
        .start = semantic::ConnectorEndpoint{semantic::FreePointEndpoint{{-3.0, 4.0}}},
        .end = semantic::ConnectorEndpoint{semantic::FreePointEndpoint{{9.0, 14.0}}},
        .routing = semantic::ConnectorRouting::kOrthogonal};
    const auto connector = scene::computeBounds(record);
    assert(connector.geometry.left <= -3.0F && connector.geometry.right >= 9.0F);

    record.kind = semantic::ObjectKind::kGroup;
    record.content = semantic::GroupContent{};
    const auto group = scene::computeBounds(record);
    assert(group.visual.left == 0.0F && group.visual.right == 0.0F);
    return 0;
}
