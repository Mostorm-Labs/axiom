#include "canvas/semantic/validator.hpp"

#include <gtest/gtest.h>

namespace canvas::semantic {
namespace {

ObjectRecord shape(std::uint32_t version, std::uint32_t kind, std::uint64_t value) {
    ObjectRecord object{};
    object.id = ObjectId::fromUint64(value);
    object.kind = ObjectKind::kShape;
    object.kind_version = version;
    object.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    object.content = ShapeContent{kind, 100.0, 80.0};
    return object;
}

Operation insert(ObjectRecord object) {
    Operation operation{};
    operation.id = OperationId{ObjectId::fromUint64(900U)};
    operation.document_id = DocumentId(ObjectId::fromUint64(901U));
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = InsertObjectsOp{{std::move(object)}};
    return operation;
}

TEST(FC01Shape2Validation, AdmitsOnlyVersionedTriangleTuple) {
    EXPECT_TRUE(validatePayloadStructure(insert(shape(2U, 3U, 1U))).ok());
    EXPECT_FALSE(validatePayloadStructure(insert(shape(1U, 3U, 2U))).ok());
    EXPECT_FALSE(validatePayloadStructure(insert(shape(2U, 1U, 3U))).ok());
    EXPECT_FALSE(validatePayloadStructure(insert(shape(2U, 2U, 4U))).ok());
}

TEST(FC01Shape2Validation, KeepsLegacyShapeKindsValid) {
    EXPECT_TRUE(validatePayloadStructure(insert(shape(1U, 1U, 5U))).ok());
    EXPECT_TRUE(validatePayloadStructure(insert(shape(1U, 2U, 6U))).ok());
}

}  // namespace
}  // namespace canvas::semantic
