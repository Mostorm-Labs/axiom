#include "canvas/semantic/snapshot.hpp"

#include "g1_06_projection.hpp"

#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"
#include "protobuf_object_mapping.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

void appendVarint(std::vector<std::uint8_t>& output, std::uint64_t value) {
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7fU);
        value >>= 7U;
        if (value != 0U) byte |= 0x80U;
        output.push_back(byte);
    } while (value != 0U);
}

void appendFixed64(std::vector<std::uint8_t>& output, double value) {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
        output.push_back(static_cast<std::uint8_t>(bits >> shift));
    }
}

void appendBytes(std::vector<std::uint8_t>& output, std::uint32_t field,
                 const std::vector<std::uint8_t>& value) {
    appendVarint(output, static_cast<std::uint64_t>(field << 3U) | 2U);
    appendVarint(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

std::vector<std::uint8_t> rawId(std::uint8_t value) {
    std::vector<std::uint8_t> bytes(16U, 0U);
    bytes.back() = value;
    return bytes;
}

[[maybe_unused]] std::vector<std::uint8_t> rawShapeObject(bool include_kind_version = true,
                                                          bool duplicate_kind = false,
                                                          bool unknown_object_field = false,
                                                          bool unknown_shape_field = false,
                                                          bool multiple_content_oneof = false,
                                                          std::uint32_t shape_kind = 1U,
                                                          bool include_transform_b = true) {
    std::vector<std::uint8_t> shape;
    appendVarint(shape, 0x08U);
    appendVarint(shape, shape_kind);
    appendVarint(shape, 0x11U);
    appendFixed64(shape, 2.0);
    appendVarint(shape, 0x19U);
    appendFixed64(shape, 3.0);
    if (unknown_shape_field) {
        appendVarint(shape, 0x20U); // ShapeContent field 4 is unknown.
        appendVarint(shape, 1U);
    }

    std::vector<std::uint8_t> content;
    appendBytes(content, 1U, shape);
    if (multiple_content_oneof) appendBytes(content, 9U, {}); // GroupContent conflicts with ShapeContent.

    std::vector<std::uint8_t> order_key;
    appendBytes(order_key, 1U, {1U});
    std::vector<std::uint8_t> placement;
    appendBytes(placement, 2U, order_key);

    std::vector<std::uint8_t> transform;
    for (std::uint32_t field = 1U; field <= 6U; ++field) {
        if (field == 2U && !include_transform_b) continue;
        appendVarint(transform, static_cast<std::uint64_t>(field << 3U) | 1U);
        appendFixed64(transform, field == 1U || field == 4U ? 1.0 : 0.0);
    }

    std::vector<std::uint8_t> object;
    appendBytes(object, 1U, [&] { std::vector<std::uint8_t> id_message; appendBytes(id_message, 1U, rawId(1U)); return id_message; }());
    appendVarint(object, 0x10U);
    appendVarint(object, static_cast<std::uint32_t>(ObjectKind::kShape));
    if (include_kind_version) {
        appendVarint(object, 0x18U);
        appendVarint(object, 1U);
    }
    if (duplicate_kind) {
        appendVarint(object, 0x10U);
        appendVarint(object, static_cast<std::uint32_t>(ObjectKind::kShape));
    }
    appendBytes(object, 4U, placement);
    appendBytes(object, 5U, transform);
    appendBytes(object, 6U, {});
    appendBytes(object, 7U, content);
    if (unknown_object_field) {
        appendVarint(object, 0x48U); // ObjectRecord field 9 is unknown.
        appendVarint(object, 1U);
    }
    return object;
}

[[maybe_unused]] std::vector<std::uint8_t> rawSnapshot(std::vector<std::uint8_t> object) {
    std::vector<std::uint8_t> document_id;
    appendBytes(document_id, 1U, rawId(2U));
    std::vector<std::uint8_t> snapshot;
    appendBytes(snapshot, 1U, document_id);
    appendVarint(snapshot, 0x10U);
    appendVarint(snapshot, 1U);
    appendBytes(snapshot, 3U, object);
    return snapshot;
}

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
    object.kind_version = 1U;
    object.placement = Placement{std::nullopt, OrderKey({1U, static_cast<std::uint8_t>(value)})};
    object.transform = Transform2D{1.0, 0.0, 0.25, 1.0, 12.5, -3.0};
    object.content = std::move(content);
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

TEST(G106Snapshot, RejectsUnknownSnapshotWireFieldsBeforeGeneratedParse) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const auto encoded = SnapshotCodec::encode(minimal());
    ASSERT_TRUE(encoded.ok());
    auto with_unknown_field = encoded.bytes;
    // DocumentSnapshot has no field 4.  Generated protobuf parsing would
    // otherwise preserve/ignore this unknown field, but the canonical wire
    // contract requires a fail-closed preflight rejection.
    with_unknown_field.push_back(0x20U); // field 4, wire type 0
    with_unknown_field.push_back(0x01U);
    EXPECT_EQ(SnapshotCodec::decode(with_unknown_field).error, SemanticError::kMalformedWire);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, RejectsUnknownAndAmbiguousNestedWireBeforeMapping) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    EXPECT_EQ(SnapshotCodec::decode(rawSnapshot(rawShapeObject(true, false, true))).error,
              SemanticError::kMalformedWire);
    EXPECT_EQ(SnapshotCodec::decode(rawSnapshot(rawShapeObject(true, false, false, true))).error,
              SemanticError::kMalformedWire);
    EXPECT_EQ(SnapshotCodec::decode(rawSnapshot(rawShapeObject(true, false, false, false, true))).error,
              SemanticError::kMalformedWire);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, RejectsDuplicateSingularAndMissingRequiredSemanticPresence) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    EXPECT_EQ(SnapshotCodec::decode(rawSnapshot(rawShapeObject(true, true))).error,
              SemanticError::kMalformedWire);
    // ObjectRecord.kind_version is a non-optional semantic value.  Absence
    // must not materialize as generated-protobuf's default zero.
    EXPECT_EQ(SnapshotCodec::decode(rawSnapshot(rawShapeObject(false))).error,
              SemanticError::kInvalidSemanticValue);
    // b=0 is a legal transform value.  This catches the dangerous case where
    // a missing scalar silently becomes the protobuf accessor default.
    EXPECT_EQ(SnapshotCodec::decode(rawSnapshot(rawShapeObject(true, false, false, false, false, 1U, false))).error,
              SemanticError::kInvalidSemanticValue);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, RejectsSemanticallyInvalidMappedRecordWithSharedValidator) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    EXPECT_EQ(SnapshotCodec::decode(rawSnapshot(rawShapeObject(true, false, false, false, false, 0U))).error,
              SemanticError::kInvalidSemanticValue);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, MapsDirectColorValueMechanicallyThenRejectsItSemantically) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto direct_color = minimal();
    direct_color.objects.front().properties.entries = {
        {0x00000001U, ColorValue{0.1F, 0.2F, 0.3F, 1.0F}}};

    ObjectRecord mapped;
    ASSERT_TRUE(internal::roundTripProtobufObjectRecord(
        direct_color.objects.front(), mapped));
    ASSERT_EQ(mapped.properties.entries.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<ColorValue>(mapped.properties.entries.front().value));
    EXPECT_EQ(std::get<ColorValue>(mapped.properties.entries.front().value),
              ColorValue(0.1F, 0.2F, 0.3F, 1.0F));

    EXPECT_EQ(SnapshotCodec::encode(direct_color).error,
              SemanticError::kInvalidSemanticValue);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, RejectsSemanticallyInvalidObjectOnEncode) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto snapshot = minimal();
    snapshot.objects.front().content = ShapeContent{0U, 2.0, 3.0};
    EXPECT_EQ(SnapshotCodec::encode(snapshot).error, SemanticError::kInvalidSemanticValue);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, CanonicalizesPropertyBagEntryPermutation) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto snapshot = minimal();
    const auto solid_fill = FillStyleValue{SolidFill{ColorValue{0.1F, 0.2F, 0.3F, 1.0F}}};
    const auto solid_stroke = StrokeStyleValue{SolidStroke{ColorValue{0.4F, 0.5F, 0.6F, 1.0F}, 2.0,
        StrokeCap::kRound, StrokeJoin{MiterJoin{4.0}}, StrokeDash{DashPattern{{1.0, 2.0}, 0.5}}}};
    snapshot.objects.front().properties.entries = {
        {0x00000101U, solid_stroke}, {0x00000002U, true}, {0x00000100U, solid_fill},
        {0x00000004U, BlendModeValue::kNormal}, {0x00000003U, 0.25F}, {0x00000001U, false}};
    const auto encoded = SnapshotCodec::encode(snapshot);
    ASSERT_TRUE(encoded.ok());

    std::reverse(snapshot.objects.front().properties.entries.begin(),
                 snapshot.objects.front().properties.entries.end());
    const auto permuted = SnapshotCodec::encode(snapshot);
    ASSERT_TRUE(permuted.ok());
    EXPECT_EQ(permuted.bytes, encoded.bytes);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, CanonicalizesEraseMaskSetPermutationOnDabStroke) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId(id(0x778U));
    snapshot.schema_version = 1U;
    snapshot.objects = {record(20U, ObjectKind::kDabStroke, DabStrokeContent{StrokeRecord{
        BrushDescriptor{3U, 1U, ColorValue{0.1F, 0.2F, 0.3F, 1.0F}, 3.0, 0.75F,
            PressureMapping{}, TiltMapping{}, SmoothingSettings{0.2F}, SpacingSettings{0.1F},
            BrushBlendMode::kNormal, ResourceId{id(501U)}},
        123U, DabStrokeData{{DabInstance{Vec2{1.0, 2.0}, 3.0, 0.5F, 0.75F}}}}})};
    snapshot.objects.front().erase_masks = {
        EraseMaskRecord{id(102U), FilledPathMask{VectorPathGeometry{FillRule::kEvenOdd,
            {MoveTo{Vec2{0.0, 0.0}}, LineTo{Vec2{1.0, 0.0}}, ClosePath{}}}}},
        EraseMaskRecord{id(101U), SweptCircleMask{{EraseCubicSegment{
            EraseKnot{Vec2{1.0, 2.0}, 3.0}, EraseKnot{Vec2{4.0, 5.0}, 6.0},
            Vec2{7.0, 8.0}, Vec2{9.0, 10.0}}}}}};
    const auto encoded = SnapshotCodec::encode(snapshot);
    ASSERT_TRUE(encoded.ok());

    std::reverse(snapshot.objects.front().erase_masks.begin(), snapshot.objects.front().erase_masks.end());
    const auto permuted = SnapshotCodec::encode(snapshot);
    ASSERT_TRUE(permuted.ok());
    EXPECT_EQ(permuted.bytes, encoded.bytes);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, CanonicalizesF32NegativeZeroInPropertyValue) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto negative_zero = minimal();
    negative_zero.objects.front().properties.entries = {{0x00000003U, -0.0F}};
    auto positive_zero = negative_zero;
    positive_zero.objects.front().properties.entries = {{0x00000003U, 0.0F}};
    const auto negative_encoded = SnapshotCodec::encode(negative_zero);
    const auto positive_encoded = SnapshotCodec::encode(positive_zero);
    ASSERT_TRUE(negative_encoded.ok());
    ASSERT_TRUE(positive_encoded.ok());
    EXPECT_EQ(negative_encoded.bytes, positive_encoded.bytes);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, CanonicalizesF64NegativeZeroInTransform) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto negative_zero = minimal();
    negative_zero.objects.front().transform.tx = -0.0;
    auto positive_zero = negative_zero;
    positive_zero.objects.front().transform.tx = 0.0;
    const auto negative_encoded = SnapshotCodec::encode(negative_zero);
    const auto positive_encoded = SnapshotCodec::encode(positive_zero);
    ASSERT_TRUE(negative_encoded.ok());
    ASSERT_TRUE(positive_encoded.ok());
    EXPECT_EQ(negative_encoded.bytes, positive_encoded.bytes);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

