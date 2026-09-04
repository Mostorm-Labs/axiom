#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "object_store_mutator.hpp"
#include "g1_06_projection.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <type_traits>
#include <vector>

namespace canvas::semantic {
namespace {

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

SemanticSnapshot emptySnapshot() {
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId(id(0x42U));
    snapshot.schema_version = 1U;
    return snapshot;
}

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord record;
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    record.content = ShapeContent{1U, 10.0, 20.0};
    return record;
}

ObjectRecord group(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord record;
    record.id = id(value);
    record.kind = ObjectKind::kGroup;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    record.content = GroupContent{};
    return record;
}

ObjectRecord connector(std::uint64_t value, ObjectId target) {
    ObjectRecord record;
    record.id = id(value);
    record.kind = ObjectKind::kConnector;
    record.kind_version = 1U;
    record.placement = Placement{std::nullopt, OrderKey({static_cast<std::uint8_t>(value)})};
    record.content = ConnectorContent{
        ConnectorEndpoint{AttachedEndpoint{target, StablePortAnchor{3U}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{1.0, 2.0}}}, ConnectorRouting::kStraight};
    return record;
}

template <typename Store>
void expectRestored(const SemanticSnapshot& snapshot) {
    Store objects;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    SemanticGenerationState generation(SemanticGeneration(9U));
    const auto before = generation.current();
    const auto result = SnapshotBootstrapper::restore(snapshot, state, objects, generation);
    ASSERT_TRUE(result.restored);
    EXPECT_TRUE(result.semantic_error.ok());
    EXPECT_EQ(state, DocumentRuntimeState::kLoading);
    EXPECT_EQ(generation.current(), before);
    ASSERT_EQ(objects.size(), snapshot.objects.size());
}

canvas::verification::g1_06::ProjectionDocumentId projectionDocumentId() {
    canvas::verification::g1_06::ProjectionDocumentId result;
    result.bytes[0] = 0x42U;
    return result;
}

template <typename Store>
void expectLoadingFailure(const SemanticSnapshot& snapshot, StatefulIssue issue) {
    Store objects;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    SemanticGenerationState generation(SemanticGeneration(12U));
    const auto before = generation.current();
    const auto result = SnapshotBootstrapper::restore(snapshot, state, objects, generation);
    EXPECT_FALSE(result.restored);
    EXPECT_EQ(result.semantic_error.issue, issue);
    EXPECT_EQ(state, DocumentRuntimeState::kFailed);
    EXPECT_EQ(objects.size(), 0U);
    EXPECT_EQ(generation.current(), before);
}

template <typename Store, typename Seed>
void expectLoadingFailureAtomically(const SemanticSnapshot& snapshot, Seed seed) {
    Store objects;
    seed(objects);
    const auto before_objects = objects.allObjects();
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    SemanticGenerationState generation(SemanticGeneration(12U));
    const auto before_generation = generation.current();

    const auto result = SnapshotBootstrapper::restore(snapshot, state, objects, generation);

    EXPECT_FALSE(result.restored);
    EXPECT_FALSE(result.semantic_error.ok());
    EXPECT_EQ(state, DocumentRuntimeState::kFailed);
    EXPECT_EQ(objects.allObjects(), before_objects);
    EXPECT_EQ(generation.current(), before_generation);
}

template <typename Seed>
void expectLoadingFailureOnBothProviders(const SemanticSnapshot& snapshot, Seed seed) {
    expectLoadingFailureAtomically<ReferenceObjectStore>(snapshot, seed);
    expectLoadingFailureAtomically<IndexedObjectStore>(snapshot, seed);
}

struct EmptyTargetSeed {
    template <typename Store>
    void operator()(Store&) const {}
};

struct NonEmptyTargetSeed {
    template <typename Store>
    void operator()(Store& objects) const {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(objects, shape(99U)));
    }
};

TEST(G106SnapshotBootstrap, EmptySnapshotRestoresWhileLoadingWithoutAdvancingGeneration) {
    ReferenceObjectStore objects;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    SemanticGenerationState generation(SemanticGeneration(17U));
    const SemanticGeneration before = generation.current();

    const SnapshotBootstrapResult result =
        SnapshotBootstrapper::restore(emptySnapshot(), state, objects, generation);

    EXPECT_TRUE(result.restored);
    EXPECT_EQ(result.decode_error, SemanticError::kNone);
    EXPECT_TRUE(result.semantic_error.ok());
    EXPECT_EQ(state, DocumentRuntimeState::kLoading);
    EXPECT_EQ(objects.size(), 0U);
    EXPECT_EQ(generation.current(), before);
}

TEST(G106SnapshotBootstrap, RestoresCanonicalReferenceAndIndexedCandidates) {
    SemanticSnapshot snapshot = emptySnapshot();
    snapshot.objects = {shape(2U), shape(1U)};
    expectRestored<ReferenceObjectStore>(snapshot);
    expectRestored<IndexedObjectStore>(snapshot);
}

