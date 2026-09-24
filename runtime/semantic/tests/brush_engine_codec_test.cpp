#include "canvas/semantic/brush_engine_codec.hpp"

#include <gtest/gtest.h>

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

}  // namespace canvas::semantic
