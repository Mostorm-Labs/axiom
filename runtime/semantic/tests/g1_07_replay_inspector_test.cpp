#include "g1_07_replay_inspector.hpp"
#include "g1_07_replay_corpus.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <array>

namespace canvas::verification::g1_07 {
namespace {

using namespace canvas::semantic;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }
DocumentId document() { return DocumentId{id(0x707U)}; }

ObjectRecord shape(std::uint64_t value) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{std::nullopt, OrderKey({1U})};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    record.content = ShapeContent{1U, 10.0, 20.0};
    return record;
}

Operation insert(std::uint64_t operation_id, std::uint64_t object_id) {
    Operation operation{};
    operation.id = OperationId{id(operation_id)};
    operation.document_id = document();
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = InsertObjectsOp{{shape(object_id)}};
    return operation;
}

Operation missingTransform(std::uint64_t operation_id, std::uint64_t object_id) {
    Operation operation{};
    operation.id = OperationId{id(operation_id)};
    operation.document_id = document();
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.payload = SetTransformsOp{{TransformItem{
        id(object_id), Transform2D{2.0, 0.0, 0.0, 2.0, 3.0, 4.0}}}};
    return operation;
}

ReplayTrace trace() {
    ReplayTrace result;
    result.document_id = document();
    result.schema_version = 1U;
    result.baseline.runtime_epoch = RuntimeEpoch(42U);
    result.operations = {insert(1U, 11U), insert(2U, 12U)};
    return result;
}

TEST(G107ReplayInspector, SeekUsesExactPrefixAndIsDeterministic) {
    ReplayInspector inspector(trace(), Provider::kReference);
    const auto first = inspector.seek(1U);
    const auto repeat = inspector.seek(1U);
    ASSERT_TRUE(first.success);
    EXPECT_EQ(first.resulting_position, 1U);
    EXPECT_EQ(first.projection_json, repeat.projection_json);
    EXPECT_EQ(first.digest, repeat.digest);
}

TEST(G107ReplayInspector, ProvidersHaveProjectionParity) {
    const ReplayInspector reference(trace(), Provider::kReference);
    const ReplayInspector indexed(trace(), Provider::kIndexed);
    const auto left = reference.seek(2U);
    const auto right = indexed.seek(2U);
    ASSERT_TRUE(left.success);
    ASSERT_TRUE(right.success);
    EXPECT_EQ(left.projection_json, right.projection_json);
}

TEST(G107ReplayInspector, InvalidPositionFailsBeforeMutation) {
    ReplayInspector inspector(trace(), Provider::kReference);
    const auto result = inspector.seek(3U);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.failure, FailureClass::kPositionInvalid);
}

TEST(G107ReplayInspector, TraceParserRejectsWrongFormat) {
    const auto result = decodeTraceJson(R"({"format":"wrong","formatVersion":1})");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.failure, FailureClass::kTraceInvalid);
}

TEST(G107ReplayInspector, AllOperationFamilyCorpusReplaysThroughSingleCanonicalRoute) {
    const ReplayTrace trace = makeAllOperationFamiliesTrace();
    ASSERT_EQ(trace.operations.size(), 15U);
    const ReplayInspector reference(trace, Provider::kReference);
    const ReplayInspector indexed(trace, Provider::kIndexed);
    const auto left = reference.run();
    const auto right = indexed.run();
    ASSERT_TRUE(left.success) << failureClassName(left.failure);
    ASSERT_TRUE(right.success) << failureClassName(right.failure);
    EXPECT_EQ(left.projection_json, right.projection_json);
    EXPECT_EQ(left.digest, right.digest);
    EXPECT_EQ(left.resulting_position, 15U);
}

