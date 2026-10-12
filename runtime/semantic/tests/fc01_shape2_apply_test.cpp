#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include <gtest/gtest.h>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord shape(std::uint32_t version, std::uint32_t kind, std::uint64_t value) {
    ObjectRecord object{};
    object.id = id(value);
    object.kind = ObjectKind::kShape;
    object.kind_version = version;
    object.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    object.content = ShapeContent{kind, 100.0, 80.0};
    return object;
}

Operation insert(ObjectRecord object, std::uint64_t operation_id) {
    Operation operation{};
    operation.id = OperationId{id(operation_id)};
    operation.document_id = DocumentId{id(1000U)};
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = InsertObjectsOp{{std::move(object)}};
    return operation;
}

template <typename Store>
void expectTriangleAdmission() {
    Store objects;
    AppliedOperationLedger ledger;
    OperationEngine engine;
    const auto operation = insert(shape(2U, 3U, 1U), 10U);
    const auto prepared = engine.prepare(operation, StatefulValidationContext{objects, ledger});
    ASSERT_EQ(prepared.disposition, PrepareDisposition::kPrepared);
    ASSERT_TRUE(prepared.plan.has_value());
    EXPECT_EQ(prepared.plan->creates.size(), 1U);
}

template <typename Store>
void expectReject(ObjectRecord object, std::uint64_t operation_id) {
    Store objects;
    AppliedOperationLedger ledger;
    OperationEngine engine;
    const auto prepared = engine.prepare(insert(std::move(object), operation_id),
                                         StatefulValidationContext{objects, ledger});
    EXPECT_EQ(prepared.disposition, PrepareDisposition::kRejected);
    EXPECT_FALSE(prepared.plan.has_value());
    EXPECT_EQ(objects.size(), 0U);
}

TEST(FC01Shape2Apply, TypedInsertUsesUniversalTupleValidation) {
    expectTriangleAdmission<ReferenceObjectStore>();
    expectTriangleAdmission<IndexedObjectStore>();
    expectReject<ReferenceObjectStore>(shape(2U, 1U, 2U), 11U);
    expectReject<IndexedObjectStore>(shape(2U, 2U, 3U), 12U);
    expectReject<ReferenceObjectStore>(shape(1U, 3U, 4U), 13U);
    expectReject<IndexedObjectStore>(shape(1U, 3U, 5U), 14U);
}

}  // namespace
}  // namespace canvas::semantic
