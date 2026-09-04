#include "g1_06_projection.hpp"
#include "g1_06_digest.hpp"

#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/replay.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::verification::g1_06 {

namespace {

using namespace canvas::semantic;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ProjectionDocumentId documentId() {
    ProjectionDocumentId result;
    result.bytes[0] = 0x42U;
    result.bytes[15] = 0x24U;
    return result;
}

ObjectRecord group(std::uint64_t value) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kGroup;
    record.kind_version = 1U;
    record.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    record.content = GroupContent{};
    return record;
}

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 2.0, 3.0};
    record.content = ShapeContent{1U, 10.0, 20.0};
    return record;
}

std::vector<ObjectRecord> fixtureObjects() {
    const ObjectRecord root = group(1U);
    return {shape(3U, root.id), root, shape(2U)};
}

template <typename Store>
void insertAll(Store& store, const std::vector<ObjectRecord>& records) {
    for (const ObjectRecord& record : records) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
    }
}

template <typename Store>
std::string projectionOf(const Store& store) {
    return writeCanonicalProjectionJson(projectDocument(documentId(), 1U, store));
}

std::uint64_t independentFnv1a64(std::string_view bytes) {
    std::uint64_t result = 14695981039346656037ULL;
    for (const unsigned char byte : bytes) {
        result ^= static_cast<std::uint64_t>(byte);
        result *= 1099511628211ULL;
    }
    return result;
}

Operation operation(OperationPayload payload, std::uint64_t operation_id) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(0x900U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

template <typename Store>
void applyOperation(Store& store, OperationPayload payload, std::uint64_t operation_id) {
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(1U));
    const auto result = OperationEngine{}.apply(
        operation(std::move(payload), operation_id), ApplySource::kLocalInteraction,
        store, ledger, generation, clock);
    ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
}

TEST(G106Digest, Fnv1aUsesExactBytesAndIndependentReference) {
    constexpr std::string_view input{"axiom\0digest", 12U};
    EXPECT_EQ(digestCanonicalProjectionBytes(input), independentFnv1a64(input));
}

TEST(G106Digest, RepeatedIdenticalInputIsDeterministic) {
    const std::string input = "{\"canonical\":true}\n";
    EXPECT_EQ(digestCanonicalProjectionBytes(input), digestCanonicalProjectionBytes(input));
}

TEST(G106Digest, EmptyDocumentProjectionHasDeterministicDigest) {
    ReferenceObjectStore store;
    const std::string projection = projectionOf(store);
    EXPECT_EQ(digestCanonicalProjectionBytes(projection), independentFnv1a64(projection));
    EXPECT_EQ(digestCanonicalProjectionBytes(projection), digestCanonicalProjectionBytes(projection));
}

TEST(G106Digest, ReferenceAndIndexedHaveProjectionThenDigestParity) {
    const auto records = fixtureObjects();
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    insertAll(reference, records);
    insertAll(indexed, records);
    const std::string reference_projection = projectionOf(reference);
    const std::string indexed_projection = projectionOf(indexed);
    ASSERT_EQ(reference_projection, indexed_projection);
    EXPECT_EQ(digestCanonicalProjectionBytes(reference_projection),
              digestCanonicalProjectionBytes(indexed_projection));
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(G106Digest, PhysicalInsertionPermutationHasProjectionThenDigestParity) {
    const auto records = fixtureObjects();
    auto permuted = records;
    std::reverse(permuted.begin(), permuted.end());
    ReferenceObjectStore canonical;
    ReferenceObjectStore permutation;
    insertAll(canonical, records);
    insertAll(permutation, permuted);
    const std::string canonical_projection = projectionOf(canonical);
    const std::string permutation_projection = projectionOf(permutation);
    ASSERT_EQ(canonical_projection, permutation_projection);
    EXPECT_EQ(digestCanonicalProjectionBytes(canonical_projection),
              digestCanonicalProjectionBytes(permutation_projection));
}

TEST(G106Digest, SnapshotRestoredStateHasProjectionThenDigestParity) {
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId{id(0x42U)};
    snapshot.schema_version = 1U;
    snapshot.objects = fixtureObjects();

    ReferenceObjectStore baseline;
    insertAll(baseline, snapshot.objects);
    ReferenceObjectStore restored;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    SemanticGenerationState generation;
    ASSERT_TRUE(SnapshotBootstrapper::restore(snapshot, state, restored, generation).restored);
    const std::string baseline_projection = projectionOf(baseline);
    const std::string restored_projection = projectionOf(restored);
    ASSERT_EQ(baseline_projection, restored_projection);
    EXPECT_EQ(digestCanonicalProjectionBytes(baseline_projection),
              digestCanonicalProjectionBytes(restored_projection));
}

TEST(G106Digest, ReplayRecoveredStateHasProjectionThenDigestParity) {
    const auto records = fixtureObjects();
    ReferenceObjectStore baseline;
    insertAll(baseline, records);
    ReferenceObjectStore replayed;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(1U));
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const auto replay = ReplayCoordinator::replayAndFinalize(
        std::vector<Operation>{operation(InsertObjectsOp{records}, 500U)}, state, replayed,
        ledger, generation, clock);
    ASSERT_TRUE(replay.ready);
    const std::string baseline_projection = projectionOf(baseline);
    const std::string replay_projection = projectionOf(replayed);
    ASSERT_EQ(baseline_projection, replay_projection);
    EXPECT_EQ(digestCanonicalProjectionBytes(baseline_projection),
              digestCanonicalProjectionBytes(replay_projection));
}

