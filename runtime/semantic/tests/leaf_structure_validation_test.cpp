#include "canvas/semantic/validator.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

Operation pathOperation(std::vector<PathCommand> commands,
                        FillRule fill_rule = FillRule::kNonZero) {
    Operation operation;
    operation.payload = SetVectorPathGeometryOp{
        id(1U), VectorPathGeometry{fill_rule, std::move(commands)}};
    return operation;
}

TEST(LeafStructureValidation, EnforcesVectorPathCommandGrammar) {
    EXPECT_FALSE(validatePayloadStructure(pathOperation({})).ok());
    EXPECT_FALSE(validatePayloadStructure(pathOperation({LineTo{{1.0, 2.0}}})).ok());
    EXPECT_FALSE(validatePayloadStructure(pathOperation({ClosePath{}})).ok());
    EXPECT_FALSE(validatePayloadStructure(pathOperation({MoveTo{{0.0, 0.0}}, ClosePath{}, LineTo{{1.0, 1.0}}})).ok());
    EXPECT_FALSE(validatePayloadStructure(pathOperation({MoveTo{{0.0, 0.0}}, LineTo{{1.0, 1.0}}, ClosePath{}, LineTo{{2.0, 2.0}}})).ok());
    EXPECT_FALSE(validatePayloadStructure(pathOperation({MoveTo{{0.0, 0.0}}, ClosePath{}, ClosePath{}})).ok());

    EXPECT_TRUE(validatePayloadStructure(pathOperation({MoveTo{{0.0, 0.0}}})).ok());
    EXPECT_TRUE(validatePayloadStructure(pathOperation({
        MoveTo{{0.0, 0.0}}, LineTo{{1.0, 1.0}}, ClosePath{},
        MoveTo{{2.0, 2.0}}, QuadTo{{3.0, 3.0}, {4.0, 4.0}},
        CubicTo{{5.0, 5.0}, {6.0, 6.0}, {7.0, 7.0}}, ClosePath{}})).ok());
    EXPECT_TRUE(validatePayloadStructure(pathOperation({MoveTo{{0.0, 0.0}}}, FillRule::kEvenOdd)).ok());
    EXPECT_FALSE(validatePayloadStructure(pathOperation({MoveTo{{0.0, 0.0}}}, static_cast<FillRule>(0U))).ok());
    EXPECT_FALSE(validatePayloadStructure(pathOperation({MoveTo{{std::numeric_limits<double>::infinity(), 0.0}}})).ok());
}

Operation richTextOperation(std::uint32_t version, std::vector<RichTextStep> steps) {
    Operation operation;
    operation.payload = EditRichTextOp{id(2U), RichTextDelta{version, std::move(steps)}};
    return operation;
}

TEST(LeafStructureValidation, EnforcesRichTextDeltaVersionAndOrderedSteps) {
    const RichTextStep delete_step = DeleteTextStep{id(3U), 0U, 1U};
    EXPECT_TRUE(validatePayloadStructure(richTextOperation(1U, {delete_step})).ok());
    EXPECT_FALSE(validatePayloadStructure(richTextOperation(0U, {delete_step})).ok());
    EXPECT_FALSE(validatePayloadStructure(richTextOperation(2U, {delete_step})).ok());
    EXPECT_FALSE(validatePayloadStructure(richTextOperation(1U, {})).ok());
    EXPECT_FALSE(validatePayloadStructure(richTextOperation(1U, {DeleteTextStep{ObjectId{}, 0U, 1U}})).ok());
    EXPECT_TRUE(validatePayloadStructure(richTextOperation(1U, {
        DeleteTextStep{id(3U), 9U, 1U}, DeleteTextStep{id(3U), 2U, 3U}})).ok());
}

StrokeRecord vectorStroke(std::uint32_t family = 1U, std::uint32_t version = 1U) {
    StrokeRecord stroke;
    stroke.brush.brush_family_id = family;
    stroke.brush.brush_version = version;
    stroke.brush.color = ColorValue{0.0F, 0.0F, 0.0F, 1.0F};
    stroke.brush.nominal_size = 1.0;
    stroke.brush.opacity = 1.0F;
    stroke.brush.blend_mode = family == 2U ? BrushBlendMode::kHighlighter : BrushBlendMode::kNormal;
    stroke.data = VectorStrokeData{{StrokeSample{{1.0, 2.0}, 1.0F, {0.0, 0.0}}}};
    return stroke;
}

