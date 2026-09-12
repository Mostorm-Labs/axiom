#include "canvas/scene/scene_impact.hpp"
#include "canvas/foundation/object_id.hpp"

#include <cassert>
#include <array>
#include <string_view>

namespace {
using canvas::DirtyState;
using canvas::ImpactClassification;

bool exact(const ImpactClassification& actual, const ImpactClassification& expected) {
    return actual.record == expected.record && actual.hierarchy == expected.hierarchy &&
           actual.local_geometry == expected.local_geometry &&
           actual.visual_bounds == expected.visual_bounds &&
           actual.world_bounds == expected.world_bounds && actual.spatial == expected.spatial &&
           actual.relation == expected.relation && actual.resource == expected.resource &&
           actual.visibility == expected.visibility &&
           actual.ordinary_property == expected.ordinary_property;
}

ImpactClassification reuse() { return {}; }
ImpactClassification dirty(std::initializer_list<const char*> fields) {
    ImpactClassification out;
    for (const auto* field : fields) {
        if (std::string_view(field) == "record") out.record = DirtyState::kDirty;
        if (std::string_view(field) == "hierarchy") out.hierarchy = DirtyState::kDirty;
        if (std::string_view(field) == "local") out.local_geometry = DirtyState::kDirty;
        if (std::string_view(field) == "visual") out.visual_bounds = DirtyState::kDirty;
        if (std::string_view(field) == "world") out.world_bounds = DirtyState::kDirty;
        if (std::string_view(field) == "spatial") out.spatial = DirtyState::kDirty;
        if (std::string_view(field) == "relation") out.relation = DirtyState::kDirty;
        if (std::string_view(field) == "resource") out.resource = DirtyState::kDirty;
        if (std::string_view(field) == "visibility") out.visibility = DirtyState::kDirty;
        if (std::string_view(field) == "ordinary") out.ordinary_property = DirtyState::kDirty;
    }
    return out;
}
} // namespace

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

    for (const auto field : {1U, 0x200U, 0x201U}) {
        semantic::ObjectSemanticChange change{
            .object_id = foundation::ObjectId::fromUint64(3),
            .flags = semantic::SemanticChangeFlags::kProperties,
            .changed_fields = {field}};
        const auto classified = scene::classifyImpact(change);
        if (field == 1U) {
            assert(classified.visibility == scene::DirtyState::kDirty);
            assert(classified.visual_bounds == scene::DirtyState::kReuse);
        } else {
            assert(classified.visual_bounds == scene::DirtyState::kDirty);
            assert(classified.world_bounds == scene::DirtyState::kDirty);
        }
    }

    const auto strokeStyle = scene::classifyImpact(semantic::ObjectSemanticChange{
        .object_id = foundation::ObjectId::fromUint64(4),
        .flags = semantic::SemanticChangeFlags::kProperties,
        .changed_fields = {0x101U}});
    assert(strokeStyle.local_geometry == scene::DirtyState::kReuse);
    assert(strokeStyle.visual_bounds == scene::DirtyState::kDirty);
    assert(strokeStyle.spatial == scene::DirtyState::kDirty);

    const auto created = scene::classifyImpact(semantic::ObjectSemanticChange{
        .object_id = foundation::ObjectId::fromUint64(5),
        .flags = semantic::SemanticChangeFlags::kCreated});
    assert(created.record == scene::DirtyState::kDirty);
    assert(created.local_geometry == scene::DirtyState::kDirty);
    assert(created.visual_bounds == scene::DirtyState::kDirty);

    const auto relation = scene::classifyImpact(semantic::ObjectSemanticChange{
        .object_id = foundation::ObjectId::fromUint64(6),
        .flags = semantic::SemanticChangeFlags::kContent});
    assert(relation.relation == scene::DirtyState::kDirty);

    semantic::ObjectRecord image;
    image.id = foundation::ObjectId::fromUint64(6);
    image.kind = semantic::ObjectKind::kImage;
    image.content = semantic::ImageContent{
        .resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(99)}};
    const auto imageContent = scene::classifyImpact(
        semantic::ObjectSemanticChange{.object_id = image.id,
                                       .flags = semantic::SemanticChangeFlags::kContent},
        &image);
    assert(exact(imageContent, dirty({"local", "visual", "world", "spatial", "resource"})));

    semantic::ObjectRecord connectorRecord;
    connectorRecord.id = foundation::ObjectId::fromUint64(7);
    connectorRecord.kind = semantic::ObjectKind::kConnector;
    const auto connectorContent = scene::classifyImpact(
        semantic::ObjectSemanticChange{.object_id = connectorRecord.id,
                                       .flags = semantic::SemanticChangeFlags::kContent},
        &connectorRecord);
    assert(exact(connectorContent, dirty({"local", "visual", "world", "spatial", "relation"})));

    const auto ordinaryContent = scene::classifyImpact(
        semantic::ObjectSemanticChange{.object_id = image.id,
                                       .flags = semantic::SemanticChangeFlags::kContent},
        static_cast<const semantic::ObjectRecord*>(nullptr));
    assert(ordinaryContent.relation == scene::DirtyState::kDirty);

    const auto emptyProperties = scene::classifyImpact(
        semantic::ObjectSemanticChange{.object_id = image.id,
                                       .flags = semantic::SemanticChangeFlags::kProperties});
    assert(emptyProperties.visual_bounds == scene::DirtyState::kDirty);

    const auto id = foundation::ObjectId::fromUint64(20);
    struct Row {
        semantic::ObjectSemanticChange change;
        ImpactClassification expected;
    };
    const std::array<Row, 11> rows{
        Row{{id, semantic::SemanticChangeFlags::kTransform, {}}, dirty({"world", "spatial"})},
        Row{{id, semantic::SemanticChangeFlags::kPlacement, {}}, dirty({"hierarchy", "world", "spatial"})},
        Row{{id, semantic::SemanticChangeFlags::kContent, {}}, dirty({"local", "visual", "world", "spatial", "relation"})},
        Row{{id, semantic::SemanticChangeFlags::kProperties, {0x101U}}, dirty({"visual", "world", "spatial"})},
        Row{{id, semantic::SemanticChangeFlags::kProperties, {1U}}, dirty({"visibility"})},
        Row{{id, semantic::SemanticChangeFlags::kProperties, {3U}}, dirty({"ordinary"})},
        Row{{id, semantic::SemanticChangeFlags::kProperties, {0x200U}}, dirty({"visual", "world", "spatial"})},
        Row{{id, semantic::SemanticChangeFlags::kCreated, {}}, dirty({"record", "hierarchy", "local", "visual", "world", "spatial"})},
        Row{{id, semantic::SemanticChangeFlags::kDeleted, {}}, dirty({"record", "hierarchy", "local", "visual", "world", "spatial"})},
        Row{{id, semantic::SemanticChangeFlags::kEraseMasks, {}}, dirty({"visual", "world", "spatial"})},
        Row{{id, semantic::SemanticChangeFlags::kProperties, {}}, dirty({"visual", "world", "spatial"})},
    };
    for (const auto& row : rows) assert(exact(scene::classifyImpact(row.change), row.expected));

    semantic::ObjectRecord vectorRecord;
    vectorRecord.id = id;
    vectorRecord.kind = semantic::ObjectKind::kVectorStroke;
    vectorRecord.content = semantic::VectorStrokeContent{semantic::StrokeRecord{
        .brush = semantic::BrushDescriptor{.texture_resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(900)}}}};
    const auto resourceImpact = scene::classifyImpact(
        semantic::ObjectSemanticChange{id, semantic::SemanticChangeFlags::kContent, {}},
        &vectorRecord);
    assert(exact(resourceImpact, dirty({"local", "visual", "world", "spatial", "resource"})));
    return 0;
}
