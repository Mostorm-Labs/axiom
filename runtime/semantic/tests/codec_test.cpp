#include "canvas/semantic/codec.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace canvas::semantic {

TEST(SemanticCodec, EncodesAndDecodesCanonicalOperation) {
    const std::vector<CanonicalField> fields{{2U, {0x20U}}, {7U, {0x01U, 0x02U}}};
    const auto encoded = SemanticCodec::encodeOperation(OperationKind::kSetObjectSize, fields);
    ASSERT_TRUE(encoded.ok());
    const auto decoded = SemanticCodec::decodeOperation(encoded.bytes);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(decoded.operation.kind, OperationKind::kSetObjectSize);
    ASSERT_EQ(decoded.fields.size(), 2U);
    EXPECT_EQ(decoded.fields[1].id, 7U);
}

TEST(SemanticCodec, CanonicalizesNegativeZeroToPositiveZero) {
    const auto negative = SemanticCodec::encodeCanonicalF64(-0.0);
    const auto positive = SemanticCodec::encodeCanonicalF64(0.0);
    EXPECT_TRUE(negative.ok());
    EXPECT_TRUE(positive.ok());
    EXPECT_EQ(negative.bytes, positive.bytes);
}

TEST(SemanticCodec, StableSeedContainsExactlySixtyCases) {
    const auto seed = SemanticCodec::stableSeedV01();
    ASSERT_EQ(seed.size(), 60U);
    EXPECT_EQ(seed.front().stable_id, "g1-seed-v0.1-000");
    EXPECT_EQ(seed.back().stable_id, "g1-seed-v0.1-059");
}

TEST(SemanticCodec, ProtobufRuntimeRoundTripsAllReconciledOperations) {
    for (unsigned value = 1; value <= 15; ++value) {
        const auto result = SemanticCodec::encodeProtobufOperation(static_cast<OperationKind>(value));
#if defined(CANVAS_SEMANTIC_PROTOBUF)
        ASSERT_TRUE(result.ok()) << value;
        ASSERT_FALSE(result.bytes.empty()) << value;
#else
        EXPECT_EQ(result.error, SemanticError::kRuntimeUnavailable) << value;
#endif
    }
}

TEST(SemanticCodec, ObservesAuthorityGoldenVec2AsCanonicalBytes) {
    const std::vector<std::uint8_t> bytes{
        0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xf0U, 0x3fU,
        0x11U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x40U,
    };
    const auto observation = SemanticCodec::observeGoldenFixture("Vec2", bytes, true);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    ASSERT_TRUE(observation.accepted);
    EXPECT_EQ(observation.canonicality, GoldenCanonicality::kCanonical);
    EXPECT_EQ(observation.canonical_bytes, bytes);
#else
    EXPECT_EQ(observation.stage, GoldenCodecStage::kRuntimeUnavailable);
#endif
}

TEST(SemanticCodec, ObservesAuthorityCanonicalityAndStableNegativeOutcomes) {
    const std::vector<std::uint8_t> vec2_noncanonical{
        0x11U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x40U,
        0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xf0U, 0x3fU,
    };
    const auto noncanonical = SemanticCodec::observeGoldenFixture("Vec2", vec2_noncanonical, true);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    ASSERT_TRUE(noncanonical.accepted);
    EXPECT_EQ(noncanonical.canonicality, GoldenCanonicality::kNonCanonicalInput);
    EXPECT_EQ(noncanonical.stage, GoldenCodecStage::kCanonicalEncode);

    const std::vector<std::uint8_t> duplicate_transform{
        0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xf0U, 0x3fU,
        0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xf0U, 0x3fU,
    };
    const auto duplicate = SemanticCodec::observeGoldenFixture("Transform2D", duplicate_transform, true);
    EXPECT_FALSE(duplicate.accepted);
    EXPECT_EQ(duplicate.stage, GoldenCodecStage::kWirePreflight);
    EXPECT_EQ(duplicate.category, "DUPLICATE_SINGULAR_FIELD");

    const std::vector<std::uint8_t> invalid_id{
        0x0aU, 0x0fU, 0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U,
        0x66U, 0x77U, 0x88U, 0x99U, 0xaaU, 0xbbU, 0xccU, 0xddU,
        0xeeU,
    };
    const auto id = SemanticCodec::observeGoldenFixture("Id128", invalid_id, true);
    EXPECT_FALSE(id.accepted);
    EXPECT_EQ(id.stage, GoldenCodecStage::kDtoMap);
    EXPECT_EQ(id.category, "INVALID_ID");

    const std::vector<std::uint8_t> empty_order_key{0x0aU, 0x00U};
    const auto order_key = SemanticCodec::observeGoldenFixture("OrderKey", empty_order_key, true);
    EXPECT_FALSE(order_key.accepted);
    EXPECT_EQ(order_key.stage, GoldenCodecStage::kValidate);
    EXPECT_EQ(order_key.category, "INVARIANT_VIOLATION");
#else
    EXPECT_EQ(noncanonical.stage, GoldenCodecStage::kRuntimeUnavailable);
#endif
}

