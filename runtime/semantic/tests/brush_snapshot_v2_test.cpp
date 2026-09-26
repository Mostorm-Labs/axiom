#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/brush_engine_codec.hpp"

#include <gtest/gtest.h>

namespace canvas::semantic {
namespace {
ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord brushObject() {
    ObjectRecord object;
    object.id = id(10);
    object.kind = ObjectKind::kVectorStroke;
    object.kind_version = 2;
    object.placement.order_key = OrderKey({1});
    BrushStrokeContent content;
    content.stroke.snapshot.snapshot_version = 2;
    content.stroke.snapshot.package_id = id(20);
    content.stroke.snapshot.profile_id = 1;
    content.stroke.snapshot.seed = 42;
    content.stroke.confirmed_samples.push_back({{0, 0}, 0.5});
    content.stroke.vector_output.outline = {{0, 0}, {1, 0}, {1, 1}};
    object.content = std::move(content);
    return object;
}
}

TEST(BrushSnapshotV2, NewObjectIsRecognizedBeforeMutation) {
    const auto object = brushObject();
    EXPECT_TRUE(BrushEngineCodec::acceptsNewObject(object));
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId{id(30)};
    snapshot.schema_version = 2;
    snapshot.objects = {object};
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const auto encoded = SnapshotCodec::encode(snapshot);
    ASSERT_TRUE(encoded.ok());
    const auto decoded = SnapshotCodec::decode(encoded.bytes);
    ASSERT_TRUE(decoded.ok());
    ASSERT_EQ(decoded.snapshot->objects.size(), 1U);
    EXPECT_EQ(decoded.snapshot->objects.front(), object);
#else
    GTEST_SKIP() << "protobuf disabled";
#endif
}

}  // namespace canvas::semantic
