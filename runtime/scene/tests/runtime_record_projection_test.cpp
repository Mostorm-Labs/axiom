#include "canvas/scene/scene_types.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/semantic_read_view.hpp"
#include "object_store_mutator.hpp"

#include <cassert>
#include <utility>
#include <vector>

using canvas::ObjectId;
using canvas::semantic::ObjectKind;
using canvas::semantic::ObjectRecord;
using canvas::semantic::SemanticGeneration;
using canvas::semantic::SemanticReadView;
using canvas::semantic::internal::ObjectStoreMutator;

namespace {
ObjectRecord record(ObjectKind kind, std::uint64_t id) {
    ObjectRecord value;
    value.id = ObjectId::fromUint64(id);
    value.kind = kind;
    value.kind_version = 1;
    value.placement.order_key = canvas::semantic::OrderKey(std::vector<std::uint8_t>{
        static_cast<std::uint8_t>(id)});
    value.transform.tx = static_cast<double>(id) + 0.25;
    value.transform.ty = static_cast<double>(id) + 0.5;
    value.properties.entries.push_back(canvas::semantic::PropertyEntry{
        .field_id = static_cast<std::uint32_t>(100 + id),
        .value = id % 2U == 0U,
    });
    canvas::semantic::SweptCircleMask mask;
    mask.segments.push_back(canvas::semantic::EraseCubicSegment{
        .p0 = canvas::semantic::EraseKnot{.position = {1.0, 2.0}, .radius = 3.0},
        .p1 = canvas::semantic::EraseKnot{.position = {4.0, 5.0}, .radius = 6.0},
        .control1 = {7.0, 8.0},
        .control2 = {9.0, 10.0},
    });
    value.erase_masks.push_back(canvas::semantic::EraseMaskRecord{
        .id = ObjectId::fromUint64(1000U + id),
        .geometry = std::move(mask),
    });
    switch (kind) {
    case ObjectKind::kShape:
        value.content = canvas::semantic::ShapeContent{11U, 12.5, 13.5};
        break;
    case ObjectKind::kImage:
        value.content = canvas::semantic::ImageContent{
            .resource_id = canvas::semantic::ResourceId{ObjectId::fromUint64(99)},
            .intrinsic_width = 20.0,
            .intrinsic_height = 30.0,
            .content_mode = canvas::semantic::ImageContentMode::kFit,
            .width = 40.0,
            .height = 50.0,
        };
        break;
    case ObjectKind::kVectorPath:
        value.content = canvas::semantic::VectorPathContent{
            .geometry = canvas::semantic::VectorPathGeometry{
                .fill_rule = canvas::semantic::FillRule::kEvenOdd,
                .commands = {canvas::semantic::MoveTo{.point = {1.0, 2.0}},
                             canvas::semantic::LineTo{.end = {3.0, 4.0}},
                             canvas::semantic::ClosePath{}},
            }};
        break;
    case ObjectKind::kRichText:
        value.content = canvas::semantic::RichTextContent{
            .document = canvas::semantic::RichTextDocument{
                .paragraphs = {canvas::semantic::Paragraph{
                    .id = ObjectId::fromUint64(2000U + id),
                    .style = canvas::semantic::ParagraphStyle{
                        .alignment = canvas::semantic::ParagraphAlignment::kCenter,
                        .line_height = 1.5,
                        .spacing_before = 2.0,
                        .spacing_after = 3.0,
                    },
                    .runs = {canvas::semantic::TextRun{
                        .text = "typed rich text",
                        .style = canvas::semantic::TextStyle{
                            .font_size = 18.0,
                            .weight = 700,
                            .italic = true,
                            .underline = true,
                        },
                    }},
                }},
            }};
        break;
    case ObjectKind::kVectorStroke:
        value.content = canvas::semantic::VectorStrokeContent{
            .stroke = canvas::semantic::StrokeRecord{
                .brush = canvas::semantic::BrushDescriptor{
                    .brush_family_id = 42,
                    .brush_version = 3,
                    .nominal_size = 5.5,
                    .opacity = 0.75F,
                    .blend_mode = canvas::semantic::BrushBlendMode::kHighlighter,
                },
                .deterministic_seed = 1234,
                .data = canvas::semantic::VectorStrokeData{
                    .samples = {canvas::semantic::StrokeSample{
                        .position = {6.0, 7.0}, .pressure = 0.8F, .tilt = {0.1, 0.2}}},
                },
            }};
        break;
    case ObjectKind::kDabStroke:
        value.content = canvas::semantic::DabStrokeContent{
            .stroke = canvas::semantic::StrokeRecord{
                .brush = canvas::semantic::BrushDescriptor{
                    .brush_family_id = 43,
                    .brush_version = 4,
                    .nominal_size = 6.5,
                    .opacity = 0.65F,
                },
                .deterministic_seed = 5678,
                .data = canvas::semantic::DabStrokeData{
                    .dabs = {canvas::semantic::DabInstance{
                        .center = {8.0, 9.0}, .size = 4.0, .rotation = 0.25F, .opacity = 0.9F}},
                },
            }};
        break;
    case ObjectKind::kConnector:
        value.content = canvas::semantic::ConnectorContent{
            .start = canvas::semantic::ConnectorEndpoint{
                .value = canvas::semantic::FreePointEndpoint{.point = {10.0, 11.0}}},
            .end = canvas::semantic::ConnectorEndpoint{
                .value = canvas::semantic::AttachedEndpoint{
                    .target_object_id = ObjectId::fromUint64(1),
                    .anchor = canvas::semantic::StablePortAnchor{.port_id = 9}}},
            .routing = canvas::semantic::ConnectorRouting::kOrthogonal,
        };
        break;
    case ObjectKind::kSticky:
        value.content = canvas::semantic::StickyContent{60.0, 70.0};
        break;
    case ObjectKind::kGroup:
        value.content = canvas::semantic::GroupContent{};
        break;
    }
    return value;
}
}

