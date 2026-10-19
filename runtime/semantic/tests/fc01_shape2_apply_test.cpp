#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/replay.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"

#include <gtest/gtest.h>

#include <limits>

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


TEST(FC01Shape2Apply, TypedIngressChecksEnvelopeBeforeMutation) {
    for (const auto version : {0U, 2U, 3U}) {
        ReferenceObjectStore objects;
        AppliedOperationLedger ledger;
        OperationEngine engine;
        auto operation = insert(shape(2U, 3U, 1U), 10U);
        operation.payload_version = version;
        EXPECT_EQ(engine.prepare(operation, {objects, ledger}).disposition, PrepareDisposition::kRejected);
        operation.payload_version = 1U;
        operation.schema_version = version;
        EXPECT_EQ(engine.prepare(operation, {objects, ledger}).disposition, PrepareDisposition::kRejected);
    }
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    auto operation = insert(shape(2U, 3U, 1U), 10U);
    operation.id = OperationId{};
    EXPECT_EQ(OperationEngine{}.prepare(operation, {objects, ledger}).disposition, PrepareDisposition::kRejected);
    operation.id = OperationId{id(10U)};
    operation.document_id = DocumentId{};
    EXPECT_EQ(OperationEngine{}.prepare(operation, {objects, ledger}).disposition, PrepareDisposition::kRejected);
}

TEST(FC01Shape2Apply, TypedIngressNormalizesBeforeIdempotency) {
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(1U));
    OperationEngine engine;
    auto operation = insert(shape(2U, 3U, 1U), 10U);
    operation.payload = InsertObjectsOp{{shape(1U, 1U, 2U), shape(2U, 3U, 1U)}};
    auto result = engine.apply(operation, ApplySource::kLocalInteraction, objects, ledger, generation, clock);
    ASSERT_EQ(result.disposition, ApplyDisposition::kApplied);
    std::get<InsertObjectsOp>(operation.payload).objects = {shape(2U, 3U, 1U), shape(1U, 1U, 2U)};
    result = engine.apply(operation, ApplySource::kRemoteSync, objects, ledger, generation, clock);
    EXPECT_EQ(result.disposition, ApplyDisposition::kAlreadyApplied);
    EXPECT_FALSE(result.commit_record.has_value());
    EXPECT_EQ(generation.current().value(), 1U);
    EXPECT_EQ(clock.lastCommittedOrdinal().value(), 1U);
}

TEST(FC01Shape2Apply, MixedBatchRejectsAtomicallyAndFailedReplayNeverReady) {
    for (const auto source : {ApplySource::kLocalInteraction, ApplySource::kRemoteSync, ApplySource::kRestoreReplay}) {
        IndexedObjectStore objects;
        AppliedOperationLedger ledger;
        SemanticGenerationState generation;
        CanonicalCommitClock clock(RuntimeEpoch(1U));
        auto operation = insert(shape(2U, 3U, 1U), 10U);
        operation.payload = InsertObjectsOp{{shape(2U, 3U, 1U), shape(1U, 3U, 2U)}};
        const auto result = OperationEngine{}.apply(operation, source, objects, ledger, generation, clock);
        EXPECT_EQ(result.disposition, ApplyDisposition::kRejected);
        EXPECT_FALSE(result.commit_record.has_value());
        EXPECT_EQ(objects.size(), 0U);
        EXPECT_FALSE(ledger.find(operation.id).has_value());
        EXPECT_EQ(generation.current().value(), 0U);
        EXPECT_EQ(clock.lastCommittedOrdinal().value(), 0U);
        DocumentRuntimeState state = DocumentRuntimeState::kLoading;
        const std::vector<Operation> continuation{operation, insert(shape(2U, 3U, 3U), 11U)};
        const auto replay = ReplayCoordinator::replayAndFinalize(continuation, state, objects, ledger, generation, clock);
        EXPECT_FALSE(replay.ready);
        EXPECT_EQ(state, DocumentRuntimeState::kFailed);
        EXPECT_EQ(objects.size(), 0U);
    }
}

