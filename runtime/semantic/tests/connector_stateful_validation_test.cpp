#include "canvas/semantic/connector_validation.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/staged_object_view.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord record(std::uint64_t value, ObjectKind kind, std::uint32_t version = 1U) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = kind;
    result.kind_version = version;
    result.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    switch (kind) {
        case ObjectKind::kShape:
            result.content = ShapeContent{7U, 10.0, 20.0};
            break;
        case ObjectKind::kImage:
            result.content = ImageContent{};
            break;
        case ObjectKind::kVectorPath:
            result.content = VectorPathContent{};
            break;
        case ObjectKind::kRichText:
            result.content = RichTextContent{};
            break;
        case ObjectKind::kVectorStroke:
            result.content = VectorStrokeContent{};
            break;
        case ObjectKind::kDabStroke:
            result.content = DabStrokeContent{};
            break;
        case ObjectKind::kConnector:
            result.content = ConnectorContent{};
            break;
        case ObjectKind::kSticky:
            result.content = StickyContent{10.0, 20.0};
            break;
        case ObjectKind::kGroup:
            result.content = GroupContent{};
            break;
    }
    return result;
}

ConnectorEndpoint freeEndpoint() {
    return ConnectorEndpoint{FreePointEndpoint{Vec2{0.1, 0.2}}};
}

ConnectorEndpoint attachedEndpoint(std::uint64_t target, AnchorRef anchor = AutoPerimeterAnchor{}) {
    return ConnectorEndpoint{AttachedEndpoint{id(target), std::move(anchor)}};
}

ConnectorContent content(ConnectorEndpoint start, ConnectorEndpoint end) {
    return ConnectorContent{std::move(start), std::move(end), ConnectorRouting::kStraight};
}

StatefulIssue independentKindOutcome(ObjectKind kind, std::uint32_t version) {
    const auto numeric_kind = static_cast<std::uint8_t>(kind);
    if (version != 1U || numeric_kind < 1U || numeric_kind > 9U) {
        return StatefulIssue::kInvalidKindVersion;
    }
    switch (kind) {
        case ObjectKind::kShape:
        case ObjectKind::kImage:
        case ObjectKind::kSticky:
            return StatefulIssue::kNone;
        case ObjectKind::kVectorPath:
        case ObjectKind::kRichText:
        case ObjectKind::kVectorStroke:
        case ObjectKind::kDabStroke:
        case ObjectKind::kConnector:
        case ObjectKind::kGroup:
            return StatefulIssue::kConnectorInvalid;
    }
    return StatefulIssue::kInvalidKindVersion;
}

template <typename Store>
void insert(Store& store, const ObjectRecord& value) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, value));
}

TEST(ConnectorStatefulValidation, FreeFreeIsValid) {
    ReferenceObjectStore store;
    StagedObjectView staged(store);

    EXPECT_EQ(validateConnectorReferences(staged, content(freeEndpoint(), freeEndpoint())).issue,
              StatefulIssue::kNone);
}

TEST(ConnectorStatefulValidation, V1ConnectableKindsAcceptAutoPerimeter) {
    const std::array<ObjectKind, 3> kinds = {
        ObjectKind::kShape, ObjectKind::kImage, ObjectKind::kSticky};
    for (std::size_t index = 0; index < kinds.size(); ++index) {
        ReferenceObjectStore store;
        insert(store, record(10U + index, kinds[index]));
        StagedObjectView staged(store);
        EXPECT_EQ(validateConnectorReferences(
                      staged, content(attachedEndpoint(10U + index), freeEndpoint())).issue,
                  StatefulIssue::kNone);
    }
}

TEST(ConnectorStatefulValidation, V1ConnectableKindsAcceptStablePorts1Through4) {
    const std::array<ObjectKind, 3> kinds = {
        ObjectKind::kShape, ObjectKind::kImage, ObjectKind::kSticky};
    for (std::size_t kind_index = 0; kind_index < kinds.size(); ++kind_index) {
        for (std::uint32_t port = 1U; port <= 4U; ++port) {
            ReferenceObjectStore store;
            insert(store, record(20U + kind_index, kinds[kind_index]));
            StagedObjectView staged(store);
            EXPECT_EQ(validateConnectorReferences(
                          staged,
                          content(attachedEndpoint(
                                      20U + kind_index, StablePortAnchor{port}), freeEndpoint()))
                          .issue,
                      StatefulIssue::kNone);
        }
    }
}

TEST(ConnectorStatefulValidation, MissingAttachedTargetReturnsInvalidReference) {
    ReferenceObjectStore store;
    StagedObjectView staged(store);

    EXPECT_EQ(validateConnectorReferences(
                  staged, content(attachedEndpoint(101U), freeEndpoint())).issue,
              StatefulIssue::kInvalidReference);
}