TEST(G106Digest, PlacementMutationChangesProjectionBeforeDigest) {
    ReferenceObjectStore baseline;
    insertAll(baseline, fixtureObjects());
    ReferenceObjectStore mutated = baseline;
    const auto target = shape(2U);
    applyOperation(mutated, SetPlacementsOp{{PlacementItem{target.id, Placement{group(1U).id, OrderKey({9U})}}}}, 601U);
    const std::string before = projectionOf(baseline);
    const std::string after = projectionOf(mutated);
    ASSERT_NE(before, after);
    EXPECT_NE(digestCanonicalProjectionBytes(before), digestCanonicalProjectionBytes(after));
}

TEST(G106Digest, TransformMutationChangesProjectionBeforeDigest) {
    ReferenceObjectStore baseline;
    insertAll(baseline, fixtureObjects());
    ReferenceObjectStore mutated = baseline;
    applyOperation(mutated, SetTransformsOp{{TransformItem{id(2U), Transform2D{2.0, 0.0, 0.0, 2.0, 8.0, 9.0}}}}, 602U);
    const std::string before = projectionOf(baseline);
    const std::string after = projectionOf(mutated);
    ASSERT_NE(before, after);
    EXPECT_NE(digestCanonicalProjectionBytes(before), digestCanonicalProjectionBytes(after));
}

TEST(G106Digest, ContentMutationChangesProjectionBeforeDigest) {
    ReferenceObjectStore baseline;
    insertAll(baseline, fixtureObjects());
    ReferenceObjectStore mutated = baseline;
    applyOperation(mutated, SetObjectSizeOp{{ObjectSizeItem{id(2U), 30.0, 40.0}}}, 603U);
    const std::string before = projectionOf(baseline);
    const std::string after = projectionOf(mutated);
    ASSERT_NE(before, after);
    EXPECT_NE(digestCanonicalProjectionBytes(before), digestCanonicalProjectionBytes(after));
}

TEST(G106Digest, VerificationOnlySourceBoundaryAndFnvConstantsAreExplicit) {
    const auto read = [](const char* path) {
        std::ifstream source(path);
        EXPECT_TRUE(source.is_open()) << path;
        return std::string((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    };
    const std::string source = read(G1_06_DIGEST_SOURCE_PATH);
    const std::string header = read(G1_06_DIGEST_HEADER_PATH);
    EXPECT_NE(source.find("0xcbf29ce484222325"), std::string::npos);
    EXPECT_NE(source.find("0x100000001b3"), std::string::npos);
    EXPECT_NE(source.find("std::string_view"), std::string::npos);
    EXPECT_EQ(source.find("ObjectStore"), std::string::npos);
    EXPECT_EQ(source.find("nlohmann"), std::string::npos);
    EXPECT_EQ(source.find("json"), std::string::npos);
    EXPECT_EQ(source.find("protobuf"), std::string::npos);
    EXPECT_EQ(header.find("canvas/semantic/"), std::string::npos);
    EXPECT_EQ(header.find("ObjectStore"), std::string::npos);
}

} // namespace
} // namespace canvas::verification::g1_06
