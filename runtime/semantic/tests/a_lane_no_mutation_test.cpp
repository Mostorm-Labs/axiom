#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/validator.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

namespace canvas::semantic {
namespace {

ObjectRecord sentinel() {
    ObjectRecord object;
    object.id = ObjectId::fromUint64(99U);
    object.kind = ObjectKind::kShape;
    object.kind_version = 1U;
    object.placement.order_key = OrderKey({1U});
    object.content = ShapeContent{1U, 10.0, 20.0};
    return object;
}

TEST(ALane, ValidationAndNormalizationDoNotMutateExternalStore) {
    ReferenceObjectStore store;
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, sentinel()));
    const auto before = store.allObjects();

    Operation accepted;
    accepted.payload = SetVectorPathGeometryOp{
        ObjectId::fromUint64(1U),
        VectorPathGeometry{FillRule::kNonZero, {MoveTo{{0.0, 0.0}}}}};
    const auto normalized = normalizeOperation(accepted);
    ASSERT_TRUE(normalized.ok());
    ASSERT_TRUE(validatePayloadStructure(normalized.value).ok());

    Operation rejected;
    rejected.payload = SetVectorPathGeometryOp{
        ObjectId::fromUint64(1U), VectorPathGeometry{FillRule::kNonZero, {}}};
    EXPECT_FALSE(validatePayloadStructure(rejected).ok());
    EXPECT_EQ(store.allObjects(), before);
}

} // namespace
} // namespace canvas::semantic
