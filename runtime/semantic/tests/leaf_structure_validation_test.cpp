#include "canvas/semantic/validator.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
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
    stroke.brush.nominal_size = 1.0;
    stroke.brush.opacity = 1.0F;
    stroke.brush.blend_mode = family == 2U ? BrushBlendMode::kHighlighter : BrushBlendMode::kNormal;
    stroke.data = VectorStrokeData{{StrokeSample{{1.0, 2.0}, 0.5F, {0.0, 0.0}}}};
    return stroke;
}

StrokeRecord dabStroke(std::uint32_t family = 3U, std::uint32_t version = 1U) {
    StrokeRecord stroke;
    stroke.brush.brush_family_id = family;
    stroke.brush.brush_version = version;
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
    stroke.brush.pressure.size_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{0.5F, 0.75F}}};
    stroke.brush.pressure.opacity_curve = PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{0.5F, 0.75F}}};
    EXPECT_TRUE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    stroke.brush.tilt.size_influence = 0.25F;
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());

    stroke = vectorStroke();
    std::get<VectorStrokeData>(stroke.data).samples.front().tilt = Vec2{1.0, 0.0};
    EXPECT_FALSE(validatePayloadStructure(strokeOperation(ObjectKind::kVectorStroke, stroke)).ok());
}

} // namespace
} // namespace canvas::semantic