TEST(FC01Shape2Apply, SnapshotTriangleRequiresExplicitReaderCapability) {
    SemanticSnapshot snapshot{DocumentId{id(1000U)}, 2U, {shape(2U, 3U, 1U)}};
    ReferenceObjectStore objects;
    SemanticGenerationState generation;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const auto result = SnapshotBootstrapper::restore(snapshot, state, objects, generation);
    EXPECT_FALSE(result.restored);
    EXPECT_EQ(state, DocumentRuntimeState::kFailed);
    EXPECT_EQ(objects.size(), 0U);
    EXPECT_EQ(generation.current().value(), 0U);
}


TEST(FC01Shape2Apply, CapableSnapshotRoundTripAndLegacyWriterRefusal) {
    const SemanticSnapshot snapshot{DocumentId{id(1000U)}, 2U, {shape(2U, 3U, 1U)}};
    EXPECT_FALSE(SnapshotCodec::encode(snapshot).ok());
    const auto encoded = SnapshotCodec::encode(snapshot, SnapshotCapability::kShape2Triangle);
    ASSERT_TRUE(encoded.ok());
    EXPECT_FALSE(SnapshotCodec::decode(encoded.bytes).ok());
    const auto decoded = SnapshotCodec::decode(encoded.bytes, SnapshotCapability::kShape2Triangle);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(*decoded.snapshot, snapshot);
    IndexedObjectStore objects;
    SemanticGenerationState generation;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const auto restored = SnapshotBootstrapper::restore(*decoded.snapshot, SnapshotCapability::kShape2Triangle,
                                                        state, objects, generation);
    EXPECT_TRUE(restored.restored);
    EXPECT_EQ(state, DocumentRuntimeState::kLoading);
    EXPECT_EQ(objects.allObjects(), snapshot.objects);
    auto legacy = snapshot;
    legacy.schema_version = 1U;
    EXPECT_FALSE(SnapshotCodec::encode(legacy, SnapshotCapability::kShape2Triangle).ok());
}


std::vector<std::uint8_t> manualHex(const char* input) {
    auto nibble = [](char c) -> unsigned { return c <= '9' ? static_cast<unsigned>(c - '0') : static_cast<unsigned>(c - 'a' + 10); };
    std::vector<std::uint8_t> result;
    for (std::size_t i = 0; input[i] != '\0'; i += 2U)
        result.push_back(static_cast<std::uint8_t>((nibble(input[i]) << 4U) | nibble(input[i + 1U])));
    return result;
}

TEST(FC01Shape2Apply, FrozenIndependentOperationGoldenMatchesProductionEncoding) {
    const auto expected = manualHex("0a120a100000000000000000000000000000000212120a1000000000000000000000000000000003180120012a750a730a710a120a100000000000000000000000000000000110011802220512030a01802a3609000000000000f03f11000000000000000019000000000000000021000000000000f03f29000000000000000031000000000000000032003a160a140803110000000000005940190000000000005440");
    ASSERT_EQ(expected.size(), 163U);
    const auto decoded = SemanticCodec::decodeProtobufOperation(expected);
    ASSERT_TRUE(decoded.ok());
    const auto encoded = SemanticCodec::encodeProtobufOperation(decoded.operation);
    ASSERT_TRUE(encoded.ok());
    EXPECT_EQ(encoded.bytes, expected);
    const auto observed = SemanticCodec::observeGoldenFixture("Operation", expected, true);
    EXPECT_TRUE(observed.accepted);
    EXPECT_EQ(observed.canonical_bytes, expected);
}

