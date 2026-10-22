#include "canvas/scene/scene_types.hpp"
#include "canvas/text/text_layout.hpp"

#include <cassert>
#include <vector>

namespace {
canvas::semantic::ObjectRecord makeShapeLabel() {
    canvas::semantic::ObjectRecord shape{};
    shape.id = canvas::semantic::ObjectId::fromUint64(10);
    shape.kind = canvas::semantic::ObjectKind::kShape;
    shape.kind_version = 3;
    shape.placement.order_key = canvas::semantic::OrderKey({1});
    canvas::semantic::TextStyle style;
    style.font_resource_id = canvas::semantic::ResourceId{canvas::semantic::ObjectId::fromUint64(77)};
    style.font_size = 16;
    style.color = {0, 0, 0, 1};
    canvas::semantic::Paragraph paragraph;
    paragraph.id = canvas::semantic::ObjectId::fromUint64(11);
    paragraph.style = {canvas::semantic::ParagraphAlignment::kLeft, 1, 0, 0, false};
    paragraph.runs = {{"label", style}};
    canvas::semantic::RichTextContent label;
    label.document.paragraphs = {paragraph};
    label.layout = canvas::semantic::RichTextLayout{120};
    shape.content = canvas::semantic::ShapeContent{1, 100, 40, label};
    return shape;
}
}

int main() {
    auto shape = makeShapeLabel();
    auto& label = *std::get<canvas::semantic::ShapeContent>(shape.content).label;

    canvas::runtime::MemoryResourceProvider resources;
    canvas::text::RichTextLayoutService textService(resources, {120.0F, 1U});
    const std::vector<canvas::semantic::ObjectRecord> firstSource{shape};
    const auto first = canvas::projectRuntimeScene(firstSource, canvas::semantic::SemanticGeneration(1), &textService);
    assert(first.records.size() == 1);
    assert(first.records[0].objectId == shape.id);
    const auto firstBounds = first.records[0].worldBounds;
    assert(firstBounds.left == 0 && firstBounds.top == 0 && firstBounds.right == 100 && firstBounds.bottom == 40);
    assert(first.records[0].directDependencies.size() == 1);
    assert(first.records[0].directDependencies[0] == canvas::semantic::ObjectId::fromUint64(77));

    auto changed = shape;
    std::get<canvas::semantic::ShapeContent>(changed.content).label->document.paragraphs[0].runs[0].text = "after";
    const auto second = canvas::projectRuntimeScene(
        std::vector<canvas::semantic::ObjectRecord>{changed}, canvas::semantic::SemanticGeneration(2), &textService);
    assert(second.records.size() == 1);
    assert(second.records[0].objectId == shape.id);
    assert(second.records[0].worldBounds == first.records[0].worldBounds);
    assert(second.records[0].referenceGeometryDigest != first.records[0].referenceGeometryDigest);

    const auto base = canvas::projectRuntimeScene(std::vector<canvas::semantic::ObjectRecord>{shape},
                                                   canvas::semantic::SemanticGeneration(1));
    assert(base.records.size() == 1);
    const auto digest = base.records[0].referenceGeometryDigest;
    auto expectDigestChange = [&](auto mutate) {
        auto changed = shape;
        mutate(*std::get<canvas::semantic::ShapeContent>(changed.content).label);
        const auto projected = canvas::projectRuntimeScene(
            std::vector<canvas::semantic::ObjectRecord>{changed},
            canvas::semantic::SemanticGeneration(1));
        assert(projected.records[0].referenceGeometryDigest != digest);
    };
    expectDigestChange([](auto& value) { value.layout->wrap_width = 140; });
    expectDigestChange([](auto& value) { value.document.paragraphs[0].style.alignment = canvas::semantic::ParagraphAlignment::kCenter; });
    expectDigestChange([](auto& value) { value.document.paragraphs[0].style.line_height = 1.5; });
    expectDigestChange([](auto& value) { value.document.paragraphs[0].style.bulleted = true; });
    expectDigestChange([](auto& value) { value.document.paragraphs[0].runs[0].style.font_size = 20; });
    expectDigestChange([](auto& value) { value.document.paragraphs[0].runs[0].style.color.r = 1; });
    expectDigestChange([](auto& value) { value.document.paragraphs[0].runs[0].style.font_resource_id = canvas::semantic::ResourceId{canvas::semantic::ObjectId::fromUint64(78)}; });
    return 0;
}
