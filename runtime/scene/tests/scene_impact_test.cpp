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
    assert(imageContent.relation == scene::DirtyState::kReuse);
    assert(imageContent.resource == scene::DirtyState::kDirty);

    semantic::ObjectRecord connectorRecord;
    connectorRecord.id = foundation::ObjectId::fromUint64(7);
    connectorRecord.kind = semantic::ObjectKind::kConnector;
    const auto connectorContent = scene::classifyImpact(
        semantic::ObjectSemanticChange{.object_id = connectorRecord.id,
                                       .flags = semantic::SemanticChangeFlags::kContent},
        &connectorRecord);
    assert(connectorContent.relation == scene::DirtyState::kDirty);

    const auto ordinaryContent = scene::classifyImpact(
        semantic::ObjectSemanticChange{.object_id = image.id,
                                       .flags = semantic::SemanticChangeFlags::kContent},
        static_cast<const semantic::ObjectRecord*>(nullptr));
    assert(ordinaryContent.relation == scene::DirtyState::kDirty);

    const auto emptyProperties = scene::classifyImpact(
        semantic::ObjectSemanticChange{.object_id = image.id,
                                       .flags = semantic::SemanticChangeFlags::kProperties});
    assert(emptyProperties.visual_bounds == scene::DirtyState::kDirty);
    return 0;
}