TEST(FC01Shape2Apply, GoldenRootsAndNestedUnknownWireFailClosed) {
    const auto shapeBytes = manualHex("0803110000000000005940190000000000005440");
    const auto objectBytes = manualHex("0a120a100000000000000000000000000000000110011802220512030a01802a3609000000000000f03f11000000000000000019000000000000000021000000000000f03f29000000000000000031000000000000000032003a160a140803110000000000005940190000000000005440");
    const auto shapeObserved = SemanticCodec::observeGoldenFixture("ShapeContent", shapeBytes, true);
    EXPECT_TRUE(shapeObserved.accepted);
    EXPECT_EQ(shapeObserved.canonical_bytes, shapeBytes);
    const auto objectObserved = SemanticCodec::observeGoldenFixture("ObjectRecord", objectBytes, true);
    EXPECT_TRUE(objectObserved.accepted);
    EXPECT_EQ(objectObserved.canonical_bytes, objectBytes);
    auto unknown = shapeBytes;
    unknown.insert(unknown.end(), {0x20U, 0x01U});
    EXPECT_FALSE(SemanticCodec::observeGoldenFixture("ShapeContent", unknown, true).accepted);
    auto missing = shapeBytes;
    missing.erase(missing.begin(), missing.begin() + 2);
    EXPECT_FALSE(SemanticCodec::observeGoldenFixture("ShapeContent", missing, true).accepted);
    auto duplicate = shapeBytes;
    duplicate.insert(duplicate.end(), {0x08U, 0x03U});
    EXPECT_FALSE(SemanticCodec::observeGoldenFixture("ShapeContent", duplicate, true).accepted);
}

// Expectations are the frozen SH2-G06..12/G18/G21 negatives, not values
// captured from the candidate. Exercise commit admission, including its ledger,
// rather than relying only on a successful structural-validator call.
template <typename Store>
void expectAtomicReject(const Operation& operation) {
    for (const auto source : {ApplySource::kLocalInteraction, ApplySource::kRemoteSync,
                              ApplySource::kRestoreReplay, ApplySource::kUndoRedo}) {
        Store objects;
        AppliedOperationLedger ledger;
        SemanticGenerationState generation;
        CanonicalCommitClock clock(RuntimeEpoch(1U));
        const auto result = OperationEngine{}.apply(operation, source, objects, ledger, generation, clock);
        EXPECT_EQ(result.disposition, ApplyDisposition::kRejected);
        EXPECT_FALSE(result.commit_record.has_value());
        EXPECT_TRUE(objects.allObjects().empty());
        EXPECT_FALSE(ledger.find(operation.id).has_value());
        EXPECT_EQ(generation.current().value(), 0U);
        EXPECT_EQ(clock.lastCommittedOrdinal().value(), 0U);
    }
}

TEST(FC01Shape2Closeout, FrozenNegativeTupleAndExtentMatrixIsAtomic) {
    std::vector<ObjectRecord> invalid;
    for (const auto tuple : {std::pair{1U, 3U}, {2U, 0U}, {2U, 1U},
                             {2U, 2U}, {2U, 4U}, {3U, 3U}}) {
        invalid.push_back(shape(tuple.first, tuple.second, 2U));
    }
    for (const auto extent : {0.0, -1.0, -0.0, std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity(),
                              std::numeric_limits<double>::quiet_NaN()}) {
        for (const bool width : {true, false}) {
            auto object = shape(2U, 3U, 2U);
            auto& content = std::get<ShapeContent>(object.content);
            if (width) content.width = extent;
            else content.height = extent;
            invalid.push_back(object);
        }
    }
    auto wrongContent = shape(2U, 3U, 2U);
    wrongContent.content = ConnectorContent{};
    invalid.push_back(wrongContent);
    auto wrongKind = shape(2U, 3U, 2U);
    wrongKind.kind = ObjectKind::kConnector;
    invalid.push_back(wrongKind);
    for (const auto& object : invalid) {
        auto operation = insert(shape(2U, 3U, 1U), 10U);
        operation.payload = InsertObjectsOp{{shape(2U, 3U, 1U), object}};
        expectAtomicReject<ReferenceObjectStore>(operation);
        expectAtomicReject<IndexedObjectStore>(operation);
        operation.payload = RestoreObjectsOp{{shape(2U, 3U, 1U), object}};
        expectAtomicReject<ReferenceObjectStore>(operation);
        expectAtomicReject<IndexedObjectStore>(operation);
    }
}