TEST(ConnectorStatefulValidation, StagedDeletedTargetReturnsInvalidReference) {
    ReferenceObjectStore store;
    insert(store, record(102U, ObjectKind::kShape));
    StagedObjectView staged(store);
    ASSERT_TRUE(staged.stageDelete(id(102U)));

    EXPECT_EQ(validateConnectorReferences(
                  staged, content(attachedEndpoint(102U), freeEndpoint())).issue,
              StatefulIssue::kInvalidReference);
}

TEST(ConnectorStatefulValidation, UnsupportedTargetKindVersionReturnsInvalidKindVersion) {
    ReferenceObjectStore store;
    insert(store, record(103U, ObjectKind::kShape, 2U));
    StagedObjectView staged(store);

    EXPECT_EQ(validateConnectorReferences(
                  staged, content(attachedEndpoint(103U), freeEndpoint())).issue,
              StatefulIssue::kInvalidKindVersion);
}

TEST(ConnectorStatefulValidation, V1NonConnectableKindsReturnConnectorInvalid) {
    const std::array<ObjectKind, 6> kinds = {
        ObjectKind::kVectorPath, ObjectKind::kRichText, ObjectKind::kVectorStroke,
        ObjectKind::kDabStroke, ObjectKind::kConnector, ObjectKind::kGroup};
    for (std::size_t index = 0; index < kinds.size(); ++index) {
        ReferenceObjectStore store;
        insert(store, record(110U + index, kinds[index]));
        StagedObjectView staged(store);
        EXPECT_EQ(validateConnectorReferences(
                      staged, content(attachedEndpoint(110U + index), freeEndpoint())).issue,
                  independentKindOutcome(kinds[index], 1U));
        EXPECT_EQ(validateConnectorReferences(
                      staged, content(attachedEndpoint(110U + index), freeEndpoint())).issue,
                  StatefulIssue::kConnectorInvalid);
    }
}

TEST(ConnectorStatefulValidation, SameBatchStagedConnectableTargetIsVisible) {
    const std::array<ObjectKind, 3> kinds = {
        ObjectKind::kShape, ObjectKind::kImage, ObjectKind::kSticky};
    for (std::size_t index = 0; index < kinds.size(); ++index) {
        ReferenceObjectStore store;
        StagedObjectView staged(store);
        ASSERT_TRUE(staged.stageCreate(record(130U + index, kinds[index])));

        EXPECT_EQ(validateConnectorReferences(
                      staged, content(attachedEndpoint(130U + index), freeEndpoint())).issue,
                  StatefulIssue::kNone);
    }
}

TEST(ConnectorStatefulValidation, StartInvalidReferenceWinsOverEndNonConnectable) {
    ReferenceObjectStore store;
    insert(store, record(141U, ObjectKind::kVectorPath));
    StagedObjectView staged(store);

    EXPECT_EQ(validateConnectorReferences(
                  staged, content(attachedEndpoint(140U), attachedEndpoint(141U))).issue,
              StatefulIssue::kInvalidReference);
}

TEST(ConnectorStatefulValidation, StartNonConnectableWinsOverEndInvalidReference) {
    ReferenceObjectStore store;
    insert(store, record(143U, ObjectKind::kVectorPath));
    StagedObjectView staged(store);

    EXPECT_EQ(validateConnectorReferences(
                  staged, content(attachedEndpoint(143U), attachedEndpoint(142U))).issue,
              StatefulIssue::kConnectorInvalid);
}

TEST(ConnectorStatefulValidation, StablePortApplicabilityUsesActualTargetKindVersion) {
    const ObjectRecord shape = record(150U, ObjectKind::kShape);
    EXPECT_EQ(validateStablePortForTarget(shape, StablePortAnchor{1U}).issue,
              StatefulIssue::kNone);

    const ObjectRecord vector_path = record(151U, ObjectKind::kVectorPath);
    EXPECT_EQ(validateStablePortForTarget(vector_path, StablePortAnchor{1U}).issue,
              StatefulIssue::kConnectorInvalid);

    const ObjectRecord shape_v2 = record(152U, ObjectKind::kShape, 2U);
    EXPECT_EQ(validateStablePortForTarget(shape_v2, StablePortAnchor{1U}).issue,
              StatefulIssue::kInvalidKindVersion);
}

TEST(ConnectorStatefulValidation, ConnectableHelperMatchesIndependentV1Matrix) {
    const std::array<ObjectKind, 9> kinds = {
        ObjectKind::kShape, ObjectKind::kImage, ObjectKind::kVectorPath, ObjectKind::kRichText,
        ObjectKind::kVectorStroke, ObjectKind::kDabStroke, ObjectKind::kConnector,
        ObjectKind::kSticky, ObjectKind::kGroup};
    for (const ObjectKind kind : kinds) {
        const bool expected = independentKindOutcome(kind, 1U) == StatefulIssue::kNone;
        EXPECT_EQ(isConnectableObjectKind(kind, 1U), expected);
        EXPECT_FALSE(isConnectableObjectKind(kind, 2U));
    }
    EXPECT_FALSE(isConnectableObjectKind(static_cast<ObjectKind>(99U), 1U));
}