StrokeRecord dabStroke(std::uint32_t family = 3U, std::uint32_t version = 1U) {
    StrokeRecord stroke;
    stroke.brush.brush_family_id = family;
    stroke.brush.brush_version = version;
    stroke.brush.color = ColorValue{0.0F, 0.0F, 0.0F, 1.0F};
    stroke.brush.nominal_size = 1.0;
    stroke.brush.opacity = 1.0F;
    stroke.brush.texture_resource_id = ResourceId{id(9U)};
    stroke.data = DabStrokeData{{DabInstance{{1.0, 2.0}, 2.0, 0.0F, 1.0F}}};
    return stroke;
}

Operation strokeOperation(ObjectKind kind, StrokeRecord stroke) {
    ObjectRecord object;
    object.id = id(10U);
    object.kind = kind;
    object.kind_version = 1U;
    object.placement.order_key = OrderKey({1U});
    if (kind == ObjectKind::kVectorStroke) {
        object.content = VectorStrokeContent{std::move(stroke)};
    } else {
        object.content = DabStrokeContent{std::move(stroke)};
    }
    Operation operation;
    operation.payload = AddStrokeOp{std::move(object)};
    return operation;
}

TEST(LeafStructureValidation, EnforcesStrokeCardinalityRepresentationAndDabDomain) {
    EXPECT_TRUE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, vectorStroke())).ok());
    EXPECT_TRUE(validatePayloadStructure(strokeOperation(ObjectKind::kDabStroke, dabStroke())).ok());

    auto empty_vector = vectorStroke();
    empty_vector.data = VectorStrokeData{};
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, std::move(empty_vector))).ok());
    auto empty_dab = dabStroke();
    empty_dab.data = DabStrokeData{};
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kDabStroke, std::move(empty_dab))).ok());

    auto bad_dab = dabStroke();
    std::get<DabStrokeData>(bad_dab.data).dabs.front().size = 0.0;
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kDabStroke, std::move(bad_dab))).ok());
    bad_dab = dabStroke();
    std::get<DabStrokeData>(bad_dab.data).dabs.front().opacity = 1.1F;
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kDabStroke, std::move(bad_dab))).ok());
    bad_dab = dabStroke();
    std::get<DabStrokeData>(bad_dab.data).dabs.front().rotation =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kDabStroke, std::move(bad_dab))).ok());

    EXPECT_FALSE(validatePayloadStructure(strokeOperation(
        ObjectKind::kVectorStroke, dabStroke())).ok());
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(
        ObjectKind::kDabStroke, vectorStroke())).ok());
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(
        ObjectKind::kVectorStroke, vectorStroke(1U, 2U))).ok());
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(
        ObjectKind::kDabStroke, dabStroke(3U, 2U))).ok());
}

TEST(LeafStructureValidation, EnforcesReleasedPressureAndTiltDomain) {
    auto stroke = vectorStroke();
    stroke.brush.pressure.size_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{1.0F, 1.0F}}};
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    stroke.brush.pressure.enabled = true;
    stroke.brush.pressure.size_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}}};
    stroke.brush.pressure.opacity_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{1.0F, 1.0F}}};
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    stroke.brush.pressure.enabled = true;
    stroke.brush.pressure.size_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{1.0F, 0.75F}}};
    stroke.brush.pressure.opacity_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{1.0F, 0.75F}}};
    EXPECT_TRUE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    stroke.brush.pressure.enabled = true;
    stroke.brush.pressure.size_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{1.0F, 0.75F}}};
    stroke.brush.pressure.opacity_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{1.0F, 0.75F}}};
    std::get<VectorStrokeData>(stroke.data).samples.front().pressure = 0.5F;
    EXPECT_TRUE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    stroke.brush.pressure.enabled = true;
    stroke.brush.pressure.size_curve = PiecewiseLinearCurve01{{CurvePoint01{0.1F, 0.0F}, CurvePoint01{1.0F, 0.75F}}};
    stroke.brush.pressure.opacity_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{1.0F, 0.75F}}};
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    std::get<VectorStrokeData>(stroke.data).samples.front().pressure = 0.5F;
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    stroke.brush.tilt.size_influence = 0.25F;
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    std::get<VectorStrokeData>(stroke.data).samples.front().tilt = Vec2{1.0, 0.0};
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());
}