TEST(G107ReplayInspector, AppliedStepExposesCanonicalCommitChangeSetAndIdentityNamespaces) {
    const ReplayInspector inspector(trace(), Provider::kReference);
    const auto result = inspector.step(0U);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.commit_record.has_value());
    EXPECT_EQ(result.commit_record->source, ApplySource::kRestoreReplay);
    EXPECT_EQ(result.before_generation, SemanticGeneration(0U));
    EXPECT_EQ(result.after_generation, SemanticGeneration(1U));
    EXPECT_EQ(result.runtime_epoch, RuntimeEpoch(42U));
    EXPECT_EQ(result.commit_ordinal, CommitOrdinal(1U));
    ASSERT_EQ(result.commit_record->change_set.objects().size(), 1U);
    EXPECT_EQ(result.commit_record->change_set.objects().front().object_id, id(11U));

    const auto output = nlohmann::ordered_json::parse(writeStepJson(result));
    EXPECT_EQ(output.at("provider"), "reference");
    EXPECT_EQ(output.at("command"), "step");
    EXPECT_EQ(output.at("traceOperationCount"), 2U);
    EXPECT_EQ(output.at("requestedCursor"), 0U);
    EXPECT_EQ(output.at("resultingCursor"), 1U);
    EXPECT_EQ(output.at("commitRecord").at("applySource"), "RESTORE_REPLAY");
    EXPECT_EQ(output.at("commitRecord").at("beforeGeneration"), 0U);
    EXPECT_EQ(output.at("commitRecord").at("afterGeneration"), 1U);
    EXPECT_EQ(output.at("commitRecord").at("runtimeEpoch"), 42U);
    EXPECT_EQ(output.at("commitRecord").at("commitOrdinal"), 1U);
    EXPECT_EQ(output.at("commitRecord").at("changeSet").at("objects").size(), 1U);
    EXPECT_EQ(output.at("digestHex").get<std::string>().size(), 16U);
    EXPECT_FALSE(output.contains("revision"));
}

TEST(G107ReplayInspector, AlreadyAppliedDoesNotAdvanceOrFabricateCommit) {
    ReplayTrace duplicate = trace();
    duplicate.operations = {duplicate.operations.front(), duplicate.operations.front()};
    for (const Provider provider : {Provider::kReference, Provider::kIndexed}) {
        const ReplayInspector inspector(duplicate, provider);
        const auto step = inspector.step(1U);
        ASSERT_TRUE(step.success);
        EXPECT_EQ(step.disposition, ApplyDisposition::kAlreadyApplied);
        EXPECT_EQ(step.before_generation, SemanticGeneration(1U));
        EXPECT_EQ(step.after_generation, SemanticGeneration(1U));
        EXPECT_EQ(step.commit_ordinal, CommitOrdinal(1U));
        EXPECT_FALSE(step.commit_record.has_value());
        EXPECT_EQ(step.resulting_position, 2U);

        const auto run = inspector.run();
        ASSERT_TRUE(run.success);
        EXPECT_EQ(run.applied, 1U);
        EXPECT_EQ(run.already_applied, 1U);
        EXPECT_EQ(run.semantic_generation, SemanticGeneration(1U));
        EXPECT_EQ(run.commit_ordinal, CommitOrdinal(1U));
    }
}

TEST(G107ReplayInspector, RejectedOperationStopsBeforeLaterSuffixAndPreservesPrefix) {
    ReplayTrace rejected = trace();
    rejected.operations = {insert(1U, 11U), missingTransform(2U, 999U), insert(3U, 13U)};
    for (const Provider provider : {Provider::kReference, Provider::kIndexed}) {
        const ReplayInspector inspector(rejected, provider);
        const auto result = inspector.run();
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.failure, FailureClass::kApplyRejected);
        EXPECT_EQ(result.applied, 1U);
        EXPECT_EQ(result.already_applied, 0U);
        ASSERT_EQ(result.failure_index, 1U);
        ASSERT_TRUE(result.failure_operation_id.has_value());
        EXPECT_EQ(*result.failure_operation_id, OperationId{id(2U)});
        EXPECT_EQ(result.failure_disposition, ApplyDisposition::kRejected);
        EXPECT_EQ(result.resulting_position, 1U);
        EXPECT_EQ(result.state_after, DocumentRuntimeState::kFailed);
        EXPECT_EQ(result.semantic_generation, SemanticGeneration(1U));
        EXPECT_EQ(result.commit_ordinal, CommitOrdinal(1U));
        EXPECT_NE(result.projection_json.find("id128:0b"), std::string::npos);
        EXPECT_EQ(result.projection_json.find("id128:0d"), std::string::npos);
    }
}

TEST(G107ReplayInspector, CommitBlockedIsDeterministicAndDoesNotAdvance) {
    ReplayTrace blocked = trace();
    blocked.baseline.runtime_epoch = RuntimeEpoch{};
    blocked.operations.resize(1U);
    for (const Provider provider : {Provider::kReference, Provider::kIndexed}) {
        const ReplayInspector inspector(blocked, provider);
        const auto result = inspector.step(0U);
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.failure, FailureClass::kCommitBlocked);
        EXPECT_EQ(result.disposition, ApplyDisposition::kCommitBlocked);
        EXPECT_EQ(result.commit_block_reason, CommitBlockReason::kInvalidRuntimeEpoch);
        EXPECT_EQ(result.before_generation, SemanticGeneration{});
        EXPECT_EQ(result.after_generation, SemanticGeneration{});
        EXPECT_EQ(result.commit_ordinal, CommitOrdinal{});
        EXPECT_EQ(result.resulting_position, 0U);
        EXPECT_FALSE(result.commit_record.has_value());
    }
}

