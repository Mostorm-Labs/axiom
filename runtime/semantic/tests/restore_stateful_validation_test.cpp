#include "canvas/semantic/restore_planner.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/idempotency.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kShape;
    result.kind_version = 1U;
    result.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    result.content = ShapeContent{1U, 10.0, 20.0};
    return result;
}

ObjectRecord group(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kGroup;
    result.kind_version = 1U;
    result.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    result.content = GroupContent{};
    return result;
}

ObjectRecord sticky(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kSticky;
    result.kind_version = 1U;
    result.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    result.content = StickyContent{10.0, 10.0};
    return result;
}

ObjectRecord richText(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kRichText;
    result.kind_version = 1U;
    result.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    result.content = RichTextContent{};
    return result;
}

ObjectRecord connector(std::uint64_t value, std::uint64_t target) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kConnector;
    result.kind_version = 1U;
    result.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    result.content = ConnectorContent{
        ConnectorEndpoint{FreePointEndpoint{Vec2{0.1, 0.2}}},
        ConnectorEndpoint{AttachedEndpoint{id(target), AutoPerimeterAnchor{}}},
        ConnectorRouting::kStraight};
    return result;
}

class CountingStore final : public ObjectStore {
  public:
    explicit CountingStore(const ObjectStore& delegate) : delegate_(delegate) {}
    std::size_t size() const noexcept override { return delegate_.size(); }
    bool contains(const ObjectId& value) const noexcept override {
        ++contains_calls;
        return delegate_.contains(value);
    }
    const ObjectRecord* find(const ObjectId& value) const noexcept override {
        ++find_calls;
        ++find_by_id[value];
        return delegate_.find(value);
    }
    std::vector<ObjectRecord> allObjects() const override {
        ++all_objects_calls;
        return delegate_.allObjects();
    }
    std::vector<ObjectRecord> children(const std::optional<ObjectId>& parent) const override {
        ++children_calls;
        return delegate_.children(parent);
    }
    mutable std::size_t find_calls = 0U;
    mutable std::size_t contains_calls = 0U;
    mutable std::size_t all_objects_calls = 0U;
    mutable std::size_t children_calls = 0U;
    mutable std::map<ObjectId, std::size_t> find_by_id;
  private:
    const ObjectStore& delegate_;
};

class AppliedOperations final : public AppliedOperationView {
  public:
    std::optional<AppliedOperationEntry> find(const OperationId& value) const override {
        ++lookups;
        return value == entry.canonical_operation.id
                   ? std::optional<AppliedOperationEntry>(entry)
                   : std::nullopt;
    }
    AppliedOperationEntry entry{};
    mutable std::size_t lookups = 0U;
};

Operation restoreOperation(std::uint64_t operation_id, std::uint64_t object_id) {
    Operation result{};
    result.id = OperationId(id(operation_id));
    result.document_id = DocumentId(id(100U));
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = RestoreObjectsOp{{shape(object_id)}};
    return result;
}

template <typename Store>
void insert(Store& store, const ObjectRecord& value) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, value));
}

template <typename Store>
void expectSuccess(Store& store, std::vector<ObjectRecord> objects) {
    const auto before = store.allObjects();
    RestoreObjectsOp restore{objects};
    RestorePlanInputs out;
    out.creates.push_back(shape(999U));
    const auto result = validateRestoreObjects(restore, store, &out);
    EXPECT_EQ(result.issue, StatefulIssue::kNone);
    EXPECT_EQ(out.creates, objects);
    EXPECT_EQ(store.allObjects(), before);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));
    }
}

template <typename Store>
void expectIssue(Store& store, std::vector<ObjectRecord> objects, StatefulIssue issue) {
    const auto before = store.allObjects();
    RestorePlanInputs out;
    out.creates.push_back(shape(998U));
    const auto original = out;
    const auto result = validateRestoreObjects(RestoreObjectsOp{objects}, store, &out);
    EXPECT_EQ(result.issue, issue);
    EXPECT_EQ(out.creates, original.creates);
    EXPECT_EQ(store.allObjects(), before);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(store));
    }
}

