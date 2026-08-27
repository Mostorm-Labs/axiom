#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/validator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace canvas::semantic {

#if defined(CANVAS_SEMANTIC_PROTOBUF)
namespace {
void appendVarintForTest(std::vector<std::uint8_t>& bytes, std::size_t value) {
    while (value >= 0x80U) {
        bytes.push_back(static_cast<std::uint8_t>(value) | 0x80U);
        value >>= 7U;
    }
    bytes.push_back(static_cast<std::uint8_t>(value));
}
void appendBytesFieldForTest(std::vector<std::uint8_t>& bytes, std::uint32_t field,
                             const std::vector<std::uint8_t>& value) {
    appendVarintForTest(bytes, (static_cast<std::size_t>(field) << 3U) | 2U);
    appendVarintForTest(bytes, value.size());
    bytes.insert(bytes.end(), value.begin(), value.end());
}

std::vector<std::uint8_t> operationWithPayloadForTest(const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> operation;
    std::vector<std::uint8_t> id_bytes(16U, 1U);
    appendBytesFieldForTest(operation, 1U, id_bytes);
    id_bytes[0] = 2U;
    appendBytesFieldForTest(operation, 2U, id_bytes);
    operation.push_back(0x18U); operation.push_back(0x01U);
    operation.push_back(0x20U); operation.push_back(0x01U);
    appendBytesFieldForTest(operation, 5U, payload);
    return operation;
}

std::vector<std::uint8_t> repeatedMessageFieldsForTest(std::uint32_t field, std::size_t count) {
    std::vector<std::uint8_t> result;
    for (std::size_t index = 0U; index < count; ++index) appendBytesFieldForTest(result, field, {});
    return result;
}
} // namespace
#endif

TEST(SemanticCodec, EncodesAndDecodesCanonicalOperation) {
    const std::vector<CanonicalField> fields{{2U, {0x20U}}, {7U, {0x01U, 0x02U}}};
    const auto encoded = SemanticCodec::encodeOperation(OperationKind::kSetObjectSize, fields);
    ASSERT_TRUE(encoded.ok());
    const auto decoded = SemanticCodec::decodeOperation(encoded.bytes);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(decoded.operation.kind(), OperationKind::kSetObjectSize);
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

TEST(SemanticCodec, ProtobufWirePresenceDistinguishesMissingExplicitZeroAndOne) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const std::vector<std::uint8_t> base{
        0x0aU, 0x12U, 0x0aU, 0x10U,
        0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x12U, 0x12U, 0x0aU, 0x10U,
        0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x2aU, 0x02U, 0x0aU, 0x00U,
    };
    const auto missing = SemanticCodec::decodeProtobufOperation(base);
    EXPECT_EQ(missing.error, SemanticError::kInvalidSemanticValue);
    EXPECT_FALSE(missing.presence.schema_version);
    EXPECT_FALSE(missing.presence.payload_version);
    EXPECT_EQ(missing.operation.schema_version, 0U);
    EXPECT_EQ(missing.operation.payload_version, 0U);

    auto explicit_zero = base;
    explicit_zero.insert(explicit_zero.end() - 4, {0x18U, 0x00U, 0x20U, 0x00U});
    const auto zero = SemanticCodec::decodeProtobufOperation(explicit_zero);
    EXPECT_EQ(zero.error, SemanticError::kInvalidSemanticValue);
    EXPECT_TRUE(zero.presence.schema_version);
    EXPECT_TRUE(zero.presence.payload_version);
    EXPECT_EQ(zero.operation.schema_version, 0U);
    EXPECT_EQ(zero.operation.payload_version, 0U);

    auto explicit_one = base;
    explicit_one.insert(explicit_one.end() - 4, {0x18U, 0x01U, 0x20U, 0x01U});
    const auto one = SemanticCodec::decodeProtobufOperation(explicit_one);
    EXPECT_EQ(one.error, SemanticError::kInvalidSemanticValue);
    EXPECT_TRUE(one.presence.schema_version);
    EXPECT_TRUE(one.presence.payload_version);
    EXPECT_EQ(one.operation.schema_version, 1U);
    EXPECT_EQ(one.operation.payload_version, 1U);
    EXPECT_TRUE(validateEnvelope(one.operation, one.presence).ok());
    EXPECT_FALSE(validateEnvelope(missing.operation, missing.presence).ok());
    EXPECT_FALSE(validateEnvelope(zero.operation, zero.presence).ok());
#else
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation({}).error, SemanticError::kRuntimeUnavailable);
#endif
}