int main() {
    std::vector<ObjectRecord> source;
    for (std::uint8_t kind = 1; kind <= 9; ++kind) {
        source.push_back(record(static_cast<ObjectKind>(kind), kind));
    }
    const auto projection = canvas::projectRuntimeScene(source, SemanticGeneration(7));
    assert(projection.generation == SemanticGeneration(7));
    assert(projection.records.size() == 9);
    for (std::uint8_t kind = 1; kind <= 9; ++kind) {
        const auto* found = projection.find(ObjectId::fromUint64(kind));
        assert(found != nullptr);
        assert(found->kind == static_cast<ObjectKind>(kind));
        assert(found->kindVersion == 1);
        assert(found->transform.tx == static_cast<double>(kind) + 0.25);
        assert(found->properties.entries.size() == 1);
        assert(std::holds_alternative<bool>(found->properties.entries[0].value));
        assert(found->eraseMasks.size() == 1);
        const auto* mask = std::get_if<canvas::semantic::SweptCircleMask>(
            &found->eraseMasks[0].geometry);
        assert(mask != nullptr);
        assert(mask->segments.size() == 1);
    }
    const auto& shape = std::get<canvas::semantic::ShapeContent>(projection.records[0].content);
    assert(shape.shape_kind == 11U && shape.width == 12.5 && shape.height == 13.5);
    const auto& image = std::get<canvas::semantic::ImageContent>(projection.records[1].content);
    assert(image.resource_id.value == ObjectId::fromUint64(99));
    assert(image.intrinsic_width == 20.0 && image.intrinsic_height == 30.0);
    assert(image.content_mode == canvas::semantic::ImageContentMode::kFit);
    assert(image.width == 40.0 && image.height == 50.0);
    const auto& path =
        std::get<canvas::semantic::VectorPathContent>(projection.records[2].content);
    assert(path.geometry.fill_rule == canvas::semantic::FillRule::kEvenOdd);
    assert(path.geometry.commands.size() == 3);
    const auto& rich = std::get<canvas::semantic::RichTextContent>(projection.records[3].content);
    assert(rich.document.paragraphs.size() == 1);
    assert(rich.document.paragraphs[0].runs[0].text == "typed rich text");
    const auto& vector_stroke =
        std::get<canvas::semantic::VectorStrokeContent>(projection.records[4].content);
    assert(vector_stroke.stroke.brush.brush_family_id == 42);
    assert(std::get<canvas::semantic::VectorStrokeData>(vector_stroke.stroke.data).samples.size() ==
           1);
    const auto& dab_stroke =
        std::get<canvas::semantic::DabStrokeContent>(projection.records[5].content);
    assert(dab_stroke.stroke.brush.brush_family_id == 43);
    assert(std::get<canvas::semantic::DabStrokeData>(dab_stroke.stroke.data).dabs.size() == 1);
    const auto& connector =
        std::get<canvas::semantic::ConnectorContent>(projection.records[6].content);
    assert(connector.routing == canvas::semantic::ConnectorRouting::kOrthogonal);
    assert(std::get<canvas::semantic::FreePointEndpoint>(connector.start.value).point.x == 10.0);
    assert(std::get<canvas::semantic::AttachedEndpoint>(connector.end.value)
               .target_object_id == ObjectId::fromUint64(1));
    const auto& sticky = std::get<canvas::semantic::StickyContent>(projection.records[7].content);
    assert(sticky.width == 60.0 && sticky.height == 70.0);
    assert(std::holds_alternative<canvas::semantic::GroupContent>(projection.records[8].content));

    canvas::semantic::ReferenceObjectStore store;
    for (const auto& value : source) {
        assert(ObjectStoreMutator::insertFresh(store, value));
    }
    canvas::RuntimeScene scene;
    const auto applied = scene.replace(SemanticReadView(store, SemanticGeneration(7)));
    assert(applied.hasValue());
    assert(scene.generation() == SemanticGeneration(7));
    assert(scene.records().size() == 9);
    assert(scene.find(ObjectId::fromUint64(7))->kind == ObjectKind::kConnector);

    ObjectRecord changed = source[0];
    changed.transform.tx = 123.0;
    assert(ObjectStoreMutator::replaceExisting(store, changed));
    const auto updated = scene.apply(SemanticReadView(store, SemanticGeneration(8)));
    assert(updated.hasValue());
    assert(scene.generation() == SemanticGeneration(8));
    assert(scene.find(ObjectId::fromUint64(1))->transform.tx == 123.0);

    const auto stale = scene.apply(SemanticReadView(store, SemanticGeneration(8)));
    assert(!stale.hasValue());
    assert(stale.error().code == canvas::foundation::ErrorCode::kInvalidRevision);

    ObjectRecord page = record(static_cast<ObjectKind>(10), 10);
    assert(ObjectStoreMutator::insertFresh(store, page));
    const auto rejected = scene.replace(SemanticReadView(store, SemanticGeneration(8)));
    assert(!rejected.hasValue());
    assert(rejected.error().code == canvas::foundation::ErrorCode::kInvalidRecord);
    assert(scene.generation() == SemanticGeneration(8));
    return 0;
}
