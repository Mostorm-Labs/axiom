#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/replay.hpp"

#include "g1_06_projection.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

using canvas::verification::g1_06::ProjectionDocumentId;
using canvas::verification::g1_06::projectDocument;
using canvas::verification::g1_06::writeCanonicalProjectionJson;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord shape(std::uint64_t value, std::uint8_t tag = 1U) {
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
    result.document_id = DocumentId{id(1234U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

std::vector<Operation> deterministicStream() {
    std::vector<Operation> result;
    for (std::uint64_t value = 1U; value <= 8U; ++value) {
        result.push_back(operation(InsertObjectsOp{{shape(value, static_cast<std::uint8_t>(value))}},
                                   2000U + value));
    }
    for (std::uint64_t value = 1U; value <= 8U; ++value) {
        result.push_back(operation(
            SetTransformsOp{{TransformItem{
                id(value), Transform2D{1.0, 0.0, 0.0, 1.0, static_cast<double>(value * 10U),
                                         static_cast<double>(value * 20U)}}}},
            2100U + value));
    }
    return result;
}

struct Outcome final {
    ReplayResult replay;
    std::string projection;
    SemanticGeneration generation;
    CommitOrdinal ordinal;
};

template <typename Store>
Outcome runReplay(std::span<const Operation> operations) {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(9U));
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const ReplayResult replay = ReplayCoordinator::replayAndFinalize(
        operations, state, objects, ledger, generation, clock);
    const ProjectionDocumentId document_id{{0x44U}};
    return Outcome{
        replay,
        writeCanonicalProjectionJson(projectDocument(document_id, 1U, objects)),
        generation.current(),
        clock.lastCommittedOrdinal()};
}

Outcome runDirectReference(std::span<const Operation> operations) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(9U));
    OperationEngine engine;
    ReplayResult result;
    for (std::size_t index = 0; index < operations.size(); ++index) {
        const ApplyResult applied = engine.apply(
            operations[index], ApplySource::kRestoreReplay, objects, ledger, generation, clock);
        if (applied.disposition == ApplyDisposition::kApplied) {
            ++result.applied;
            continue;
        }
        if (applied.disposition == ApplyDisposition::kAlreadyApplied) {
            ++result.already_applied;
            continue;
        }
        result.failure = ReplayFailure{index, operations[index].id, applied.disposition};
        break;
    }
    result.ready = !result.failure.has_value();
    return Outcome{
        result,
        writeCanonicalProjectionJson(projectDocument(ProjectionDocumentId{{0x44U}}, 1U, objects)),
        generation.current(),
        clock.lastCommittedOrdinal()};
}

} // namespace

TEST(G106ReplayDifferential, ReplayCoordinatorMatchesDirectReferenceLoopAndIndexedProjection) {
    const std::vector<Operation> operations = deterministicStream();
    const Outcome replay_reference = runReplay<ReferenceObjectStore>(operations);
    const Outcome replay_indexed = runReplay<IndexedObjectStore>(operations);
    const Outcome direct_reference = runDirectReference(operations);

    ASSERT_TRUE(replay_reference.replay.ready);
    ASSERT_TRUE(replay_indexed.replay.ready);
    ASSERT_TRUE(direct_reference.replay.ready);
    EXPECT_EQ(replay_reference.replay.applied, operations.size());
    EXPECT_EQ(replay_indexed.replay.applied, operations.size());
    EXPECT_EQ(replay_reference.replay.already_applied, 0U);
    EXPECT_EQ(replay_reference.projection, direct_reference.projection);
    EXPECT_EQ(replay_reference.projection, replay_indexed.projection);
    EXPECT_EQ(replay_reference.generation, direct_reference.generation);
    EXPECT_EQ(replay_reference.generation, replay_indexed.generation);
    EXPECT_EQ(replay_reference.ordinal, direct_reference.ordinal);
    EXPECT_EQ(replay_reference.ordinal, replay_indexed.ordinal);
}

TEST(G106ReplayDifferential, LongStreamHasThreeDeterministicCheckpointEqualities) {
    const std::vector<Operation> operations = deterministicStream();
    for (const std::size_t checkpoint : {std::size_t{4U}, std::size_t{12U}, operations.size()}) {
        const Outcome replay = runReplay<ReferenceObjectStore>(
            std::span<const Operation>{operations.data(), checkpoint});
        const Outcome direct = runDirectReference(
            std::span<const Operation>{operations.data(), checkpoint});
        ASSERT_TRUE(replay.replay.ready) << checkpoint;
        EXPECT_EQ(replay.projection, direct.projection) << checkpoint;
        EXPECT_EQ(replay.generation, direct.generation) << checkpoint;
        EXPECT_EQ(replay.ordinal, direct.ordinal) << checkpoint;
    }
}

TEST(G106ReplayDifferential, FirstDivergenceReportsExactIndexAndOperationId) {
    std::vector<Operation> operations = deterministicStream();
    operations.insert(operations.begin() + 3U, operation(
        SetTransformsOp{{TransformItem{id(999U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        2999U));
    const Outcome replay = runReplay<ReferenceObjectStore>(operations);
    ASSERT_FALSE(replay.replay.ready);
    ASSERT_TRUE(replay.replay.failure.has_value());
    EXPECT_EQ(replay.replay.failure->operation_index, 3U);
    EXPECT_EQ(replay.replay.failure->operation_id, operations[3U].id);
    EXPECT_EQ(replay.replay.failure->disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(replay.replay.applied, 3U);
}

TEST(G106ReplayDifferential, ReplaySourceUsesNoEchoProjectionAndNoPublicationLane) {
    std::ifstream source(G1_06_REPLAY_SOURCE_PATH);
    ASSERT_TRUE(source.is_open());
    const std::string contents{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
    EXPECT_NE(contents.find("ApplySource::kRestoreReplay"), std::string::npos);
    EXPECT_EQ(contents.find("Outbox"), std::string::npos);
    EXPECT_EQ(contents.find("outbox"), std::string::npos);
    EXPECT_EQ(contents.find("publish"), std::string::npos);
    EXPECT_EQ(contents.find("publication"), std::string::npos);
}

} // namespace canvas::semantic
