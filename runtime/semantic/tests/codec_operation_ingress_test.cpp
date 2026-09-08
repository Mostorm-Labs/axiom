#include "canvas/semantic/codec.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

namespace canvas::semantic {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
static void varint(std::vector<std::uint8_t>& out, std::size_t v) { while (v >= 0x80) { out.push_back(static_cast<std::uint8_t>(v) | 0x80); v >>= 7; } out.push_back(static_cast<std::uint8_t>(v)); }
static void bytesField(std::vector<std::uint8_t>& out, std::uint32_t n, const std::vector<std::uint8_t>& v) { varint(out, (n << 3) | 2); varint(out, v.size()); out.insert(out.end(), v.begin(), v.end()); }
TEST(CodecOperationIngress, DecodesNonEmptyDeleteObjectsPayload) {
    const auto generated = SemanticCodec::encodeProtobufOperation(OperationKind::kDeleteObjects);
    ASSERT_TRUE(generated.ok());
    const auto generated_decoded = SemanticCodec::decodeProtobufOperation(generated.bytes);
    ASSERT_EQ(generated_decoded.error, SemanticError::kNone);
    std::vector<std::uint8_t> operation_id(16, 1), document_id(16, 2), object_id(16, 3);
    std::vector<std::uint8_t> object_id_message; bytesField(object_id_message, 1, object_id);
    std::vector<std::uint8_t> del; bytesField(del, 1, object_id_message);
    std::vector<std::uint8_t> payload; bytesField(payload, 2, del);
    std::vector<std::uint8_t> operation_id_message; bytesField(operation_id_message, 1, operation_id);
    std::vector<std::uint8_t> document_id_message; bytesField(document_id_message, 1, document_id);
    std::vector<std::uint8_t> bytes; bytesField(bytes, 1, operation_id_message); bytesField(bytes, 2, document_id_message); bytes.push_back(0x18); bytes.push_back(1); bytes.push_back(0x20); bytes.push_back(1); bytesField(bytes, 5, payload);
    const auto decoded = SemanticCodec::decodeProtobufOperation(bytes);
    ASSERT_EQ(static_cast<int>(decoded.error), static_cast<int>(SemanticError::kNone)) << static_cast<int>(decoded.error);
    ASSERT_TRUE(std::holds_alternative<DeleteObjectsOp>(decoded.operation.payload));
    EXPECT_EQ(std::get<DeleteObjectsOp>(decoded.operation.payload).object_ids.size(), 1U);
}

#else
TEST(CodecOperationIngress, ProtobufUnavailable) { EXPECT_EQ(SemanticCodec::decodeProtobufOperation({}).error, SemanticError::kRuntimeUnavailable); }
#endif
}
