#include "canvas/semantic/codec.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace canvas::semantic {

TEST(SemanticCodecNegative, RejectsTruncatedWire) {
    const std::vector<std::uint8_t> bytes{0x41U, 0x58U, 0x01U};
    EXPECT_EQ(SemanticCodec::decodeOperation(bytes).error, SemanticError::kTruncatedWire);
}

TEST(SemanticCodecNegative, RejectsUnsupportedVersion) {
    const std::vector<std::uint8_t> bytes{0x41U, 0x58U, 0x7fU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    EXPECT_EQ(SemanticCodec::decodeOperation(bytes).error, SemanticError::kUnsupportedVersion);
}

TEST(SemanticCodecNegative, RejectsUnknownOperation) {
    const std::vector<std::uint8_t> bytes{0x41U, 0x58U, 0x01U, 0xffU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    EXPECT_EQ(SemanticCodec::decodeOperation(bytes).error, SemanticError::kUnknownOperation);
}

TEST(SemanticCodecNegative, RejectsDuplicateAndUnorderedCanonicalFields) {
    const std::vector<CanonicalField> duplicate{{2U, {0x01U}}, {2U, {0x02U}}};
    EXPECT_EQ(SemanticCodec::encodeOperation(OperationKind::kPatchProperties, duplicate).error,
              SemanticError::kDuplicateCanonicalKey);
    const std::vector<CanonicalField> unordered{{7U, {0x01U}}, {2U, {0x02U}}};
    EXPECT_EQ(SemanticCodec::encodeOperation(OperationKind::kPatchProperties, unordered).error,
              SemanticError::kNonCanonicalOrder);
}

TEST(SemanticCodecNegative, RejectsNonFiniteAndOversizedValues) {
    EXPECT_EQ(SemanticCodec::encodeCanonicalF64(std::numeric_limits<double>::quiet_NaN()).error,
              SemanticError::kNonFiniteValue);
    EXPECT_EQ(SemanticCodec::encodeCanonicalF64(std::numeric_limits<double>::infinity()).error,
              SemanticError::kNonFiniteValue);
    const std::vector<CanonicalField> oversized{{1U, std::vector<std::uint8_t>(1024U * 1024U + 1U)}};
    EXPECT_EQ(SemanticCodec::encodeOperation(OperationKind::kPatchProperties, oversized).error,
              SemanticError::kLimitExceeded);
}

TEST(SemanticCodecNegative, EnforcesOperationWireSizeBoundaryBeforeDecode) {
    constexpr std::size_t kOperationLimit = 32U * 1024U * 1024U;
    std::vector<std::uint8_t> exact(kOperationLimit, 0U);
    EXPECT_EQ(SemanticCodec::preflightOperationBytes(exact), SemanticError::kNone);
    exact.push_back(0U);
    EXPECT_EQ(SemanticCodec::preflightOperationBytes(exact), SemanticError::kLimitExceeded);
}

TEST(SemanticCodecNegative, EnforcesCustomOperationWireSizeLimit) {
    std::vector<CanonicalField> fields;
    fields.reserve(33U);
    for (std::uint32_t id = 1U; id <= 33U; ++id) {
        fields.push_back(CanonicalField{id, std::vector<std::uint8_t>(1024U * 1024U, 0U)});
    }
    EXPECT_EQ(SemanticCodec::encodeOperation(OperationKind::kPatchProperties, fields).error,
              SemanticError::kLimitExceeded);
}

} // namespace canvas::semantic