TEST(LeafStructureValidation, EnforcesImageContentGeometryAndMode) {
    auto image = ImageContent{ResourceId{id(9U)}, 640.0, 480.0, std::nullopt,
                              ImageContentMode::kFit, 320.0, 240.0};
    Operation operation;
    operation.payload = SetImageContentOp{id(2U), image};
    EXPECT_TRUE(validatePayloadStructure(operation).ok());

    image.resource_id = ResourceId{};
    operation.payload = SetImageContentOp{id(2U), image};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    image = ImageContent{ResourceId{id(9U)}, 0.0, 480.0, std::nullopt,
                         ImageContentMode::kFit, 320.0, 240.0};
    operation.payload = SetImageContentOp{id(2U), image};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    image = ImageContent{ResourceId{id(9U)}, 640.0, 480.0,
                         NormalizedRect{0.0, 0.0, 0.0, 1.0},
                         ImageContentMode::kFit, 320.0, 240.0};
    operation.payload = SetImageContentOp{id(2U), image};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    image.content_mode = static_cast<ImageContentMode>(0U);
    operation.payload = SetImageContentOp{id(2U), image};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
}

TEST(LeafStructureValidation, EnforcesConnectorAnchorAndRoutingDomain) {
    Operation operation;
    operation.payload = SetConnectorContentOp{
        id(4U), ConnectorContent{
            ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}},
            ConnectorEndpoint{AttachedEndpoint{id(8U), AutoPerimeterAnchor{Vec2{0.2, 0.8}}}},
            ConnectorRouting::kOrthogonal}};
    EXPECT_TRUE(validatePayloadStructure(operation).ok());

    auto& content = std::get<SetConnectorContentOp>(operation.payload).content;
    content.start = ConnectorEndpoint{FreePointEndpoint{{std::numeric_limits<double>::infinity(), 0.0}}};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    content.start = ConnectorEndpoint{AttachedEndpoint{id(8U), AutoPerimeterAnchor{Vec2{0.5, 0.5}}}};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    content.start = ConnectorEndpoint{AttachedEndpoint{id(8U), StablePortAnchor{0U}}};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    content.start = ConnectorEndpoint{AttachedEndpoint{id(8U), StablePortAnchor{5U}}};
    EXPECT_TRUE(validatePayloadStructure(operation).ok());
    content.start = ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}};
    content.routing = static_cast<ConnectorRouting>(0U);
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
}

TEST(LeafStructureValidation, EnforcesRichTextStyleAndUtf8Domain) {
    Operation operation;
    TextStyle style{ResourceId{id(7U)}, 14.0, 400U, false, false, ColorValue{0.0F, 0.0F, 0.0F, 1.0F}};
    operation.payload = EditRichTextOp{id(2U), RichTextDelta{1U, {
        InsertTextStep{id(3U), 0U, "hello", style}}}};
    EXPECT_TRUE(validatePayloadStructure(operation).ok());
    style.font_resource_id.reset();
    std::get<EditRichTextOp>(operation.payload).delta.steps.front() =
        InsertTextStep{id(3U), 0U, "hello", style};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    style.font_resource_id = ResourceId{id(7U)};
    style.weight = 450U;
    std::get<EditRichTextOp>(operation.payload).delta.steps.front() =
        InsertTextStep{id(3U), 0U, "hello", style};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    style.weight = 400U;
    std::get<EditRichTextOp>(operation.payload).delta.steps.front() =
        InsertTextStep{id(3U), 0U, std::string("bad\xff", 4), style};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
}

TEST(LeafStructureValidation, EnforcesRichTextInsertedUtf8AggregateLimit) {
    const TextStyle style{ResourceId{id(7U)}, 14.0, 400U, false, false,
                          ColorValue{0.0F, 0.0F, 0.0F, 1.0F}};
    std::vector<RichTextStep> steps;
    steps.reserve(9U);
    for (std::uint64_t paragraph = 1U; paragraph <= 9U; ++paragraph) {
        steps.emplace_back(InsertTextStep{id(paragraph), 0U, std::string(1024U * 1024U, 'a'), style});
    }
    EXPECT_FALSE(validatePayloadStructure(richTextOperation(1U, std::move(steps))).ok());
}