TEST(G107ReplayInspector, SnapshotBootstrapFailureStopsBeforeContinuation) {
    ReplayTrace invalid = trace();
    invalid.baseline.kind = BaselineKind::kSnapshot;
    invalid.baseline.generation = SemanticGeneration(7U);
    SemanticSnapshot snapshot;
    snapshot.document_id = document();
    snapshot.schema_version = 1U;
    snapshot.objects = {shape(21U), shape(21U)};
    invalid.baseline.snapshot = snapshot;
    invalid.operations = {insert(3U, 13U)};
    for (const Provider provider : {Provider::kReference, Provider::kIndexed}) {
        const ReplayInspector inspector(invalid, provider);
        const auto result = inspector.run();
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.failure, FailureClass::kSnapshotBootstrapFailed);
        EXPECT_EQ(result.applied, 0U);
        EXPECT_EQ(result.resulting_position, 0U);
        EXPECT_EQ(result.state_after, DocumentRuntimeState::kFailed);
        EXPECT_EQ(result.semantic_generation, SemanticGeneration(7U));
        EXPECT_EQ(result.projection_json.find("id128:0d"), std::string::npos);
    }
}

TEST(G107ReplayInspector, ObjectFoundAndMissingQueriesAreReadOnlyAndProviderEquivalent) {
    std::optional<std::string> expected_projection;
    for (const Provider provider : {Provider::kReference, Provider::kIndexed}) {
        const ReplayInspector inspector(trace(), provider);
        const auto found = inspector.object(1U, id(11U));
        const auto missing = inspector.object(1U, id(999U));
        ASSERT_TRUE(found.success);
        ASSERT_TRUE(missing.success);
        EXPECT_TRUE(found.found);
        EXPECT_FALSE(missing.found);
        EXPECT_FALSE(found.object_json.empty());
        EXPECT_TRUE(missing.object_json.empty());
        EXPECT_EQ(found.projection_json, missing.projection_json);
        EXPECT_EQ(found.semantic_generation, missing.semantic_generation);
        EXPECT_EQ(found.commit_ordinal, missing.commit_ordinal);
        if (expected_projection.has_value()) {
            EXPECT_EQ(found.projection_json, *expected_projection);
        }
        expected_projection = found.projection_json;
    }
}

TEST(G107ReplayInspector, SeekCoversBoundaryAndLongCorpusPositionsAcrossProviders) {
    const ReplayTrace corpus = makeAllOperationFamiliesTrace();
    for (const std::size_t position : std::array<std::size_t, 5>{0U, 1U, 5U, 10U, 15U}) {
        const ReplayInspector reference(corpus, Provider::kReference);
        const ReplayInspector indexed(corpus, Provider::kIndexed);
        const auto left = reference.seek(position);
        const auto repeat = reference.seek(position);
        const auto right = indexed.seek(position);
        ASSERT_TRUE(left.success) << position;
        ASSERT_TRUE(right.success) << position;
        EXPECT_EQ(left.projection_json, repeat.projection_json) << position;
        EXPECT_EQ(left.projection_json, right.projection_json) << position;
        EXPECT_EQ(left.semantic_generation, SemanticGeneration(position)) << position;
        EXPECT_EQ(left.commit_ordinal, CommitOrdinal(position)) << position;
    }
}

TEST(G107ReplayInspector, TraceParserFailsClosedForStructuralAndMalformedInputs) {
    const std::array<std::string, 4> malformed = {
        R"({"format":"wrong","formatVersion":1})",
        R"({"format":"axiom-semantic-replay-trace-v1","formatVersion":1,"documentIdHex":"00000000000000000000000000000000","schemaVersion":1,"baseline":{},"operations":[]})",
        R"({"format":"axiom-semantic-replay-trace-v1","formatVersion":1,"documentIdHex":"00000000000000000000000000000707","schemaVersion":1,"baseline":{"kind":"empty","semanticGeneration":1,"runtimeEpoch":42,"commitOrdinal":0},"operations":[]})",
        R"({"format":"axiom-semantic-replay-trace-v1","formatVersion":1,"documentIdHex":"00000000000000000000000000000707","schemaVersion":1,"baseline":{"kind":"snapshot","semanticGeneration":0,"runtimeEpoch":42,"commitOrdinal":0,"snapshotBytesHex":"0"},"operations":[]})",
    };
    for (const auto& input : malformed) {
        const auto result = decodeTraceJson(input);
        EXPECT_FALSE(result.ok());
        EXPECT_NE(result.failure, FailureClass::kNone);
    }
}

} // namespace
} // namespace canvas::verification::g1_07