TEST(G106Snapshot, RoundTripsAllV1ContentAndCanonicalizesOrder) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId(id(0x777));
    snapshot.schema_version = 1U;
    snapshot.objects = {
        record(10U, ObjectKind::kConnector, ConnectorContent{
            ConnectorEndpoint{AttachedEndpoint{id(9U), StablePortAnchor{2U}}},
            ConnectorEndpoint{FreePointEndpoint{Vec2{3.0, 4.0}}}, ConnectorRouting::kStraight}),
        record(9U, ObjectKind::kGroup, GroupContent{}),
        record(8U, ObjectKind::kSticky, StickyContent{10.0, 20.0}),
        record(7U, ObjectKind::kConnector, ConnectorContent{
            ConnectorEndpoint{AttachedEndpoint{id(2U), AutoPerimeterAnchor{Vec2{0.25, 0.75}}}},
            ConnectorEndpoint{FreePointEndpoint{Vec2{3.0, 4.0}}}, ConnectorRouting::kOrthogonal}),
        record(6U, ObjectKind::kDabStroke, DabStrokeContent{StrokeRecord{
            BrushDescriptor{3U, 1U, ColorValue{0.1F, 0.2F, 0.3F, 0.4F}, 3.0, 0.75F,
                PressureMapping{}, TiltMapping{}, SmoothingSettings{0.2F}, SpacingSettings{0.1F},
                BrushBlendMode::kNormal, ResourceId{id(501U)}},
            0x8000000000000001ULL, DabStrokeData{{DabInstance{Vec2{1.0, 2.0}, 3.0, 0.5F, 0.75F}}}}}),
        record(5U, ObjectKind::kVectorStroke, VectorStrokeContent{StrokeRecord{
            BrushDescriptor{2U, 1U, ColorValue{0.1F, 0.2F, 0.3F, 0.4F}, 5.0, 0.75F,
                PressureMapping{true,
                    PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.25F}, CurvePoint01{1.0F, 1.0F}}},
                    PiecewiseLinearCurve01{{CurvePoint01{0.0F, 0.5F}, CurvePoint01{1.0F, 0.75F}}}},
                TiltMapping{}, SmoothingSettings{0.2F}, SpacingSettings{0.1F},
                BrushBlendMode::kHighlighter, std::nullopt},
            42U, VectorStrokeData{{StrokeSample{Vec2{1.0, 2.0}, 0.5F, Vec2{0.0, 0.0}}}}}}),
        record(4U, ObjectKind::kRichText, RichTextContent{RichTextDocument{std::vector<Paragraph>{Paragraph{
            id(40U), ParagraphStyle{ParagraphAlignment::kCenter, 1.2, 0.1, 0.2},
            std::vector<TextRun>{
                TextRun{"styled", TextStyle{ResourceId{id(41U)}, 12.0, 400U, true, false,
                                               ColorValue{1, 1, 1, 1}}},
                TextRun{"plain", TextStyle{ResourceId{id(42U)}, 12.0, 400U, false, false,
                                              ColorValue{1, 1, 1, 1}}}}}}}}),
        record(3U, ObjectKind::kVectorPath, VectorPathContent{VectorPathGeometry{
            FillRule::kNonZero, {MoveTo{Vec2{0, 0}}, LineTo{Vec2{0.25, 0.5}},
                QuadTo{Vec2{0.5, 1}, Vec2{1, 0}}, CubicTo{Vec2{1.25, 0.5}, Vec2{1.5, 1.0}, Vec2{2.0, 0.0}},
                ClosePath{}}}}),
        record(2U, ObjectKind::kImage, ImageContent{ResourceId{id(22U)}, 640.0, 480.0,
            NormalizedRect{0.1, 0.2, 0.3, 0.4}, ImageContentMode::kFit, 320.0, 240.0}),
        record(1U, ObjectKind::kShape, ShapeContent{1U, 2.5, 2.5})};
    snapshot.objects.back().properties.entries = {
        {2U, true}, {3U, 0.5F},
        {0x00000100U, FillStyleValue{SolidFill{ColorValue{1.0F, 0.0F, 0.5F, 1.0F}}}},
        {0x00000101U, StrokeStyleValue{SolidStroke{ColorValue{1, 1, 1, 1}, 2.0, StrokeCap::kRound,
              StrokeJoin{MiterJoin{4.0}}, StrokeDash{DashPattern{{1.0, 2.0}, 0.0}}}}},
        };
    snapshot.objects[4].erase_masks = {
        EraseMaskRecord{id(101U), SweptCircleMask{{EraseCubicSegment{
            EraseKnot{Vec2{1.0, 2.0}, 3.0}, EraseKnot{Vec2{4.0, 5.0}, 6.0},
            Vec2{7.0, 8.0}, Vec2{9.0, 10.0}}}}},
        EraseMaskRecord{
            id(102U),
            FilledPathMask{VectorPathGeometry{
                FillRule::kEvenOdd,
                {MoveTo{Vec2{0.0, 0.0}}, LineTo{Vec2{1.0, 0.0}}, ClosePath{}}}}}};
    snapshot.objects[0].properties.entries = {
        {3U, 0.5F},
        {0x00000101U, StrokeStyleValue{SolidStroke{ColorValue{0.2F, 0.3F, 0.4F, 1.0F}, 1.0,
            StrokeCap::kButt, StrokeJoin{RoundJoin{}}, StrokeDash{SolidDash{}}}}},
        {0x00000200U, ConnectorDecorationValue::kArrow},
        {0x00000201U, ConnectorDecorationValue::kArrow}};
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