template <typename Store>
StatefulIssue runParityCase(Store& store, ObjectKind kind, std::uint32_t version, std::uint64_t target) {
    if (target != 0U) insert(store, record(target, kind, version));
    StagedObjectView staged(store);
    return validateConnectorReferences(
               staged, content(attachedEndpoint(target == 0U ? 999U : target), freeEndpoint()))
        .issue;
}

TEST(ConnectorStatefulValidation, ReferenceAndIndexedStoresHaveDecisionParity) {
    struct Case final {
        ObjectKind kind;
        std::uint32_t version;
        std::uint64_t target;
        StatefulIssue expected;
    };
    const std::array<Case, 4> cases = {
        Case{ObjectKind::kShape, 1U, 160U, StatefulIssue::kNone},
        Case{ObjectKind::kVectorPath, 1U, 161U, StatefulIssue::kConnectorInvalid},
        Case{ObjectKind::kShape, 2U, 162U, StatefulIssue::kInvalidKindVersion},
        Case{ObjectKind::kShape, 1U, 0U, StatefulIssue::kInvalidReference}};

    for (const Case& test_case : cases) {
        ReferenceObjectStore reference;
        IndexedObjectStore indexed;
        const StatefulIssue reference_issue =
            runParityCase(reference, test_case.kind, test_case.version, test_case.target);
        const StatefulIssue indexed_issue =
            runParityCase(indexed, test_case.kind, test_case.version, test_case.target);
        EXPECT_EQ(reference_issue, test_case.expected);
        EXPECT_EQ(indexed_issue, test_case.expected);
        EXPECT_EQ(reference_issue, indexed_issue);
    }
}

class CountingObjectStore final : public ObjectStore {
  public:
    explicit CountingObjectStore(const ObjectStore& delegate) : delegate_(delegate) {}

    [[nodiscard]] std::size_t size() const noexcept override { return delegate_.size(); }
    [[nodiscard]] bool contains(const ObjectId& value) const noexcept override {
        ++contains_calls;
        return delegate_.contains(value);
    }
    [[nodiscard]] const ObjectRecord* find(const ObjectId& value) const noexcept override {
        ++find_calls;
        return delegate_.find(value);
    }
    [[nodiscard]] std::vector<ObjectRecord> allObjects() const override {
        ++all_objects_calls;
        return delegate_.allObjects();
    }
    [[nodiscard]] std::vector<ObjectRecord> children(
        const std::optional<ObjectId>& parent_id) const override {
        ++children_calls;
        return delegate_.children(parent_id);
    }

    mutable std::size_t contains_calls = 0U;
    mutable std::size_t find_calls = 0U;
    mutable std::size_t all_objects_calls = 0U;
    mutable std::size_t children_calls = 0U;

  private:
    const ObjectStore& delegate_;
};

TEST(ConnectorStatefulValidation, AttachedEndpointsUseAtMostOneStagedLookupAndNoFullScan) {
    ReferenceObjectStore base;
    insert(base, record(170U, ObjectKind::kShape));
    insert(base, record(171U, ObjectKind::kImage));
    CountingObjectStore counting(base);
    StagedObjectView staged(counting);

    const StatefulResult result = validateConnectorReferences(
        staged, content(attachedEndpoint(170U), attachedEndpoint(171U)));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(counting.find_calls, 2U);
    EXPECT_EQ(counting.contains_calls, 0U);
    EXPECT_EQ(counting.all_objects_calls, 0U);
    EXPECT_EQ(counting.children_calls, 0U);

    counting.find_calls = 0U;
    EXPECT_TRUE(validateConnectorReferences(staged, content(freeEndpoint(), freeEndpoint())).ok());
    EXPECT_EQ(counting.find_calls, 0U);
}

template <typename Store, typename IndexCheck>
void expectNoMutation(Store& store, IndexCheck index_check) {
    insert(store, record(180U, ObjectKind::kShape));
    StagedObjectView staged(store);
    const auto base_before = store.allObjects();
    const auto staged_before = staged.projection();
    const bool index_before = index_check(store);

    EXPECT_TRUE(validateConnectorReferences(
                    staged, content(attachedEndpoint(180U, StablePortAnchor{4U}), freeEndpoint()))
                    .ok());
    EXPECT_EQ(store.allObjects(), base_before);
    EXPECT_EQ(staged.projection(), staged_before);
    EXPECT_EQ(index_check(store), index_before);
}

TEST(ConnectorStatefulValidation, EvaluationDoesNotMutateReferenceStore) {
    ReferenceObjectStore store;
    expectNoMutation(store, [](const ReferenceObjectStore&) { return true; });
}

TEST(ConnectorStatefulValidation, EvaluationDoesNotMutateIndexedStoreOrIndex) {
    IndexedObjectStore store;
    expectNoMutation(store, [](const IndexedObjectStore& value) {
        return internal::ObjectStoreMutator::indexMatchesRebuild(value);
    });
}

} // namespace
} // namespace canvas::semantic
