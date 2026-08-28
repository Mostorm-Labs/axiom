#include "canvas/semantic/hierarchy_validation.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/staged_object_view.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <optional>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectRecord rec(std::uint64_t id, std::optional<ObjectId> parent = std::nullopt,
                 std::vector<std::uint8_t> order = {1U}) {
    ObjectRecord r{};
    r.id = ObjectId::fromUint64(id);
    r.kind = ObjectKind::kShape;
    r.kind_version = 1U;
    r.placement = Placement{parent, OrderKey(std::move(order))};
    r.content = ShapeContent{7U, static_cast<double>(id), 10.0};
    return r;
}

template <typename Store>
void insert(Store& store, const ObjectRecord& value) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, value));
}

TEST(HierarchyValidation, RootAndExistingParentValid) {
    ReferenceObjectStore base;
    insert(base, rec(1));
    StagedObjectView staged(base);
    EXPECT_TRUE(validateStagedHierarchy(staged, std::vector<HierarchyEdit>{{ObjectId::fromUint64(1), Placement{std::nullopt, OrderKey({1U})}}}).ok());
}

TEST(HierarchyValidation, MissingObjectsAndParents) {
    ReferenceObjectStore base;
    insert(base, rec(1));
    StagedObjectView staged(base);
    EXPECT_EQ(validateStagedHierarchy(staged, std::vector<HierarchyEdit>{{ObjectId::fromUint64(9), Placement{std::nullopt, OrderKey({1U})}}}).issue, StatefulIssue::kObjectMissing);
    EXPECT_EQ(validateStagedHierarchy(staged, std::vector<HierarchyEdit>{{ObjectId::fromUint64(1), Placement{ObjectId::fromUint64(9), OrderKey({1U})}}}).issue, StatefulIssue::kInvalidReference);
    ASSERT_TRUE(staged.stageDelete(ObjectId::fromUint64(1)));
    EXPECT_EQ(validateStagedHierarchy(staged, std::vector<HierarchyEdit>{{ObjectId::fromUint64(1), Placement{std::nullopt, OrderKey({1U})}}}).issue, StatefulIssue::kObjectMissing);
}

TEST(HierarchyValidation, DetectsSelfAndSimultaneousCycles) {
    ReferenceObjectStore base;
    insert(base, rec(1)); insert(base, rec(2, ObjectId::fromUint64(1)));
    StagedObjectView staged(base);
    EXPECT_EQ(validateStagedHierarchy(staged, std::vector<HierarchyEdit>{{ObjectId::fromUint64(1), Placement{ObjectId::fromUint64(1), OrderKey({1U})}}}).issue, StatefulIssue::kHierarchyCycle);
    std::vector<HierarchyEdit> edits{{ObjectId::fromUint64(1), Placement{ObjectId::fromUint64(2), OrderKey({1U})}}, {ObjectId::fromUint64(2), Placement{ObjectId::fromUint64(1), OrderKey({1U})}}};
    EXPECT_EQ(validateStagedHierarchy(staged, edits).issue, StatefulIssue::kHierarchyCycle);
    std::reverse(edits.begin(), edits.end());
    EXPECT_EQ(validateStagedHierarchy(staged, edits).issue, StatefulIssue::kHierarchyCycle);
}

TEST(HierarchyValidation, DescendantsAreDeterministicAndStrict) {
    ReferenceObjectStore base;
    insert(base, rec(1)); insert(base, rec(2, ObjectId::fromUint64(1), {2U})); insert(base, rec(3, ObjectId::fromUint64(1), {1U})); insert(base, rec(4, ObjectId::fromUint64(3), {1U}));
    StagedObjectView staged(base);
    const auto descendants = resolveDescendants(staged, ObjectId::fromUint64(1));
    ASSERT_EQ(descendants.size(), 3U);
    EXPECT_EQ(descendants, (std::vector<ObjectId>{ObjectId::fromUint64(2), ObjectId::fromUint64(3), ObjectId::fromUint64(4)}));
    EXPECT_EQ(std::find(descendants.begin(), descendants.end(), ObjectId::fromUint64(1)), descendants.end());
}

TEST(HierarchyValidation, ReferenceAndIndexedParityAndNoMutation) {
    ReferenceObjectStore reference; IndexedObjectStore indexed;
    for (const auto& r : {rec(1), rec(2, ObjectId::fromUint64(1))}) { insert(reference, r); insert(indexed, r); }
    const auto before = reference.allObjects();
    StagedObjectView a(reference), b(indexed);
    const std::vector<HierarchyEdit> edits{{ObjectId::fromUint64(2), Placement{std::nullopt, OrderKey({3U})}}};
    EXPECT_EQ(validateStagedHierarchy(a, edits).issue, validateStagedHierarchy(b, edits).issue);
    EXPECT_EQ(reference.allObjects(), before);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(HierarchyValidation, StagedCreatedParentChildAndDeletedParent) {
    ReferenceObjectStore base; insert(base, rec(1)); StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(rec(2, ObjectId::fromUint64(1)))); ASSERT_TRUE(staged.stageCreate(rec(3, ObjectId::fromUint64(2))));
    EXPECT_TRUE(validateStagedHierarchy(staged, std::vector<HierarchyEdit>{{ObjectId::fromUint64(3), Placement{ObjectId::fromUint64(2), OrderKey({1U})}}}).ok());
    ASSERT_TRUE(staged.stageDelete(ObjectId::fromUint64(1)));
    EXPECT_EQ(validateStagedHierarchy(staged, std::vector<HierarchyEdit>{{ObjectId::fromUint64(2), Placement{ObjectId::fromUint64(1), OrderKey({1U})}}}).issue, StatefulIssue::kObjectMissing);
}

TEST(HierarchyValidation, ThreeNodeAndUntouchedAncestorCycles) {
    ReferenceObjectStore base; insert(base, rec(1)); insert(base, rec(2, ObjectId::fromUint64(1))); insert(base, rec(3, ObjectId::fromUint64(2))); insert(base, rec(4, ObjectId::fromUint64(3)));
    StagedObjectView staged(base);
    std::vector<HierarchyEdit> tri{{ObjectId::fromUint64(1), Placement{ObjectId::fromUint64(3), OrderKey({1U})}}, {ObjectId::fromUint64(2), Placement{ObjectId::fromUint64(1), OrderKey({1U})}}, {ObjectId::fromUint64(3), Placement{ObjectId::fromUint64(2), OrderKey({1U})}}};
    EXPECT_EQ(validateStagedHierarchy(staged, tri).issue, StatefulIssue::kHierarchyCycle);
    EXPECT_EQ(validateStagedHierarchy(staged, std::vector<HierarchyEdit>{{ObjectId::fromUint64(1), Placement{ObjectId::fromUint64(4), OrderKey({1U})}}}).issue, StatefulIssue::kHierarchyCycle);
}

} // namespace
} // namespace canvas::semantic
