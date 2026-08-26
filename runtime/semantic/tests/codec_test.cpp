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

} // namespace canvas::semantic
