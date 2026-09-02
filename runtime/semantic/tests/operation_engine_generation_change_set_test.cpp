#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord shape(std::uint64_t value, std::uint8_t tag) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{std::nullopt, OrderKey({tag})};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, static_cast<double>(tag), 0.0};
    record.content = ShapeContent{tag, static_cast<double>(tag), static_cast<double>(tag + 1U)};
    return record;
}

Operation operation(OperationPayload payload, std::uint64_t operation_id) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(1000U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

void expectSingleChange(
    const ApplyResult& result,
    SemanticGeneration before,
    SemanticGeneration after,
    ObjectId object_id,
    SemanticChangeFlags flags,
    std::vector<FieldId> fields = {}) {
    ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
    ASSERT_TRUE(result.change_set.has_value());
    EXPECT_EQ(result.change_set->beforeGeneration(), before);
    EXPECT_EQ(result.change_set->afterGeneration(), after);
    ASSERT_EQ(result.change_set->objects().size(), 1U);
    const auto& change = result.change_set->objects().front();
    EXPECT_EQ(change.object_id, object_id);
    EXPECT_EQ(change.flags, flags);
    EXPECT_EQ(change.changed_fields, fields);
}

template <typename Store>
void expectFirstInsertAdvancesGenerationAndBuildsChangeSet() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(7U));
    OperationEngine engine;
    const ObjectRecord created = shape(1U, 7U);
    const ApplyResult result = engine.apply(
        operation(InsertObjectsOp{{created}}, 301U), objects, ledger, generation);

    expectSingleChange(
        result,
        SemanticGeneration(7U),
        SemanticGeneration(8U),
        created.id,
        SemanticChangeFlags::kCreated);
    EXPECT_EQ(generation.current(), SemanticGeneration(8U));
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

} // namespace

TEST(OperationEngineGenerationChangeSet, FirstInsertAdvancesAndCreatesForReferenceAndIndexed) {
    expectFirstInsertAdvancesGenerationAndBuildsChangeSet<ReferenceObjectStore>();
    expectFirstInsertAdvancesGenerationAndBuildsChangeSet<IndexedObjectStore>();
}

TEST(OperationEngineGenerationChangeSet, AlreadyAppliedAndRejectedAreGenerationNeutralAndChangeSetFree) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(4U));
    OperationEngine engine;
    const Operation first = operation(InsertObjectsOp{{shape(2U, 8U)}}, 302U);
    ASSERT_EQ(engine.apply(first, objects, ledger, generation).disposition, ApplyDisposition::kApplied);

    const ApplyResult already = engine.apply(first, objects, ledger, generation);
    EXPECT_EQ(already.disposition, ApplyDisposition::kAlreadyApplied);
    EXPECT_FALSE(already.change_set.has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(5U));

    const Operation collision = operation(DeleteObjectsOp{{id(2U)}}, 302U);
    const ApplyResult rejected = engine.apply(collision, objects, ledger, generation);
    EXPECT_EQ(rejected.disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(rejected.error.issue, StatefulIssue::kOperationIdCollision);
    EXPECT_FALSE(rejected.change_set.has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(5U));

    const Operation missing = operation(
        SetTransformsOp{{TransformItem{id(99U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        303U);
    const ApplyResult state_rejected = engine.apply(missing, objects, ledger, generation);
    EXPECT_EQ(state_rejected.disposition, ApplyDisposition::kRejected);
    EXPECT_FALSE(state_rejected.change_set.has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(5U));
}

TEST(OperationEngineGenerationChangeSet, SequentialAppliedOperationsChainGenerationIntervalsAndFlags) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    OperationEngine engine;
    const ObjectRecord created = shape(3U, 9U);
    const Operation insert = operation(InsertObjectsOp{{created}}, 304U);
    const ApplyResult insert_result = engine.apply(insert, objects, ledger, generation);
    expectSingleChange(
        insert_result,
        SemanticGeneration(0U),
        SemanticGeneration(1U),
        created.id,
        SemanticChangeFlags::kCreated);

    ObjectRecord transformed = created;
    transformed.transform.tx = 99.0;
    const ApplyResult transform_result = engine.apply(
        operation(SetTransformsOp{{TransformItem{created.id, transformed.transform}}}, 305U),
        objects,
        ledger,
        generation);
    expectSingleChange(
        transform_result,
        SemanticGeneration(1U),
        SemanticGeneration(2U),
        created.id,
        SemanticChangeFlags::kTransform);
    EXPECT_EQ(generation.current(), SemanticGeneration(2U));

    const ApplyResult delete_result = engine.apply(
        operation(DeleteObjectsOp{{created.id}}, 306U), objects, ledger, generation);
    expectSingleChange(
        delete_result,
        SemanticGeneration(2U),
        SemanticGeneration(3U),
        created.id,
        SemanticChangeFlags::kDeleted);
}

TEST(OperationEngineGenerationChangeSet, PatchPropertiesCarriesSortedDistinctFieldIds) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    OperationEngine engine;
    const ObjectRecord created = shape(4U, 10U);
    ASSERT_EQ(
        engine.apply(operation(InsertObjectsOp{{created}}, 307U), objects, ledger, generation).disposition,
        ApplyDisposition::kApplied);

    const PatchPropertiesOp patch{
        {{created.id, 2U, PropertyPatchAction::kSet, PropertyValue{true}},
         {created.id, 1U, PropertyPatchAction::kSet, PropertyValue{false}},
         {created.id, 2U, PropertyPatchAction::kSet, PropertyValue{true}}}};
    const ApplyResult result = engine.apply(operation(patch, 308U), objects, ledger, generation);
    expectSingleChange(
        result,
        SemanticGeneration(1U),
        SemanticGeneration(2U),
        created.id,
        SemanticChangeFlags::kProperties,
        {1U, 2U});
}

TEST(OperationEngineGenerationChangeSet, GenerationOverflowFailsBeforeMutation) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation{
        SemanticGeneration{std::numeric_limits<std::uint64_t>::max()}};
    OperationEngine engine;
    const ObjectRecord created = shape(5U, 11U);
    const ApplyResult result = engine.apply(
        operation(InsertObjectsOp{{created}}, 309U), objects, ledger, generation);

    EXPECT_EQ(result.disposition, ApplyDisposition::kRejected);
    EXPECT_FALSE(result.change_set.has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(std::numeric_limits<std::uint64_t>::max()));
    EXPECT_TRUE(objects.allObjects().empty());
    EXPECT_FALSE(ledger.find(OperationId{id(309U)}).has_value());
}

} // namespace canvas::semantic