TEST(FC01Shape2Closeout, AlteredPayloadCollisionPreservesOriginalCommit) {
    IndexedObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(1U));
    OperationEngine engine;
    auto operation = insert(shape(2U, 3U, 1U), 10U);
    ASSERT_EQ(engine.apply(operation, ApplySource::kLocalInteraction, objects, ledger,
                           generation, clock).disposition, ApplyDisposition::kApplied);
    const auto before = objects.allObjects();
    const auto original = ledger.find(operation.id);
    ASSERT_TRUE(original.has_value());
    std::get<ShapeContent>(std::get<InsertObjectsOp>(operation.payload).objects.front().content).width = 200.0;
    const auto result = engine.apply(operation, ApplySource::kRemoteSync, objects, ledger, generation, clock);
    EXPECT_EQ(result.disposition, ApplyDisposition::kRejected);
    EXPECT_FALSE(result.commit_record.has_value());
    EXPECT_EQ(objects.allObjects(), before);
    ASSERT_TRUE(ledger.find(operation.id).has_value());
    EXPECT_EQ(ledger.find(operation.id)->canonical_operation.id, original->canonical_operation.id);
    EXPECT_EQ(ledger.find(operation.id)->canonical_operation.payload, original->canonical_operation.payload);
    EXPECT_EQ(generation.current().value(), 1U);
    EXPECT_EQ(clock.lastCommittedOrdinal().value(), 1U);
}

TEST(FC01Shape2Closeout, LegacySnapshotRemainsV1AndTriangleCannotBeDowngraded) {
    const SemanticSnapshot legacy{DocumentId{id(1000U)}, 1U,
                                   {shape(1U, 1U, 1U), shape(1U, 2U, 2U)}};
    const auto oldReader = SnapshotCodec::encode(legacy);
    const auto newReader = SnapshotCodec::encode(legacy, SnapshotCapability::kShape2Triangle);
    ASSERT_TRUE(oldReader.ok());
    ASSERT_TRUE(newReader.ok());
    EXPECT_EQ(oldReader.bytes, newReader.bytes);
    const auto decoded = SnapshotCodec::decode(oldReader.bytes, SnapshotCapability::kShape2Triangle);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(*decoded.snapshot, legacy);
    auto invalid = legacy;
    invalid.objects.push_back(shape(2U, 3U, 3U));
    EXPECT_FALSE(SnapshotCodec::encode(invalid, SnapshotCapability::kShape2Triangle).ok());
    ReferenceObjectStore objects;
    SemanticGenerationState generation;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    EXPECT_FALSE(SnapshotBootstrapper::restore(invalid, SnapshotCapability::kShape2Triangle,
                                              state, objects, generation).restored);
    EXPECT_EQ(state, DocumentRuntimeState::kFailed);
    EXPECT_TRUE(objects.allObjects().empty());
    EXPECT_EQ(generation.current().value(), 0U);
}

TEST(FC01Shape2Closeout, AttachedConnectorCannotTargetTriangle) {
    IndexedObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(1U));
    OperationEngine engine;
    ASSERT_EQ(engine.apply(insert(shape(2U, 3U, 1U), 10U), ApplySource::kLocalInteraction,
                           objects, ledger, generation, clock).disposition, ApplyDisposition::kApplied);
    auto connector = shape(1U, 1U, 2U);
    connector.kind = ObjectKind::kConnector;
    ConnectorContent content;
    content.start.value = AttachedEndpoint{id(1U), AutoPerimeterAnchor{}};
    content.end.value = FreePointEndpoint{{200.0, 100.0}};
    content.routing = ConnectorRouting::kStraight;
    connector.content = content;
    const auto before = objects.allObjects();
    const auto rejected = engine.apply(insert(connector, 11U), ApplySource::kRemoteSync,
                                        objects, ledger, generation, clock);
    EXPECT_EQ(rejected.disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(objects.allObjects(), before);
    EXPECT_FALSE(ledger.find(OperationId{id(11U)}).has_value());
    EXPECT_EQ(generation.current().value(), 1U);
    EXPECT_EQ(clock.lastCommittedOrdinal().value(), 1U);
}

}  // namespace
}  // namespace canvas::semantic