TEST(LeafStructureValidation, RichTextDocumentTotalTextMayExceedEditInsertAggregateLimit) {
    const TextStyle style{ResourceId{id(7U)}, 14.0, 400U, false, false,
                          ColorValue{0.0F, 0.0F, 0.0F, 1.0F}};
    RichTextDocument document;
    document.paragraphs.reserve(9U);
    for (std::uint64_t paragraph_id = 1U; paragraph_id <= 9U; ++paragraph_id) {
        document.paragraphs.push_back(Paragraph{
            id(paragraph_id), ParagraphStyle{ParagraphAlignment::kLeft, 1.0, 0.0, 0.0},
            {TextRun{std::string(1024U * 1024U, 'a'), style}}});
    }
    ObjectRecord object;
    object.id = id(10U);
    object.kind = ObjectKind::kRichText;
    object.kind_version = 1U;
    object.placement.order_key = OrderKey({1U});
    object.content = RichTextContent{std::move(document)};
    Operation operation;
    operation.payload = InsertObjectsOp{{std::move(object)}};
    EXPECT_TRUE(validatePayloadStructure(operation).ok());
}

TEST(LeafStructureValidation, EnforcesShapeKindAndPositiveSize) {
    Operation operation;
    ObjectRecord object;
    object.id = id(10U);
    object.kind = ObjectKind::kShape;
    object.kind_version = 1U;
    object.placement.order_key = OrderKey({1U});
    object.content = ShapeContent{1U, 10.0, 20.0};
    operation.payload = AddStrokeOp{object};
    EXPECT_TRUE(validatePayloadStructure(operation).ok());
    object = std::get<AddStrokeOp>(operation.payload).object;
    object.content = ShapeContent{3U, 10.0, 20.0};
    operation.payload = AddStrokeOp{object};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
    object.content = ShapeContent{1U, 0.0, 20.0};
    operation.payload = AddStrokeOp{object};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
}

TEST(LeafStructureValidation, EnforcesEraseMaskGeometryAndProtocolLimits) {
    Operation operation;
    EraseCubicSegment segment{
        EraseKnot{Vec2{0.0, 0.0}, 1.0}, EraseKnot{Vec2{1.0, 1.0}, 1.0},
        Vec2{0.5, 0.5}, Vec2{0.5, 0.5}};
    operation.payload = AddEraseMasksOp{{EraseMaskAddItem{
        id(1U), {EraseMaskRecord{id(2U), SweptCircleMask{{segment}}}}}}};
    EXPECT_TRUE(validatePayloadStructure(operation).ok());
    auto& mask_value = std::get<AddEraseMasksOp>(operation.payload).items.front().masks.front();
    std::get<SweptCircleMask>(mask_value.geometry).segments.front().p0.radius = 0.0;
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
}

TEST(LeafStructureValidation, EnforcesOperationGeometryAggregateLimit) {
    auto path = [](double offset) {
        std::vector<PathCommand> commands;
        commands.reserve(1000001U);
        commands.emplace_back(MoveTo{{offset, 0.0}});
        for (std::size_t index = 1U; index < 1000001U; ++index) {
            commands.emplace_back(LineTo{{offset + static_cast<double>(index), 0.0}});
        }
        return VectorPathContent{VectorPathGeometry{FillRule::kNonZero, std::move(commands)}};
    };

    ObjectRecord first;
    first.id = id(10U);
    first.kind = ObjectKind::kVectorPath;
    first.kind_version = 1U;
    first.placement.order_key = OrderKey({1U});
    first.content = path(0.0);

    ObjectRecord second = first;
    second.id = id(20U);
    second.placement.order_key = OrderKey({2U});
    second.content = path(2000000.0);

    Operation operation;
    operation.payload = InsertObjectsOp{{std::move(first), std::move(second)}};
    EXPECT_FALSE(validatePayloadStructure(operation).ok());
}

} // namespace
} // namespace canvas::semantic
