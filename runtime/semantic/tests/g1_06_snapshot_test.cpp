#include "canvas/semantic/snapshot.hpp"

#include "g1_06_projection.hpp"

#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

#if defined(CANVAS_SEMANTIC_PROTOBUF)
std::string canonicalProjectionJson(const SemanticSnapshot& snapshot) {
    ReferenceObjectStore store;
    for (const ObjectRecord& object : snapshot.objects) {
        if (!internal::ObjectStoreMutator::insertFresh(store, object)) {
            throw std::runtime_error("snapshot fixture contains a duplicate object ID");
        }
    }
    verification::g1_06::ProjectionDocumentId document_id;
    std::copy(snapshot.document_id.value().bytes.begin(), snapshot.document_id.value().bytes.end(),
              document_id.bytes.begin());
    return verification::g1_06::writeCanonicalProjectionJson(
        verification::g1_06::projectDocument(document_id, snapshot.schema_version, store));
}
#endif

SemanticSnapshot minimal() {
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId(id(0x100));
    snapshot.schema_version = 1U;
    ObjectRecord object;
    object.id = id(1U);
    object.kind = ObjectKind::kShape;
    object.kind_version = 1U;
    object.placement.order_key = OrderKey({1U});
    object.content = ShapeContent{1U, 2.0, 3.0};
    snapshot.objects.push_back(object);
    return snapshot;
}

[[maybe_unused]] ObjectRecord record(std::uint64_t value, ObjectKind kind, ObjectContent content) {
    ObjectRecord object;
    object.id = id(value);
    object.kind = kind;
    object.kind_version = 7U;
    object.placement = Placement{value > 1U ? std::optional<ObjectId>(id(1U)) : std::nullopt,
                                 OrderKey({1U, static_cast<std::uint8_t>(value)})};
    object.transform = Transform2D{1.0, -0.0, 0.25, 1.0, 12.5, -3.0};
    object.content = std::move(content);
    object.properties.entries = {
        {1U, true}, {2U, 0.5F}, {3U, ColorValue{0.1F, 0.2F, 0.3F, 1.0F}},
        {4U, FillStyleValue{SolidFill{ColorValue{1.0F, 0.0F, 0.5F, 1.0F}}}},
        {5U, StrokeStyleValue{SolidStroke{ColorValue{1, 1, 1, 1}, 2.0, StrokeCap::kRound,
                                            StrokeJoin{MiterJoin{4.0}}, StrokeDash{SolidDash{}}}}},
        {6U, BlendModeValue::kNormal}, {7U, ConnectorDecorationValue::kArrow}};
    object.erase_masks = {EraseMaskRecord{id(100U + value), FilledPathMask{VectorPathGeometry{
        FillRule::kEvenOdd, {MoveTo{Vec2{0.0, 0.0}}, LineTo{Vec2{1.0, 0.0}}, ClosePath{}}}}}};
    return object;
}

TEST(G106Snapshot, ProtobufOffContractOrRoundTrip) {
    const auto encoded = SnapshotCodec::encode(minimal());
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    ASSERT_TRUE(encoded.ok());
    const auto decoded = SnapshotCodec::decode(encoded.bytes);
    ASSERT_TRUE(decoded.ok());
    ASSERT_TRUE(decoded.snapshot.has_value());
    EXPECT_EQ(decoded.snapshot->document_id, minimal().document_id);
    EXPECT_EQ(decoded.snapshot->objects, minimal().objects);
    SemanticSnapshot empty;
    empty.document_id = DocumentId(id(0x101));
    empty.schema_version = 1U;
    const auto empty_encoded = SnapshotCodec::encode(empty);
    ASSERT_TRUE(empty_encoded.ok());
    const auto empty_decoded = SnapshotCodec::decode(empty_encoded.bytes);
    ASSERT_TRUE(empty_decoded.ok());
    EXPECT_EQ(empty_decoded.snapshot, empty);
#else
    EXPECT_EQ(encoded.error, SemanticError::kRuntimeUnavailable);
    const auto decoded = SnapshotCodec::decode({});
    EXPECT_EQ(decoded.error, SemanticError::kRuntimeUnavailable);
#endif
}

TEST(G106Snapshot, RejectsInvalidEnvelopeAndDuplicateIds) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto snapshot = minimal();
    snapshot.schema_version = 2U;
    EXPECT_EQ(SnapshotCodec::encode(snapshot).error, SemanticError::kUnsupportedVersion);
    snapshot = minimal();
    snapshot.document_id = DocumentId{};
    EXPECT_EQ(SnapshotCodec::encode(snapshot).error, SemanticError::kInvalidSemanticValue);
    snapshot = minimal();
    snapshot.objects.push_back(snapshot.objects.front());
    EXPECT_EQ(SnapshotCodec::encode(snapshot).error, SemanticError::kDuplicateCanonicalKey);
    EXPECT_EQ(SnapshotCodec::decode({0x80U}).error, SemanticError::kMalformedWire);
    const auto encoded = SnapshotCodec::encode(minimal());
    ASSERT_TRUE(encoded.ok());
    auto truncated = encoded.bytes;
    truncated.pop_back();
    EXPECT_EQ(SnapshotCodec::decode(truncated).error, SemanticError::kMalformedWire);
#else
    EXPECT_EQ(SnapshotCodec::encode(minimal()).error, SemanticError::kRuntimeUnavailable);
#endif
}