TEST(SemanticCodec, ObservesRichTextDeltaAsCanonicalBytesAndCountBasedProjection) {
    const std::vector<std::uint8_t> bytes{
        0x08U, 0x01U,
        0x12U, 0x1aU,
        0x12U, 0x18U,
        0x0aU, 0x12U,
        0x0aU, 0x10U,
        0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
        0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU,
        0x10U, 0x03U, 0x18U, 0x05U,
    };
    const auto observation = SemanticCodec::observeGoldenFixture("RichTextDelta", bytes, true);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    ASSERT_TRUE(observation.accepted);
    EXPECT_EQ(observation.canonicality, GoldenCanonicality::kCanonical);
    EXPECT_EQ(observation.canonical_bytes, bytes);
    EXPECT_EQ(
        observation.semantic_projection_json,
        "{\"deltaVersion\":1,\"steps\":[{\"kind\":\"DeleteText\",\"paragraphId\":\"000102030405060708090a0b0c0d0e0f\",\"startScalar\":3,\"scalarCount\":5}]}"
    );
#else
    EXPECT_EQ(observation.stage, GoldenCodecStage::kRuntimeUnavailable);
#endif
}

TEST(SemanticCodec, AcceptsOrderedRepeatedRichTextDeltaSteps) {
    const std::vector<std::uint8_t> bytes{
        0x08U, 0x01U, 0x12U, 0x1dU, 0x0aU, 0x1bU, 0x0aU, 0x12U,
        0x0aU, 0x10U, 0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
        0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU, 0x10U, 0x01U,
        0x1aU, 0x01U, 0x41U, 0x22U, 0x00U, 0x12U, 0x1aU, 0x12U, 0x18U, 0x0aU,
        0x12U, 0x0aU, 0x10U, 0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U,
        0x07U, 0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU, 0x10U,
        0x09U, 0x18U, 0x02U,
    };
    const auto observation = SemanticCodec::observeGoldenFixture("RichTextDelta", bytes, true);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    EXPECT_TRUE(observation.accepted);
    EXPECT_EQ(observation.canonicality, GoldenCanonicality::kCanonical);
    EXPECT_EQ(observation.canonical_bytes, bytes);
#else
    EXPECT_EQ(observation.stage, GoldenCodecStage::kRuntimeUnavailable);
#endif
}

TEST(SemanticCodec, ObservesRichTextDocumentWithReleasedParagraphStyleProjection) {
    const std::vector<std::uint8_t> bytes{
        0x0aU, 0x41U, 0x0aU, 0x12U, 0x0aU, 0x10U,
        0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
        0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU,
        0x12U, 0x1dU, 0x08U, 0x01U,
        0x11U, 0x9aU, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0xf1U, 0x3fU,
        0x19U, 0x9aU, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0xc9U, 0x3fU,
        0x21U, 0x33U, 0x33U, 0x33U, 0x33U, 0x33U, 0x33U, 0xd3U, 0x3fU,
        0x1aU, 0x0cU, 0x0aU, 0x08U, 0x72U, 0x65U, 0x6cU, 0x65U,
        0x61U, 0x73U, 0x65U, 0x64U, 0x12U, 0x00U,
    };
    const auto observation = SemanticCodec::observeGoldenFixture("RichTextDocument", bytes, true);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    ASSERT_TRUE(observation.accepted);
    EXPECT_EQ(observation.canonicality, GoldenCanonicality::kCanonical);
    EXPECT_EQ(observation.canonical_bytes, bytes);
    EXPECT_NE(observation.semantic_projection_json.find("\"paragraphs\""), std::string::npos);
    EXPECT_NE(observation.semantic_projection_json.find("\"released\""), std::string::npos);
#else
    EXPECT_EQ(observation.stage, GoldenCodecStage::kRuntimeUnavailable);
#endif
}

TEST(SemanticCodec, ObservesStrokeFixedSeedAndDabCenterAsCanonicalProjection) {
    const std::vector<std::uint8_t> bytes{
        0x0aU, 0x00U,
        0x11U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U,
        0x22U, 0x29U,
        0x0aU, 0x27U,
        0x0aU, 0x12U,
        0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xf0U, 0x3fU,
        0x11U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x40U,
        0x11U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x10U, 0x40U,
        0x1dU, 0x00U, 0x00U, 0x00U, 0x3fU,
        0x25U, 0x00U, 0x00U, 0x40U, 0x3fU,
    };
    const auto observation = SemanticCodec::observeGoldenFixture("StrokeRecord", bytes, true);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    ASSERT_TRUE(observation.accepted);
    EXPECT_EQ(observation.canonicality, GoldenCanonicality::kCanonical);
    EXPECT_EQ(observation.canonical_bytes, bytes);
    EXPECT_EQ(
        observation.semantic_projection_json,
        "{\"deterministicSeed\":\"9223372036854775809\",\"data\":{\"kind\":\"Dab\",\"dabs\":[{\"center\":{\"x\":1,\"y\":2},\"size\":4,\"rotation\":0.5,\"opacity\":0.75}]}}"
    );
#else
    EXPECT_EQ(observation.stage, GoldenCodecStage::kRuntimeUnavailable);
#endif
}

} // namespace canvas::semantic