TEST(G106SnapshotBootstrap, ReferenceAndIndexedProjectionMatchesCandidateProjection) {
    SemanticSnapshot snapshot = emptySnapshot();
    snapshot.objects = {shape(3U), shape(1U), shape(2U)};

    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    DocumentRuntimeState reference_state = DocumentRuntimeState::kLoading;
    DocumentRuntimeState indexed_state = DocumentRuntimeState::kLoading;
    SemanticGenerationState reference_generation;
    SemanticGenerationState indexed_generation;
    ASSERT_TRUE(SnapshotBootstrapper::restore(snapshot, reference_state, reference, reference_generation).restored);
    ASSERT_TRUE(SnapshotBootstrapper::restore(snapshot, indexed_state, indexed, indexed_generation).restored);

    const auto expected = canvas::verification::g1_06::projectDocument(
        projectionDocumentId(), snapshot.schema_version, reference);
    auto candidate_objects = snapshot.objects;
    std::sort(candidate_objects.begin(), candidate_objects.end(),
              [](const ObjectRecord& left, const ObjectRecord& right) { return left.id < right.id; });
    EXPECT_EQ(expected.objects, candidate_objects);
    EXPECT_EQ(expected,
              canvas::verification::g1_06::projectDocument(
                  projectionDocumentId(), snapshot.schema_version, indexed));
    EXPECT_EQ(canvas::verification::g1_06::writeCanonicalProjectionJson(expected),
              canvas::verification::g1_06::writeCanonicalProjectionJson(
                  canvas::verification::g1_06::projectDocument(
                      projectionDocumentId(), snapshot.schema_version, indexed)));
}

TEST(G106SnapshotBootstrap, RestoresHierarchyAndConnectorGraph) {
    SemanticSnapshot snapshot = emptySnapshot();
    snapshot.objects = {connector(3U, id(2U)), shape(2U, id(1U)), group(1U)};
    expectRestored<ReferenceObjectStore>(snapshot);
    expectRestored<IndexedObjectStore>(snapshot);
}

TEST(G106SnapshotBootstrap, RejectsNonLoadingStatesWithoutChangingAnything) {
    const std::array<DocumentRuntimeState, 6> states = {
        DocumentRuntimeState::kConstructed, DocumentRuntimeState::kReady,
        DocumentRuntimeState::kSuspended, DocumentRuntimeState::kClosing,
        DocumentRuntimeState::kClosed, DocumentRuntimeState::kFailed};
    for (const auto initial : states) {
        ReferenceObjectStore objects;
        DocumentRuntimeState state = initial;
        SemanticGenerationState generation(SemanticGeneration(4U));
        const auto result = SnapshotBootstrapper::restore(emptySnapshot(), state, objects, generation);
        EXPECT_FALSE(result.restored);
        EXPECT_EQ(result.semantic_error.issue, StatefulIssue::kInvalidApplicability);
        EXPECT_EQ(state, initial);
        EXPECT_EQ(objects.size(), 0U);
        EXPECT_EQ(generation.current(), SemanticGeneration(4U));
    }
}

TEST(G106SnapshotBootstrap, RejectsNonEmptyTargetAndLeavesItUnchanged) {
    ReferenceObjectStore objects;
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(objects, shape(99U)));
    const auto before = objects.allObjects();
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    SemanticGenerationState generation;
    const auto result = SnapshotBootstrapper::restore(emptySnapshot(), state, objects, generation);
    EXPECT_FALSE(result.restored);
    EXPECT_EQ(state, DocumentRuntimeState::kFailed);
    EXPECT_EQ(objects.allObjects(), before);
}

TEST(G106SnapshotBootstrap, RejectsInvalidCandidateBeforeMutation) {
    SemanticSnapshot snapshot = emptySnapshot();
    auto invalid = shape(1U);
    invalid.kind_version = 2U;
    snapshot.objects = {invalid};
    expectLoadingFailure<ReferenceObjectStore>(snapshot, StatefulIssue::kInvalidApplicability);
    expectLoadingFailure<IndexedObjectStore>(snapshot, StatefulIssue::kInvalidApplicability);
}

TEST(G106SnapshotBootstrap, RejectsDuplicateAndBrokenHierarchyBeforeMutation) {
    SemanticSnapshot duplicate = emptySnapshot();
    duplicate.objects = {shape(1U), shape(1U)};
    expectLoadingFailure<ReferenceObjectStore>(duplicate, StatefulIssue::kObjectAlreadyExists);

    SemanticSnapshot missing_parent = emptySnapshot();
    missing_parent.objects = {shape(2U, id(77U))};
    expectLoadingFailure<ReferenceObjectStore>(missing_parent, StatefulIssue::kInvalidReference);

    SemanticSnapshot cycle = emptySnapshot();
    cycle.objects = {shape(1U, id(2U)), shape(2U, id(1U))};
    expectLoadingFailure<IndexedObjectStore>(cycle, StatefulIssue::kHierarchyCycle);
}

