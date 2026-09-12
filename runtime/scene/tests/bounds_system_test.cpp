#include "canvas/scene/bounds_system.hpp"
#include "canvas/foundation/object_id.hpp"

#include <cassert>
#include <cmath>
#include <limits>

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

    record.kind = semantic::ObjectKind::kImage;
    record.content = semantic::ImageContent{
        .resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(7)},
        .intrinsic_width = 40.0,
        .intrinsic_height = 30.0,
        .width = 40.0,
        .height = 30.0};
    const auto image = scene::computeBounds(record);
    assert(image.geometry.right == 40.0F && image.geometry.bottom == 30.0F);

    record.kind = semantic::ObjectKind::kSticky;
    record.content = semantic::StickyContent{25.0, 15.0};
    const auto sticky = scene::computeBounds(record);
    assert(sticky.geometry.right == 25.0F && sticky.geometry.bottom == 15.0F);

    record.kind = semantic::ObjectKind::kRichText;
    record.content = semantic::RichTextContent{semantic::RichTextDocument{
        {semantic::Paragraph{.style = semantic::ParagraphStyle{.line_height = 12.0},
                             .runs = {semantic::TextRun{.text = "abcd", .style = semantic::TextStyle{.font_size = 10.0}}}}}}};
    const auto text = scene::computeBounds(record);
    assert(text.geometry.right == 20.0F && text.geometry.bottom == 12.0F);

    record.kind = semantic::ObjectKind::kShape;
    record.content = semantic::ShapeContent{1, std::numeric_limits<double>::quiet_NaN(), 2.0};
    const auto nonfinite = scene::computeBounds(record);
    assert(!nonfinite.finite);
    assert(!nonfinite.world.isFiniteAndOrdered());

    record.content = semantic::ShapeContent{1, 0.0, 0.0};
    const auto degenerate = scene::computeBounds(record);
    assert(degenerate.finite);
    assert(degenerate.geometry == foundation::WorldRect{});

    record.kind = semantic::ObjectKind::kVectorPath;
    record.transform = semantic::Transform2D{1.0, 0.0, 0.0, 1.0, 10.0, -5.0};
    record.content = semantic::VectorPathContent{semantic::VectorPathGeometry{
        .commands = {semantic::MoveTo{{-2.0, 4.0}},
                     semantic::CubicTo{{8.0, -6.0}, {12.0, 18.0}, {20.0, 3.0}}}}};
    const auto semanticPath = scene::computeBounds(record);
    assert(semanticPath.finite);
    assert((semanticPath.geometry == foundation::WorldRect{-2.0F, -6.0F, 20.0F, 18.0F}));
    assert(semanticPath.visual == semanticPath.geometry);
    assert((semanticPath.world == foundation::WorldRect{8.0F, -11.0F, 30.0F, 13.0F}));
    return 0;
}
