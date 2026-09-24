#include "canvas/semantic/brush_engine_codec.hpp"

#include <gtest/gtest.h>
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "canvas/semantic/codec.hpp"
#endif

namespace canvas::semantic {

TEST(BrushEngineCodec, AcceptsOnlyVersionedNewObjectBranch) {
    ObjectRecord object{};
    object.kind = ObjectKind::kVectorStroke;
    object.kind_version = 2U;
    object.content = BrushStrokeContent{};
    EXPECT_TRUE(BrushEngineCodec::acceptsNewObject(object));

    object.kind_version = 1U;
    object.content = VectorStrokeContent{};
    EXPECT_FALSE(BrushEngineCodec::acceptsNewObject(object));
    EXPECT_TRUE(BrushEngineCodec::rejectsLegacyObjectAsNewAuthoring(object));
}

TEST(BrushEngineCodec, AddStrokeRequiresPayloadVersionTwo) {
    ObjectRecord object{};
    object.kind = ObjectKind::kVectorStroke;
    object.kind_version = 2U;
    object.content = BrushStrokeContent{};
    Operation operation{};
    operation.schema_version = 1U;
    operation.payload_version = 2U;
    operation.payload = AddStrokeOp{object};
    EXPECT_TRUE(BrushEngineCodec::acceptsNewAddStroke(operation));
    operation.payload_version = 1U;
    EXPECT_FALSE(BrushEngineCodec::acceptsNewAddStroke(operation));
}

TEST(BrushEngineCodec, ProtobufAddStrokeV2RoundTripsBeforeMutation) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    ObjectRecord object{};
    object.id = ObjectId::fromUint64(10);
    object.kind = ObjectKind::kVectorStroke;
    object.kind_version = 2U;
    object.placement.order_key = OrderKey({1});
    BrushStrokeContent content;
    content.stroke.snapshot.snapshot_version = 2U;
    content.stroke.snapshot.package_id = ObjectId::fromUint64(20);
    content.stroke.snapshot.profile_id = 1U;
    content.stroke.confirmed_samples.push_back({{0.0, 0.0}, 0.5});
    content.stroke.vector_output.outline = {{0.0, 0.0}, {1.0, 1.0}};
    object.content = content;
    Operation operation{};
    operation.id = OperationId{ObjectId::fromUint64(30)};
    operation.document_id = DocumentId{ObjectId::fromUint64(40)};
    operation.schema_version = 1U;
    operation.payload_version = 2U;
    operation.payload = AddStrokeOp{object};
    const auto encoded = SemanticCodec::encodeProtobufOperation(operation);
    ASSERT_TRUE(encoded.ok());
    const auto decoded = SemanticCodec::decodeProtobufOperation(encoded.bytes);
    ASSERT_TRUE(decoded.ok());
    EXPECT_TRUE(BrushEngineCodec::acceptsNewAddStroke(decoded.operation));
    EXPECT_EQ(decoded.operation.payload_version, 2U);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

}  // namespace canvas::semantic