TEST(G106Snapshot, RoundTripsAllV1ContentAndCanonicalizesOrder) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId(id(0x777));
    snapshot.schema_version = 1U;
    snapshot.objects = {
        record(10U, ObjectKind::kConnector, ConnectorContent{
            ConnectorEndpoint{AttachedEndpoint{id(9U), StablePortAnchor{42U}}},
            ConnectorEndpoint{FreePointEndpoint{Vec2{3.0, 4.0}}}, ConnectorRouting::kStraight}),
        record(9U, ObjectKind::kGroup, GroupContent{}),
        record(8U, ObjectKind::kSticky, StickyContent{10.0, 20.0}),
        record(7U, ObjectKind::kConnector, ConnectorContent{
            ConnectorEndpoint{AttachedEndpoint{id(2U), AutoPerimeterAnchor{Vec2{0.25, 0.75}}}},
            ConnectorEndpoint{FreePointEndpoint{Vec2{3.0, 4.0}}}, ConnectorRouting::kOrthogonal}),
        record(6U, ObjectKind::kDabStroke, DabStrokeContent{StrokeRecord{
            BrushDescriptor{}, 0x8000000000000001ULL, DabStrokeData{{DabInstance{Vec2{1.0, 2.0}, 3.0, 0.5F, 0.75F}}}}}),
        record(5U, ObjectKind::kVectorStroke, VectorStrokeContent{StrokeRecord{
            BrushDescriptor{51U, 2U, ColorValue{0.1F, 0.2F, 0.3F, 0.4F}, 5.0, 0.75F,
                PressureMapping{true, PiecewiseLinearCurve01{{{0.0F, 0.25F}, {1.0F, 1.0F}}},
                                PiecewiseLinearCurve01{{{0.0F, 0.5F}, {1.0F, 0.75F}}}},
                TiltMapping{true, 0.5F, -0.25F}, SmoothingSettings{0.2F}, SpacingSettings{0.1F},
                BrushBlendMode::kHighlighter, ResourceId{id(501U)}},
            42U, VectorStrokeData{{StrokeSample{Vec2{1.0, 2.0}, 0.5F, Vec2{0.1, 0.2}}}}}}),
        record(4U, ObjectKind::kRichText, RichTextContent{RichTextDocument{{Paragraph{
            id(40U), ParagraphStyle{ParagraphAlignment::kCenter, 1.2, 0.1, 0.2},
            {TextRun{"styled", TextStyle{ResourceId{id(41U)}, 12.0, 400U, true, false,
                                           ColorValue{1, 1, 1, 1}}},
             TextRun{"plain", TextStyle{}}}}}}}),
        record(3U, ObjectKind::kVectorPath, VectorPathContent{VectorPathGeometry{
            FillRule::kNonZero, {MoveTo{Vec2{0, 0}}, LineTo{Vec2{0.25, 0.5}},
                QuadTo{Vec2{0.5, 1}, Vec2{1, 0}}, CubicTo{Vec2{1.25, 0.5}, Vec2{1.5, 1.0}, Vec2{2.0, 0.0}},
                ClosePath{}}}}),
        record(2U, ObjectKind::kImage, ImageContent{ResourceId{id(22U)}, 640.0, 480.0,
            NormalizedRect{0.1, 0.2, 0.3, 0.4}, ImageContentMode::kFit, 320.0, 240.0}),
        record(1U, ObjectKind::kShape, ShapeContent{3U, -0.0, 2.5})};
    snapshot.objects.back().properties.entries = {
        {1U, true}, {2U, -0.0F}, {3U, ColorValue{0.25F, 0.5F, 0.75F, 1.0F}},
        {4U, FillStyleValue{NoFill{}}}, {5U, FillStyleValue{SolidFill{ColorValue{1.0F, 0.0F, 0.5F, 1.0F}}}},
        {6U, StrokeStyleValue{NoStroke{}}},
        {7U, StrokeStyleValue{SolidStroke{ColorValue{1, 1, 1, 1}, 2.0, StrokeCap::kRound,
              StrokeJoin{MiterJoin{4.0}}, StrokeDash{DashPattern{{1.0, 2.0}, -0.0}}}}},
        {8U, BlendModeValue::kNormal}, {9U, ConnectorDecorationValue::kArrow}};
    snapshot.objects.back().erase_masks = {
        EraseMaskRecord{id(101U), SweptCircleMask{{EraseCubicSegment{
            EraseKnot{Vec2{1.0, 2.0}, 3.0}, EraseKnot{Vec2{4.0, 5.0}, 6.0},
            Vec2{7.0, 8.0}, Vec2{9.0, 10.0}}}}},
        EraseMaskRecord{
            id(102U),
            FilledPathMask{VectorPathGeometry{
                FillRule::kEvenOdd,
                {MoveTo{Vec2{0.0, 0.0}}, LineTo{Vec2{1.0, 0.0}}, ClosePath{}}}}}};
    const auto encoded = SnapshotCodec::encode(snapshot);
    ASSERT_TRUE(encoded.ok());
    const auto decoded = SnapshotCodec::decode(encoded.bytes);
    ASSERT_TRUE(decoded.ok());
    ASSERT_TRUE(decoded.snapshot.has_value());
    EXPECT_EQ(canonicalProjectionJson(snapshot), canonicalProjectionJson(*decoded.snapshot));
    ASSERT_EQ(decoded.snapshot->objects.size(), 10U);
    for (std::size_t i = 1; i < decoded.snapshot->objects.size(); ++i)
        EXPECT_LT(decoded.snapshot->objects[i - 1U].id.bytes, decoded.snapshot->objects[i].id.bytes);
    std::reverse(snapshot.objects.begin(), snapshot.objects.end());
    const auto permuted = SnapshotCodec::encode(snapshot);
    ASSERT_TRUE(permuted.ok());
    EXPECT_EQ(permuted.bytes, encoded.bytes);
    EXPECT_EQ(decoded.snapshot->objects.front().id, id(1U));
    EXPECT_EQ(decoded.snapshot->objects.back().id, id(10U));
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

} // namespace
} // namespace canvas::semantic
