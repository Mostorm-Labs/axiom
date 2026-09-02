#include "canvas/semantic/canonical_numeric.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/canonical_commit_stamp.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/erase_mask.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/property_value.hpp"
#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/document_id.hpp"
#include "canvas/semantic/operation_payload.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>
#include <variant>

namespace canvas::semantic {

namespace {

struct ServerRevisionBoundaryType final {
    std::uint64_t value = 0;
};

struct ServerCursorBoundaryType final {
    std::uint64_t value = 0;
};

} // namespace

TEST(SemanticTypes, ObjectIdIsOpaqueSixteenBytesAndZeroIsInvalid) {
    static_assert(sizeof(ObjectId) == 16);
    ObjectId id = ObjectId::fromUint64(42);
    EXPECT_FALSE(id.isZero());
    EXPECT_TRUE(ObjectId{}.isZero());
}

TEST(SemanticTypes, OrderKeyAcceptsOnlyAuthorityBounds) {
    EXPECT_FALSE(OrderKey(std::vector<std::uint8_t>{}).isValid());
    EXPECT_TRUE(OrderKey(std::vector<std::uint8_t>{1U}).isValid());
    auto thirty_one = std::vector<std::uint8_t>(31, 0U);
    thirty_one.back() = 1U;
    EXPECT_TRUE(OrderKey(std::move(thirty_one)).isValid());
    EXPECT_TRUE(OrderKey(std::vector<std::uint8_t>(32, 0x01U)).isValid());
    EXPECT_FALSE(OrderKey(std::vector<std::uint8_t>(33, 0)).isValid());
    EXPECT_FALSE(OrderKey(std::vector<std::uint8_t>{1U, 0U}).isValid());
}

TEST(SemanticTypes, OrderKeyUsesUnsignedLexicographicBytes) {
    const OrderKey low({0x01});
    const OrderKey high({0x80});
    EXPECT_LT(low, high);
}

TEST(SemanticTypes, NumericNormalizationRejectsNonFiniteAndCanonicalizesNegativeZero) {
    double output = -1.0;
    EXPECT_TRUE(normalizeFinite(-0.0, output));
    EXPECT_EQ(output, 0.0);
    EXPECT_FALSE(std::signbit(output));
    EXPECT_FALSE(normalizeFinite(std::numeric_limits<double>::quiet_NaN(), output));
    EXPECT_FALSE(normalizeFinite(std::numeric_limits<double>::infinity(), output));
}

TEST(SemanticTypes, RegistryIsExplicitAndClosed) {
    for (std::uint8_t value = 1; value <= 9; ++value) {
        EXPECT_TRUE(isKnownObjectKind(static_cast<ObjectKind>(value)));
    }
    EXPECT_FALSE(isKnownObjectKind(static_cast<ObjectKind>(0)));
    EXPECT_FALSE(isKnownObjectKind(static_cast<ObjectKind>(10)));

    for (std::uint8_t value = 1; value <= 15; ++value) {
        EXPECT_TRUE(isKnownOperationKind(static_cast<OperationKind>(value)));
    }
    EXPECT_FALSE(isKnownOperationKind(static_cast<OperationKind>(0)));
    EXPECT_FALSE(isKnownOperationKind(static_cast<OperationKind>(16)));
}

TEST(SemanticTypes, SemanticGenerationIsASeparateStrongRuntimeLocalToken) {
    static_assert(!std::is_convertible_v<OperationId, ObjectId>);
    static_assert(!std::is_convertible_v<ObjectId, OperationId>);
    static_assert(!std::is_convertible_v<OperationId, SemanticGeneration>);
    static_assert(!std::is_convertible_v<SemanticGeneration, OperationId>);
    static_assert(!std::is_convertible_v<SemanticGeneration, std::uint64_t>);
    static_assert(!std::is_convertible_v<SemanticGeneration, CanonicalCommitStamp>);
    static_assert(!std::is_convertible_v<CommitOrdinal, std::uint64_t>);
    static_assert(!std::is_convertible_v<std::uint64_t, CommitOrdinal>);
    static_assert(!std::is_convertible_v<CommitOrdinal, SemanticGeneration>);
    static_assert(!std::is_convertible_v<SemanticGeneration, CommitOrdinal>);
    static_assert(!std::is_convertible_v<SemanticGeneration, ServerRevisionBoundaryType>);
    static_assert(!std::is_convertible_v<SemanticGeneration, ServerCursorBoundaryType>);

    const SemanticGeneration baseline(0U);
    const SemanticGeneration successor(1U);
    EXPECT_LT(baseline, successor);
    EXPECT_EQ(successor.value(), 1U);
}

TEST(SemanticTypes, CanonicalCommitOrdinalStartsWithReservedZero) {
    const CanonicalCommitClock clock(RuntimeEpoch(9U));
    EXPECT_EQ(clock.runtimeEpoch(), RuntimeEpoch(9U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal{});
}

TEST(SemanticTypes, OperationIdZeroPredicateSupportsConstantEvaluation) {
    constexpr OperationId zero{};
    static_assert(zero.isZero());

    EXPECT_TRUE(zero.isZero());
    EXPECT_FALSE(OperationId(ObjectId::fromUint64(42U)).isZero());
}

TEST(SemanticTypes, OperationCarriesClosedTypedPayloadAndStrongDocumentId) {
    static_assert(!std::is_convertible_v<DocumentId, ObjectId>);
    static_assert(!std::is_convertible_v<ObjectId, DocumentId>);
    static_assert(std::variant_size_v<OperationPayload> == 15);

    Operation operation{};
    operation.payload = DeleteObjectsOp{{ObjectId::fromUint64(7U)}};
    EXPECT_EQ(operation.kind(), OperationKind::kDeleteObjects);
}

TEST(SemanticTypes, ImageContentModeUsesReleasedWireIdentities) {
    EXPECT_EQ(static_cast<unsigned>(ImageContentMode::kFit), 1U);
    EXPECT_EQ(static_cast<unsigned>(ImageContentMode::kFill), 2U);
    EXPECT_EQ(static_cast<unsigned>(ImageContentMode::kStretch), 3U);
}

TEST(SemanticTypes, AutoPerimeterHintPresenceIsSemantic) {
    const AutoPerimeterAnchor absent{};
    const AutoPerimeterAnchor present{Vec2{0.0, 0.0}};
    EXPECT_NE(absent, present);
}

TEST(SemanticTypes, ChangeSetMergesObjectChangesInDeterministicOrder) {
    const ObjectId first = ObjectId::fromUint64(1U);
    const ObjectId second = ObjectId::fromUint64(2U);
    const ChangeSet changes = ChangeSet::fromChanges(
        SemanticGeneration(7U), SemanticGeneration(8U),
        {
            {second, SemanticChangeFlags::kTransform, {30U, 10U, 10U}},
            {first, SemanticChangeFlags::kProperties, {9U, 3U, 9U}},
            {second, SemanticChangeFlags::kProperties, {10U, 5U}},
        });

    EXPECT_EQ(changes.beforeGeneration(), SemanticGeneration(7U));
    EXPECT_EQ(changes.afterGeneration(), SemanticGeneration(8U));
    ASSERT_EQ(changes.objects().size(), 2U);
    EXPECT_EQ(changes.objects()[0].object_id, first);
    EXPECT_EQ(changes.objects()[1].object_id, second);
    EXPECT_EQ(changes.objects()[1].flags,
              SemanticChangeFlags::kTransform | SemanticChangeFlags::kProperties);
    EXPECT_EQ(changes.objects()[1].changed_fields,
              (std::vector<FieldId>{5U, 10U, 30U}));
}

TEST(SemanticTypes, ChangeSetExpressesCreatedAndDeletedObjects) {
    const ObjectId created = ObjectId::fromUint64(1U);
    const ObjectId deleted = ObjectId::fromUint64(2U);
    const ChangeSet changes = ChangeSet::fromChanges(
        SemanticGeneration(4U), SemanticGeneration(5U),
        {
            {deleted, SemanticChangeFlags::kDeleted, {}},
            {created, SemanticChangeFlags::kCreated, {}},
        });

    ASSERT_EQ(changes.objects().size(), 2U);
    EXPECT_EQ(changes.objects()[0].flags, SemanticChangeFlags::kCreated);
    EXPECT_EQ(changes.objects()[1].flags, SemanticChangeFlags::kDeleted);
}

TEST(SemanticTypes, PublicTypesDoNotRequireSceneOrRenderer) {
    ObjectRecord record{};
    record.id = ObjectId::fromUint64(1U);
    record.kind = ObjectKind::kShape;
    record.placement.order_key = OrderKey({1U});
    const ChangeSet changes = ChangeSet::fromChanges(
        SemanticGeneration(0U), SemanticGeneration(1U),
        {{record.id, SemanticChangeFlags::kCreated, {}}});
    EXPECT_EQ(changes.objects().size(), 1U);
}

TEST(SemanticTypes, ObjectRecordCarriesEveryFrozenCanonicalField) {
    ObjectRecord record{};
    record.id = ObjectId::fromUint64(1U);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{ObjectId::fromUint64(9U), OrderKey({0x10U})};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 12.0, 24.0};
    record.properties = PropertyBag{{PropertyEntry{7U, ColorValue{0.25F, 0.5F, 0.75F, 1.0F}}}};
    record.content = ShapeContent{7U, 48.0, 24.0};
    record.erase_masks = {EraseMaskRecord{
        ObjectId::fromUint64(2U),
        SweptCircleMask{{EraseCubicSegment{
            EraseKnot{Vec2{1.0, 2.0}, 3.0},
            EraseKnot{Vec2{4.0, 5.0}, 6.0},
            Vec2{2.0, 3.0},
            Vec2{3.0, 4.0},
        }}}}};

    EXPECT_EQ(record.kind_version, 1U);
    ASSERT_TRUE(record.placement.parent_id.has_value());
    EXPECT_EQ(*record.placement.parent_id, ObjectId::fromUint64(9U));
    EXPECT_EQ(record.transform.tx, 12.0);
    ASSERT_EQ(record.properties.entries.size(), 1U);
    EXPECT_EQ(record.properties.entries.front().field_id, 7U);
    EXPECT_TRUE(std::holds_alternative<ShapeContent>(record.content));
    ASSERT_EQ(record.erase_masks.size(), 1U);
    EXPECT_EQ(record.erase_masks.front().id, ObjectId::fromUint64(2U));
}

TEST(SemanticTypes, PropertyValueIsAClosedTypedUnion) {
    const PropertyValue boolean = true;
    const PropertyValue scalar = 1.25F;
    const PropertyValue color = ColorValue{0.1F, 0.2F, 0.3F, 1.0F};
    const PropertyValue fill = FillStyleValue{SolidFill{ColorValue{1.0F, 0.0F, 0.0F, 1.0F}}};
    const PropertyValue stroke = StrokeStyleValue{NoStroke{}};
    const PropertyValue blend = BlendModeValue::kNormal;
    const PropertyValue decoration = ConnectorDecorationValue::kArrow;

    EXPECT_TRUE(std::holds_alternative<bool>(boolean));
    EXPECT_TRUE(std::holds_alternative<float>(scalar));
    EXPECT_TRUE(std::holds_alternative<ColorValue>(color));
    EXPECT_TRUE(std::holds_alternative<FillStyleValue>(fill));
    EXPECT_TRUE(std::holds_alternative<StrokeStyleValue>(stroke));
    EXPECT_TRUE(std::holds_alternative<BlendModeValue>(blend));
    EXPECT_TRUE(std::holds_alternative<ConnectorDecorationValue>(decoration));
}

TEST(SemanticTypes, ObjectContentIsAClosedNineWayTypedUnion) {
    const ResourceId resource_id{ObjectId::fromUint64(2U)};
    const ObjectContent shape = ShapeContent{7U, 12.0, 24.0};
    const ObjectContent image = ImageContent{
        resource_id, 1920.0, 1080.0, NormalizedRect{0.0, 0.0, 1.0, 1.0},
        ImageContentMode::kFit, 320.0, 180.0};
    const ObjectContent path = VectorPathContent{
        VectorPathGeometry{FillRule::kNonZero, {MoveTo{Vec2{1.0, 2.0}}}}};
    const ObjectContent rich_text = RichTextContent{RichTextDocument{{
        Paragraph{ObjectId::fromUint64(3U), ParagraphStyle{ParagraphAlignment::kLeft, 1.0, 0.0, 0.0},
                  {TextRun{"typed", TextStyle{}}}}}}};
    const ObjectContent vector_stroke = VectorStrokeContent{
        StrokeRecord{BrushDescriptor{}, 5U, VectorStrokeData{}}};
    const ObjectContent dab_stroke = DabStrokeContent{
        StrokeRecord{BrushDescriptor{}, 6U, DabStrokeData{}}};
    const ObjectContent connector = ConnectorContent{
        ConnectorEndpoint{FreePointEndpoint{Vec2{0.0, 0.0}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{1.0, 1.0}}},
        ConnectorRouting::kStraight};
    const ObjectContent sticky = StickyContent{80.0, 60.0};
    const ObjectContent group = GroupContent{};

    EXPECT_TRUE(std::holds_alternative<ShapeContent>(shape));
    EXPECT_TRUE(std::holds_alternative<ImageContent>(image));
    EXPECT_TRUE(std::holds_alternative<VectorPathContent>(path));
    EXPECT_TRUE(std::holds_alternative<RichTextContent>(rich_text));
    EXPECT_TRUE(std::holds_alternative<VectorStrokeContent>(vector_stroke));
    EXPECT_TRUE(std::holds_alternative<DabStrokeContent>(dab_stroke));
    EXPECT_TRUE(std::holds_alternative<ConnectorContent>(connector));
    EXPECT_TRUE(std::holds_alternative<StickyContent>(sticky));
    EXPECT_TRUE(std::holds_alternative<GroupContent>(group));
}

TEST(SemanticTypes, LeafMachineProjectionUsesReleasedRichTextAndStrokeTypes) {
    const ParagraphStyle paragraph_style{
        ParagraphAlignment::kJustify, 1.25, 2.0, 3.0};
    EXPECT_EQ(paragraph_style.alignment, ParagraphAlignment::kJustify);
    EXPECT_EQ(paragraph_style.line_height, 1.25);
    EXPECT_EQ(paragraph_style.spacing_before, 2.0);
    EXPECT_EQ(paragraph_style.spacing_after, 3.0);

    const ObjectId paragraph_id = ObjectId::fromUint64(9U);
    const RichTextDelta delta{
        1U,
        {DeleteTextStep{paragraph_id, 3U, 5U},
         SetInlineStyleStep{paragraph_id, 3U, 5U, TextStyle{}}},
    };
    EXPECT_EQ(delta.delta_version, 1U);
    ASSERT_EQ(delta.steps.size(), 2U);
    EXPECT_TRUE(std::holds_alternative<DeleteTextStep>(delta.steps.front()));
    EXPECT_EQ(std::get<DeleteTextStep>(delta.steps.front()).scalar_count, 5U);

    const PiecewiseLinearCurve01 curve{{CurvePoint01{0.0F, 0.0F}, CurvePoint01{1.0F, 1.0F}}};
    const PressureMapping pressure{true, curve, curve};
    const SpacingSettings spacing{0.25F};
    const BrushDescriptor brush{
        1U, 1U, ColorValue{}, 4.0, 1.0F, pressure, TiltMapping{}, SmoothingSettings{}, spacing,
        BrushBlendMode::kNormal, std::nullopt,
    };
    EXPECT_EQ(brush.pressure.size_curve, curve);
    EXPECT_EQ(brush.spacing.normalized_spacing, 0.25F);

    const DabInstance dab{Vec2{2.0, 3.0}, 4.0, 0.5F, 0.75F};
    const StrokeRecord record{brush, 0x8000000000000001ULL, DabStrokeData{{dab}}};
    EXPECT_EQ(std::get<DabStrokeData>(record.data).dabs.front().center, (Vec2{2.0, 3.0}));
    EXPECT_EQ(record.deterministic_seed, 0x8000000000000001ULL);
}

TEST(SemanticTypes, EraseMaskGeometryIsAClosedTypedUnion) {
    const EraseMaskGeometry swept = SweptCircleMask{{EraseCubicSegment{
        EraseKnot{Vec2{0.0, 0.0}, 2.0}, EraseKnot{Vec2{4.0, 4.0}, 2.0},
        Vec2{1.0, 0.0}, Vec2{3.0, 4.0}}}};
    const EraseMaskGeometry filled = FilledPathMask{
        VectorPathGeometry{FillRule::kEvenOdd, {ClosePath{}}}};

    EXPECT_TRUE(std::holds_alternative<SweptCircleMask>(swept));
    EXPECT_TRUE(std::holds_alternative<FilledPathMask>(filled));
}

} // namespace canvas::semantic