template <typename Fn>
void both(Fn&& fn) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    fn(reference);
    fn(indexed);
}

struct Outcome final {
    StatefulIssue issue = StatefulIssue::kNone;
    std::vector<ObjectRecord> creates;
    std::vector<ObjectRecord> projection;
};

template <typename Store>
Outcome runOutcome(Store& store, const RestoreObjectsOp& restore) {
    Outcome outcome;
    outcome.projection = store.allObjects();
    RestorePlanInputs out;
    outcome.issue = validateRestoreObjects(restore, store, &out).issue;
    outcome.creates = out.creates;
    return outcome;
}

TEST(RestoreStatefulValidation, RST_Differential_ReferenceIndexedParity) {
    const RestoreObjectsOp restore{{connector(1U, 2U), shape(2U)}};
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    const Outcome reference_outcome = runOutcome(reference, restore);
    const Outcome indexed_outcome = runOutcome(indexed, restore);
    EXPECT_EQ(reference_outcome.issue, indexed_outcome.issue);
    EXPECT_EQ(reference_outcome.creates, indexed_outcome.creates);
    EXPECT_EQ(reference_outcome.projection, indexed_outcome.projection);
    EXPECT_EQ(reference.allObjects(), reference_outcome.projection);
    EXPECT_EQ(indexed.allObjects(), indexed_outcome.projection);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(RestoreStatefulValidation, RST_B01_B12_StateOwningReferenceIndexedParity) {
    const auto assert_parity = []<typename Seed>(Seed&& seed, const RestoreObjectsOp& restore) {
        ReferenceObjectStore reference;
        IndexedObjectStore indexed;
        seed(reference);
        seed(indexed);
        const auto reference_before = reference.allObjects();
        const auto indexed_before = indexed.allObjects();
        RestorePlanInputs reference_out;
        RestorePlanInputs indexed_out;
        const StatefulResult reference_result =
            validateRestoreObjects(restore, reference, &reference_out);
        const StatefulResult indexed_result = validateRestoreObjects(restore, indexed, &indexed_out);

        EXPECT_EQ(reference_result.issue, indexed_result.issue);
        EXPECT_EQ(reference_out.creates, indexed_out.creates);
        EXPECT_EQ(reference_before, indexed_before);
        EXPECT_EQ(reference.allObjects(), reference_before);
        EXPECT_EQ(indexed.allObjects(), indexed_before);
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
    };
    const auto empty = [](auto&) {};
    const auto existing_same = [](auto& store) { insert(store, shape(1U)); };
    const auto existing_different = [](auto& store) {
        auto value = shape(1U);
        value.transform.tx = 1.0;
        insert(store, value);
    };
    const auto checkpoint = [](auto& store) {
        insert(store, group(100U));
        insert(store, shape(101U, id(100U)));
    };

    assert_parity(empty, RestoreObjectsOp{{shape(1U), shape(2U)}});                       // B01
    assert_parity(existing_same, RestoreObjectsOp{{shape(1U)}});                           // B02
    assert_parity(existing_different, RestoreObjectsOp{{shape(1U)}});                      // B03
    assert_parity(empty, RestoreObjectsOp{{shape(1U, id(2U)), group(2U)}});                // B04
    assert_parity(empty, RestoreObjectsOp{{shape(2U, id(1U))}});                           // B05
    assert_parity(empty, RestoreObjectsOp{{connector(1U, 2U), shape(2U)}});                // B06
    assert_parity(empty, RestoreObjectsOp{{connector(2U, 1U)}});                           // B07
    assert_parity(existing_same, RestoreObjectsOp{{shape(1U)}});                           // B09
    assert_parity(existing_same, RestoreObjectsOp{{shape(1U), shape(2U)}});                // B10
    assert_parity(empty, RestoreObjectsOp{{shape(1U)}});                                  // B11
    assert_parity(checkpoint, RestoreObjectsOp{{shape(1U)}});                             // B12
}

TEST(RestoreStatefulValidation, RST_B01_AllAbsentValidRestore) {
    both([](auto& store) { expectSuccess(store, {shape(1U), shape(2U)}); });
}

TEST(RestoreStatefulValidation, RST_B02_ExistingSameRecordCollides) {
    both([](auto& store) {
        insert(store, shape(1U));
        expectIssue(store, {shape(1U)}, StatefulIssue::kObjectAlreadyExists);
    });
}

TEST(RestoreStatefulValidation, RST_B03_ExistingDifferentRecordCollides) {
    both([](auto& store) {
        auto existing = shape(1U);
        existing.transform.tx = 7.0;
        insert(store, existing);
        expectIssue(store, {shape(1U)}, StatefulIssue::kObjectAlreadyExists);
    });
}

TEST(RestoreStatefulValidation, RST_B04_ParentAndChildSamePayload) {
    // The child has the lower canonical ObjectId, so its parent follows it in
    // payload order. Complete staging makes the resulting graph valid.
    both([](auto& store) { expectSuccess(store, {shape(1U, id(2U)), group(2U)}); });
}

TEST(RestoreStatefulValidation, RST_B05_MissingParentInvalidReference) {
    both([](auto& store) {
        expectIssue(store, {shape(2U, id(1U))}, StatefulIssue::kInvalidReference);
    });
}

TEST(RestoreStatefulValidation, RST_B06_TargetAndConnectorSamePayload) {
    // The Connector has the lower canonical ObjectId; its target follows it.
    both([](auto& store) { expectSuccess(store, {connector(1U, 2U), shape(2U)}); });
}

TEST(RestoreStatefulValidation, RST_B07_ConnectorMissingTargetInvalidReference) {
    both([](auto& store) {
        expectIssue(store, {connector(2U, 1U)}, StatefulIssue::kInvalidReference);
    });
}

TEST(RestoreStatefulValidation, RST_B08_EquivalentReplayStopsAtB1) {
    const Operation operation = restoreOperation(8U, 1U);
    AppliedOperations applied;
    applied.entry.canonical_operation = operation;
    ReferenceObjectStore base;
    CountingStore counting(base);
    bool planner_invoked = false;
    const IdempotencyResult disposition = classifyOperation(operation, applied);
    EXPECT_EQ(disposition.disposition, IdempotencyDisposition::kAlreadyApplied);
    if (disposition.disposition == IdempotencyDisposition::kNew) {
        RestorePlanInputs out;
        static_cast<void>(validateRestoreObjects(
            std::get<RestoreObjectsOp>(operation.payload), counting, &out));
        planner_invoked = true;
    }
    EXPECT_EQ(applied.lookups, 1U);
    EXPECT_FALSE(planner_invoked);
    EXPECT_EQ(counting.find_calls, 0U);
    EXPECT_EQ(counting.contains_calls, 0U);
    EXPECT_EQ(counting.all_objects_calls, 0U);
    EXPECT_EQ(counting.children_calls, 0U);
}

TEST(RestoreStatefulValidation, RST_B09_NewOperationIdExistingCandidateCollides) {
    const Operation prior = restoreOperation(8U, 1U);
    const Operation incoming = restoreOperation(9U, 1U);
    AppliedOperations applied;
    applied.entry.canonical_operation = prior;
    EXPECT_EQ(classifyOperation(incoming, applied).disposition, IdempotencyDisposition::kNew);
    EXPECT_EQ(applied.lookups, 1U);
    both([](auto& store) {
        insert(store, shape(1U));
        expectIssue(store, {shape(1U)}, StatefulIssue::kObjectAlreadyExists);
    });
}

TEST(RestoreStatefulValidation, RST_B10_BatchCollisionIsAtomic) {
    both([](auto& store) {
        insert(store, shape(2U));
        expectIssue(store, {shape(1U), shape(2U), shape(3U)},
                    StatefulIssue::kObjectAlreadyExists);
    });
}

TEST(RestoreStatefulValidation, RST_B11_SourceLabelsDoNotChangeResult) {
    both([](auto& store) {
        RestoreObjectsOp restore{{shape(1U)}};
        std::optional<RestorePlanInputs> baseline;
        for (const auto* label : {"LocalUndo", "Replay", "Remote"}) {
            (void)label;
            RestorePlanInputs out;
            EXPECT_TRUE(validateRestoreObjects(restore, store, &out).ok());
            EXPECT_EQ(out.creates, restore.objects);
            if (!baseline.has_value()) baseline = out;
            EXPECT_EQ(out.creates, baseline->creates);
        }
    });
}

TEST(RestoreStatefulValidation, RST_B12_CheckpointLikeStateWithoutLedger) {
    both([](auto& store) {
        insert(store, group(100U));
        insert(store, shape(101U, id(100U)));
        expectSuccess(store, {shape(1U)});
    });
}

TEST(RestoreStatefulValidation, RST_HCV_01_GroupShape) {
    both([](auto& store) { expectSuccess(store, {shape(1U, id(2U)), group(2U)}); });
}

TEST(RestoreStatefulValidation, RST_HCV_02_GroupNestedGroup) {
    both([](auto& store) { expectSuccess(store, {group(1U, id(2U)), group(2U)}); });
}

TEST(RestoreStatefulValidation, RST_HCV_03_StickyRichText) {
    both([](auto& store) { expectSuccess(store, {richText(1U, id(2U)), sticky(2U)}); });
}

TEST(RestoreStatefulValidation, RST_HCV_04_EmptySticky) {
    both([](auto& store) { expectSuccess(store, {sticky(1U)}); });
}

TEST(RestoreStatefulValidation, RST_HCV_05_StickyShapeRejected) {
    both([](auto& store) {
        expectIssue(store, {shape(1U, id(2U)), sticky(2U)}, StatefulIssue::kInvalidApplicability);
    });
}

TEST(RestoreStatefulValidation, RST_HCV_06_ShapeRichTextRejected) {
    both([](auto& store) {
        expectIssue(store, {richText(1U, id(2U)), shape(2U)}, StatefulIssue::kInvalidApplicability);
    });
}

TEST(RestoreStatefulValidation, RST_HCV_07_StickyTwoRichTextRejected) {
    both([](auto& store) {
        expectIssue(store, {richText(1U, id(3U)), richText(2U, id(3U)), sticky(3U)},
                    StatefulIssue::kInvalidApplicability);
    });
}

TEST(RestoreStatefulValidation, RST_HCV_08_ExistingEmptyStickyAcceptsRichText) {
    both([](auto& store) {
        insert(store, sticky(10U));
        expectSuccess(store, {richText(11U, id(10U))});
    });
}

TEST(RestoreStatefulValidation, RST_HCV_09_ExistingStickyWithRichTextRejectsSecond) {
    both([](auto& store) {
        insert(store, sticky(10U));
        insert(store, richText(11U, id(10U)));
        expectIssue(store, {richText(12U, id(10U))}, StatefulIssue::kInvalidApplicability);
    });
}

TEST(RestoreStatefulValidation, RST_HCV_10_CapabilityRejectionIsAtomic) {
    both([](auto& store) {
        expectIssue(store, {shape(1U, id(2U)), sticky(2U)}, StatefulIssue::kInvalidApplicability);
    });
}

TEST(RestoreStatefulValidation, RST_Performance_OneLookupPerCandidateAndNoScan) {
    ReferenceObjectStore base;
    CountingStore counting(base);
    RestoreObjectsOp restore{{connector(1U, 2U), shape(2U)}};
    RestorePlanInputs out;
    EXPECT_TRUE(validateRestoreObjects(restore, counting, &out).ok());
    EXPECT_EQ(counting.find_calls, 2U);
    EXPECT_EQ(counting.find_by_id.size(), 2U);
    EXPECT_EQ(counting.find_by_id.at(id(1U)), 1U);
    EXPECT_EQ(counting.find_by_id.at(id(2U)), 1U);
    EXPECT_TRUE(counting.find_by_id.contains(id(1U)));
    EXPECT_TRUE(counting.find_by_id.contains(id(2U)));
    EXPECT_EQ(counting.contains_calls, 0U);
    EXPECT_EQ(counting.all_objects_calls, 0U);
}

} // namespace
} // namespace canvas::semantic