TEST(G106SnapshotBootstrap, RejectsInvalidParentCapabilityAndConnectorReferences) {
    SemanticSnapshot bad_parent = emptySnapshot();
    bad_parent.objects = {shape(2U, id(1U)), connector(1U, id(2U))};
    expectLoadingFailure<ReferenceObjectStore>(bad_parent, StatefulIssue::kInvalidApplicability);

    SemanticSnapshot missing_target = emptySnapshot();
    missing_target.objects = {connector(1U, id(77U))};
    expectLoadingFailure<IndexedObjectStore>(missing_target, StatefulIssue::kInvalidReference);

    SemanticSnapshot non_connectable = emptySnapshot();
    non_connectable.objects = {group(2U), connector(1U, id(2U))};
    expectLoadingFailure<ReferenceObjectStore>(non_connectable, StatefulIssue::kConnectorInvalid);

    SemanticSnapshot invalid_port = emptySnapshot();
    invalid_port.objects = {shape(2U), connector(1U, id(2U))};
    auto& connector_content = std::get<ConnectorContent>(invalid_port.objects.back().content);
    connector_content.start = ConnectorEndpoint{
        AttachedEndpoint{id(2U), StablePortAnchor{0U}}};
    expectLoadingFailure<IndexedObjectStore>(invalid_port, StatefulIssue::kInvalidApplicability);
}

TEST(G106SnapshotBootstrap, RejectsZeroDocumentAndUnsupportedSchema) {
    SemanticSnapshot zero = emptySnapshot();
    zero.document_id = DocumentId{};
    expectLoadingFailure<ReferenceObjectStore>(zero, StatefulIssue::kInvalidApplicability);

    SemanticSnapshot version = emptySnapshot();
    version.schema_version = 2U;
    expectLoadingFailure<IndexedObjectStore>(version, StatefulIssue::kInvalidApplicability);
}

TEST(G106SnapshotBootstrap, NegativeFixturesRejectAtomicallyOnReferenceAndIndexedProviders) {
    const EmptyTargetSeed empty_target;
    const NonEmptyTargetSeed non_empty_target;

    expectLoadingFailureOnBothProviders(emptySnapshot(), non_empty_target);

    SemanticSnapshot invalid_record = emptySnapshot();
    auto invalid = shape(1U);
    invalid.kind_version = 2U;
    invalid_record.objects = {invalid};
    expectLoadingFailureOnBothProviders(invalid_record, empty_target);

    SemanticSnapshot duplicate = emptySnapshot();
    duplicate.objects = {shape(1U), shape(1U)};
    expectLoadingFailureOnBothProviders(duplicate, empty_target);

    SemanticSnapshot missing_parent = emptySnapshot();
    missing_parent.objects = {shape(2U, id(77U))};
    expectLoadingFailureOnBothProviders(missing_parent, empty_target);

    SemanticSnapshot cycle = emptySnapshot();
    cycle.objects = {shape(1U, id(2U)), shape(2U, id(1U))};
    expectLoadingFailureOnBothProviders(cycle, empty_target);

    SemanticSnapshot invalid_parent_capability = emptySnapshot();
    invalid_parent_capability.objects = {shape(2U, id(1U)), connector(1U, id(2U))};
    expectLoadingFailureOnBothProviders(invalid_parent_capability, empty_target);

    SemanticSnapshot connector_target_missing = emptySnapshot();
    connector_target_missing.objects = {connector(1U, id(77U))};
    expectLoadingFailureOnBothProviders(connector_target_missing, empty_target);

    SemanticSnapshot connector_target_non_connectable = emptySnapshot();
    connector_target_non_connectable.objects = {group(2U), connector(1U, id(2U))};
    expectLoadingFailureOnBothProviders(connector_target_non_connectable, empty_target);

    SemanticSnapshot invalid_stable_port = emptySnapshot();
    invalid_stable_port.objects = {shape(2U), connector(1U, id(2U))};
    auto& connector_content = std::get<ConnectorContent>(invalid_stable_port.objects.back().content);
    connector_content.start = ConnectorEndpoint{
        AttachedEndpoint{id(2U), StablePortAnchor{0U}}};
    expectLoadingFailureOnBothProviders(invalid_stable_port, empty_target);

    SemanticSnapshot zero_document_id = emptySnapshot();
    zero_document_id.document_id = DocumentId{};
    expectLoadingFailureOnBothProviders(zero_document_id, empty_target);

    SemanticSnapshot unsupported_schema = emptySnapshot();
    unsupported_schema.schema_version = 2U;
    expectLoadingFailureOnBothProviders(unsupported_schema, empty_target);
}

} // namespace
} // namespace canvas::semantic