TEST(SemanticCodec, ProtobufPreflightRejectsOversizedNestedObjectRecord) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    constexpr std::size_t kObjectLimit = 16U * 1024U * 1024U;
    auto operationWithObjectSize = [](std::size_t object_size) {
        std::vector<std::uint8_t> object(object_size, 0U);
        std::vector<std::uint8_t> insert;
        appendBytesFieldForTest(insert, 1U, object);
        std::vector<std::uint8_t> payload;
        appendBytesFieldForTest(payload, 1U, insert);
        std::vector<std::uint8_t> operation;
        std::vector<std::uint8_t> id_bytes(16U, 1U);
        appendBytesFieldForTest(operation, 1U, id_bytes);
        id_bytes[0] = 2U;
        appendBytesFieldForTest(operation, 2U, id_bytes);
        operation.push_back(0x18U); operation.push_back(0x01U);
        operation.push_back(0x20U); operation.push_back(0x01U);
        appendBytesFieldForTest(operation, 5U, payload);
        return operation;
    };
    EXPECT_NE(SemanticCodec::decodeProtobufOperation(operationWithObjectSize(kObjectLimit)).error,
              SemanticError::kLimitExceeded);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(operationWithObjectSize(kObjectLimit + 1U)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightRejectsUnknownDuplicateWrongTypeAndOverflowFields) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const std::vector<std::uint8_t> base{
        0x0aU, 0x12U, 0x0aU, 0x10U,
        0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x12U, 0x12U, 0x0aU, 0x10U,
        0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x2aU, 0x02U, 0x0aU, 0x00U,
    };
    auto unknown = base;
    unknown.insert(unknown.end(), {0x30U, 0x00U});
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(unknown).error, SemanticError::kMalformedWire);

    auto duplicate = base;
    duplicate.insert(duplicate.end(), {0x2aU, 0x02U, 0x0aU, 0x00U});
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(duplicate).error, SemanticError::kMalformedWire);

    auto wrong_type = base;
    wrong_type.insert(wrong_type.begin(), {0x08U, 0x00U});
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(wrong_type).error, SemanticError::kMalformedWire);

    const std::vector<std::uint8_t> overflow_field{
        0x80U, 0x80U, 0x80U, 0x80U, 0x10U, 0x00U};
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(overflow_field).error, SemanticError::kMalformedWire);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightRejectsNestedSplitReplacementCollectionOverflow) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const auto replacements = repeatedMessageFieldsForTest(2U, 65536U);
    std::vector<std::uint8_t> split;
    appendBytesFieldForTest(split, 1U, {});
    appendBytesFieldForTest(split, 2U, replacements);
    std::vector<std::uint8_t> split_collection;
    appendBytesFieldForTest(split_collection, 1U, split);
    std::vector<std::uint8_t> payload;
    appendBytesFieldForTest(payload, 11U, split_collection);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(operationWithPayloadForTest(payload)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightAllowsKeyedBatchAtLimitAndRejectsAboveLimit) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto make = [](std::size_t count) {
        const auto ids = repeatedMessageFieldsForTest(1U, count);
        std::vector<std::uint8_t> payload;
        appendBytesFieldForTest(payload, 2U, ids);
        return operationWithPayloadForTest(payload);
    };
    EXPECT_NE(SemanticCodec::decodeProtobufOperation(make(65535U)).error,
              SemanticError::kLimitExceeded);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(make(65536U)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightRejectsNestedRemovedMaskCollectionOverflow) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const auto mask_ids = repeatedMessageFieldsForTest(2U, 65536U);
    std::vector<std::uint8_t> item;
    appendBytesFieldForTest(item, 1U, {});
    item.insert(item.end(), mask_ids.begin(), mask_ids.end());
    std::vector<std::uint8_t> items;
    appendBytesFieldForTest(items, 1U, item);
    std::vector<std::uint8_t> payload;
    appendBytesFieldForTest(payload, 13U, items);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(operationWithPayloadForTest(payload)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightRejectsPersistentEraseMaskCollectionOverflow) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const auto erase_masks = repeatedMessageFieldsForTest(8U, 65536U);
    std::vector<std::uint8_t> object;
    object.insert(object.end(), erase_masks.begin(), erase_masks.end());
    std::vector<std::uint8_t> objects;
    appendBytesFieldForTest(objects, 1U, object);
    std::vector<std::uint8_t> payload;
    appendBytesFieldForTest(payload, 1U, objects);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(operationWithPayloadForTest(payload)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightRejectsNestedOrderKeyByteOverflow) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto make = [](std::size_t order_key_size) {
        const std::vector<std::uint8_t> order_key(order_key_size, 0x01U);
        std::vector<std::uint8_t> order_key_message;
        appendBytesFieldForTest(order_key_message, 1U, order_key);
        std::vector<std::uint8_t> placement;
        appendBytesFieldForTest(placement, 2U, order_key_message);
        std::vector<std::uint8_t> object;
        appendBytesFieldForTest(object, 4U, placement);
        std::vector<std::uint8_t> objects;
        appendBytesFieldForTest(objects, 1U, object);
        std::vector<std::uint8_t> payload;
        appendBytesFieldForTest(payload, 1U, objects);
        return operationWithPayloadForTest(payload);
    };
    EXPECT_NE(SemanticCodec::decodeProtobufOperation(make(32U)).error,
              SemanticError::kLimitExceeded);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(make(33U)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightRejectsNestedRichTextRunStringOverflow) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const std::vector<std::uint8_t> oversized_text(1024U * 1024U + 1U, 'x');
    std::vector<std::uint8_t> run;
    appendBytesFieldForTest(run, 1U, oversized_text);
    std::vector<std::uint8_t> paragraph;
    appendBytesFieldForTest(paragraph, 3U, run);
    std::vector<std::uint8_t> document;
    appendBytesFieldForTest(document, 1U, paragraph);
    std::vector<std::uint8_t> rich_text;
    appendBytesFieldForTest(rich_text, 1U, document);
    std::vector<std::uint8_t> object_content;
    appendBytesFieldForTest(object_content, 4U, rich_text);
    std::vector<std::uint8_t> object;
    appendBytesFieldForTest(object, 7U, object_content);
    std::vector<std::uint8_t> objects;
    appendBytesFieldForTest(objects, 1U, object);
    std::vector<std::uint8_t> payload;
    appendBytesFieldForTest(payload, 1U, objects);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(operationWithPayloadForTest(payload)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightAllowsExactRichTextRunStringLimit) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const std::vector<std::uint8_t> exact_text(1024U * 1024U, 'x');
    std::vector<std::uint8_t> run;
    appendBytesFieldForTest(run, 1U, exact_text);
    std::vector<std::uint8_t> paragraph;
    appendBytesFieldForTest(paragraph, 3U, run);
    std::vector<std::uint8_t> document;
    appendBytesFieldForTest(document, 1U, paragraph);
    std::vector<std::uint8_t> rich_text;
    appendBytesFieldForTest(rich_text, 1U, document);
    std::vector<std::uint8_t> object_content;
    appendBytesFieldForTest(object_content, 4U, rich_text);
    std::vector<std::uint8_t> object;
    appendBytesFieldForTest(object, 7U, object_content);
    std::vector<std::uint8_t> objects;
    appendBytesFieldForTest(objects, 1U, object);
    std::vector<std::uint8_t> payload;
    appendBytesFieldForTest(payload, 1U, objects);
    EXPECT_NE(SemanticCodec::decodeProtobufOperation(operationWithPayloadForTest(payload)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightRejectsOversizedEditRichTextInsertBeforeParse) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto make = [](std::size_t text_size) {
        std::vector<std::uint8_t> insert_step;
        appendBytesFieldForTest(insert_step, 3U, std::vector<std::uint8_t>(text_size, 'x'));
        std::vector<std::uint8_t> delta_step;
        appendBytesFieldForTest(delta_step, 1U, insert_step);
        std::vector<std::uint8_t> delta;
        appendBytesFieldForTest(delta, 2U, delta_step);
        std::vector<std::uint8_t> edit;
        appendBytesFieldForTest(edit, 2U, delta);
        std::vector<std::uint8_t> payload;
        appendBytesFieldForTest(payload, 14U, edit);
        return operationWithPayloadForTest(payload);
    };
    EXPECT_NE(SemanticCodec::decodeProtobufOperation(make(1024U * 1024U)).error,
              SemanticError::kLimitExceeded);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(make(1024U * 1024U + 1U)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
}

TEST(SemanticCodec, ProtobufPreflightAllowsExactEraseMaskCollectionLimit) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    auto make = [](std::size_t count) {
        const auto masks = repeatedMessageFieldsForTest(2U, count);
        std::vector<std::uint8_t> item;
        appendBytesFieldForTest(item, 1U, {});
        item.insert(item.end(), masks.begin(), masks.end());
        std::vector<std::uint8_t> items;
        appendBytesFieldForTest(items, 1U, item);
        std::vector<std::uint8_t> payload;
        appendBytesFieldForTest(payload, 12U, items);
        return operationWithPayloadForTest(payload);
    };
    EXPECT_NE(SemanticCodec::decodeProtobufOperation(make(65535U)).error,
              SemanticError::kLimitExceeded);
    EXPECT_EQ(SemanticCodec::decodeProtobufOperation(make(65536U)).error,
              SemanticError::kLimitExceeded);
#else
    GTEST_SKIP() << "Protobuf runtime is unavailable";
#endif
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

TEST(SemanticCodec, RejectsUnpackedDashSegmentsWithCanonicalPackedCategory) {
    const std::vector<std::uint8_t> unpacked_segments{
        0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xf0U, 0x3fU,
        0x09U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x40U,
        0x11U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xe0U, 0x3fU,
    };
    const auto observation = SemanticCodec::observeGoldenFixture("DashPattern", unpacked_segments, true);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    EXPECT_FALSE(observation.accepted);
    EXPECT_EQ(observation.stage, GoldenCodecStage::kWirePreflight);
    EXPECT_EQ(observation.category, "NON_CANONICAL_PACKED_ENCODING");
#else
    EXPECT_EQ(observation.stage, GoldenCodecStage::kRuntimeUnavailable);
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
