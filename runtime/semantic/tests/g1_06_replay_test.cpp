#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/replay.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

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

ObjectRecord group(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kGroup;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey({static_cast<std::uint8_t>(value)})};
    record.content = GroupContent{};
    return record;
}

ObjectRecord vectorPath(std::uint64_t value) {
    ObjectRecord record = shape(value);
    record.kind = ObjectKind::kVectorPath;
    record.content = VectorPathContent{VectorPathGeometry{
        FillRule::kNonZero, {MoveTo{Vec2{0.0, 0.0}}, LineTo{Vec2{1.0, 1.0}}}}};
    return record;
}

ObjectRecord image(std::uint64_t value) {
    ObjectRecord record = shape(value);
    record.kind = ObjectKind::kImage;
    record.content = ImageContent{
        ResourceId{id(800U)}, 10.0, 10.0, std::nullopt, ImageContentMode::kFit, 10.0, 10.0};
    return record;
}

ObjectRecord vectorStroke(std::uint64_t value) {
    ObjectRecord record = shape(value);
    record.kind = ObjectKind::kVectorStroke;
    StrokeRecord stroke{};
    stroke.brush.brush_family_id = 1U;
    stroke.brush.brush_version = 1U;
    stroke.brush.nominal_size = 1.0;
    stroke.brush.opacity = 1.0F;
    stroke.data = VectorStrokeData{{StrokeSample{Vec2{0.0, 0.0}, 1.0F, Vec2{0.0, 0.0}}}};
    record.content = VectorStrokeContent{stroke};
    return record;
}

ObjectRecord richText(std::uint64_t value) {
    ObjectRecord record = shape(value);
    record.kind = ObjectKind::kRichText;
    TextStyle style{};
    style.font_resource_id = ResourceId{id(700U)};
    style.font_size = 12.0;
    style.weight = 400U;
    style.color = ColorValue{0.0F, 0.0F, 0.0F, 1.0F};
    Paragraph paragraph{};
    paragraph.id = id(value * 10U);
    paragraph.style = ParagraphStyle{ParagraphAlignment::kLeft, 1.0, 0.0, 0.0};
    paragraph.runs = {TextRun{"A", style}};
    record.content = RichTextContent{RichTextDocument{{paragraph}}};
    return record;
}

ObjectRecord connector(std::uint64_t value) {
    ObjectRecord record = shape(value);
    record.kind = ObjectKind::kConnector;
    record.content = ConnectorContent{
        ConnectorEndpoint{FreePointEndpoint{Vec2{0.0, 0.0}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{1.0, 1.0}}}, ConnectorRouting::kStraight};
    return record;
}

ObjectRecord attachedConnector(std::uint64_t value, ObjectId target) {
    ObjectRecord record = connector(value);
    record.content = ConnectorContent{
        ConnectorEndpoint{AttachedEndpoint{target, AutoPerimeterAnchor{}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{9.0, 9.0}}}, ConnectorRouting::kStraight};
    return record;
}

EraseMaskRecord eraseMask(std::uint64_t value) {
    return EraseMaskRecord{
        id(value), SweptCircleMask{{EraseCubicSegment{
                       EraseKnot{Vec2{0.0, 0.0}, 1.0}, EraseKnot{Vec2{1.0, 1.0}, 1.0},
                       Vec2{0.5, 0.5}, Vec2{0.5, 0.5}}}}};
}

Operation operation(OperationPayload payload, std::uint64_t operation_id) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(9000U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

Operation insertOperation(std::uint64_t operation_id, std::uint64_t object_id) {
    return operation(InsertObjectsOp{{shape(object_id, 7U)}}, operation_id);
}

Operation missingTransform(std::uint64_t operation_id) {
    return operation(
        SetTransformsOp{{TransformItem{id(999U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        operation_id);
}

struct FamilyFixture final {
    Operation operation;
    std::vector<ObjectRecord> initial_objects;
};

std::vector<FamilyFixture> releasedFamilyFixtures() {
    const ObjectRecord base = shape(100U, 3U);
    const ObjectRecord vector_path = vectorPath(101U);
    const ObjectRecord image_record = image(102U);
    const ObjectRecord vector_stroke = vectorStroke(103U);
    const ObjectRecord rich_text = richText(104U);
    const ObjectRecord connector_record = connector(105U);
    ObjectRecord placed = base;
    placed.placement = Placement{std::nullopt, OrderKey({9U})};
    ObjectRecord transformed = base;
    transformed.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 8.0, 9.0};
    ObjectRecord patched = base;
    patched.properties.entries = {{1U, PropertyValue{false}}, {2U, PropertyValue{true}}};
    ObjectRecord resized = base;
    resized.content = ShapeContent{1U, 20.0, 30.0};
    const VectorPathGeometry geometry{
        FillRule::kEvenOdd, {MoveTo{Vec2{2.0, 3.0}}, LineTo{Vec2{4.0, 5.0}}}};
    ObjectRecord geometry_changed = vector_path;
    geometry_changed.content = VectorPathContent{geometry};
    ObjectRecord image_changed = image_record;
    image_changed.content = ImageContent{
        ResourceId{id(801U)}, 44.0, 55.0, std::nullopt, ImageContentMode::kFill, 66.0, 77.0};
    const EraseMaskRecord mask = eraseMask(701U);
    ObjectRecord masked = vector_stroke;
    masked.erase_masks = {mask};
    ObjectRecord connector_changed = connector_record;
    connector_changed.content = ConnectorContent{
        ConnectorEndpoint{FreePointEndpoint{Vec2{5.0, 6.0}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{7.0, 8.0}}}, ConnectorRouting::kOrthogonal};

    return {
        {operation(InsertObjectsOp{{base}}, 2001U), {}},
        {operation(DeleteObjectsOp{{base.id}}, 2002U), {base}},
        {operation(RestoreObjectsOp{{base}}, 2003U), {}},
        {operation(SetPlacementsOp{{PlacementItem{base.id, placed.placement}}}, 2004U), {base}},
        {operation(SetTransformsOp{{TransformItem{base.id, transformed.transform}}}, 2005U), {base}},
        {operation(PatchPropertiesOp{{
                       PropertyPatch{base.id, 2U, PropertyPatchAction::kSet, PropertyValue{true}},
                       PropertyPatch{base.id, 1U, PropertyPatchAction::kSet, PropertyValue{false}}}},
                   2006U),
         {base}},
        {operation(SetObjectSizeOp{{ObjectSizeItem{base.id, 20.0, 30.0}}}, 2007U), {base}},
        {operation(SetVectorPathGeometryOp{vector_path.id, geometry}, 2008U), {vector_path}},
        {operation(SetImageContentOp{image_record.id, std::get<ImageContent>(image_changed.content)}, 2009U),
         {image_record}},
        {operation(AddStrokeOp{vector_stroke}, 2010U), {}},
        {operation(SplitStrokesOp{{StrokeSplit{vector_stroke.id, {vectorStroke(106U)}}}}, 2011U),
         {vector_stroke}},
        {operation(AddEraseMasksOp{{EraseMaskAddItem{vector_stroke.id, {mask}}}}, 2012U),
         {vector_stroke}},
        {operation(RemoveEraseMasksOp{{EraseMaskRemoveItem{masked.id, {mask.id}}}}, 2013U), {masked}},
        {operation(EditRichTextOp{rich_text.id, RichTextDelta{}}, 2014U), {rich_text}},
        {operation(SetConnectorContentOp{
                       connector_record.id, std::get<ConnectorContent>(connector_changed.content)},
                   2015U),
         {connector_record}},
    };
}

template <typename Store>
void expectAllReleasedFamiliesReplay() {
    const std::vector<FamilyFixture> fixtures = releasedFamilyFixtures();
    ASSERT_EQ(fixtures.size(), 15U);
    for (const FamilyFixture& fixture : fixtures) {
        Store objects;
        for (const ObjectRecord& record : fixture.initial_objects) {
            ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(objects, record));
        }
        AppliedOperationLedger ledger;
        SemanticGenerationState generation;
        CanonicalCommitClock clock(RuntimeEpoch(12U));
        DocumentRuntimeState state = DocumentRuntimeState::kLoading;
        const std::vector<Operation> continuation{fixture.operation};
        const ReplayResult result = ReplayCoordinator::replayAndFinalize(
            continuation, state, objects, ledger, generation, clock);
        ASSERT_TRUE(result.ready) << static_cast<unsigned>(fixture.operation.kind());
        EXPECT_EQ(result.applied, 1U) << static_cast<unsigned>(fixture.operation.kind());
        EXPECT_EQ(result.already_applied, 0U) << static_cast<unsigned>(fixture.operation.kind());
        EXPECT_FALSE(result.failure.has_value()) << static_cast<unsigned>(fixture.operation.kind());
        EXPECT_TRUE(ledger.find(fixture.operation.id).has_value());
        EXPECT_EQ(generation.current(), SemanticGeneration(1U));
        EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(1U));
        if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
            EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
        }
    }
}

template <typename Store>
void expectEmptyReplayReady() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(7U));
    CanonicalCommitClock clock(RuntimeEpoch(3U), CommitOrdinal(11U));
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;

    const ReplayResult result = ReplayCoordinator::replayAndFinalize(
        std::span<const Operation>{}, state, objects, ledger, generation, clock);

    EXPECT_TRUE(result.ready);
    EXPECT_EQ(result.applied, 0U);
    EXPECT_EQ(result.already_applied, 0U);
    EXPECT_FALSE(result.failure.has_value());
    EXPECT_EQ(state, DocumentRuntimeState::kReady);
    EXPECT_EQ(generation.current(), SemanticGeneration(7U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(11U));
    EXPECT_TRUE(objects.allObjects().empty());
}

template <typename Store>
void expectNonLoadingIsFenced(DocumentRuntimeState initial) {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(4U));
    CanonicalCommitClock clock(RuntimeEpoch(5U), CommitOrdinal(9U));
    DocumentRuntimeState state = initial;
    const Operation incoming = insertOperation(101U, 1U);
    const std::vector<Operation> continuation{incoming};

    const ReplayResult result = ReplayCoordinator::replayAndFinalize(
        continuation, state, objects, ledger, generation, clock);

    EXPECT_FALSE(result.ready);
    EXPECT_EQ(result.applied, 0U);
    EXPECT_EQ(result.already_applied, 0U);
    EXPECT_FALSE(result.failure.has_value());
    EXPECT_EQ(state, initial);
    EXPECT_TRUE(objects.allObjects().empty());
    EXPECT_FALSE(ledger.find(incoming.id).has_value());
    EXPECT_EQ(generation.current(), SemanticGeneration(4U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(9U));
}

template <typename Store>
void expectAppliedAndAlreadyAppliedAccounting() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation(SemanticGeneration(10U));
    CanonicalCommitClock clock(RuntimeEpoch(5U), CommitOrdinal(20U));
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    OperationEngine engine;
    const Operation first = insertOperation(102U, 2U);
    ASSERT_EQ(
        engine.apply(first, ApplySource::kLocalInteraction, objects, ledger, generation, clock)
            .disposition,
        ApplyDisposition::kApplied);
    const SemanticGeneration before_replay_generation = generation.current();
    const CommitOrdinal before_replay_ordinal = clock.lastCommittedOrdinal();
    const Operation follow_up = operation(
        SetTransformsOp{{TransformItem{id(2U), Transform2D{2.0, 0.0, 0.0, 2.0, 8.0, 9.0}}}},
        103U);
    const std::vector<Operation> continuation{first, follow_up};

    const ReplayResult result = ReplayCoordinator::replayAndFinalize(
        continuation, state, objects, ledger, generation, clock);

    EXPECT_TRUE(result.ready);
    EXPECT_EQ(result.applied, 1U);
    EXPECT_EQ(result.already_applied, 1U);
    EXPECT_FALSE(result.failure.has_value());
    EXPECT_EQ(state, DocumentRuntimeState::kReady);
    EXPECT_EQ(generation.current(), SemanticGeneration(before_replay_generation.value() + 1U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(before_replay_ordinal.value() + 1U));
    ASSERT_NE(objects.find(id(2U)), nullptr);
    EXPECT_EQ(objects.find(id(2U))->transform.tx, 8.0);
}

template <typename Store>
void expectFailureStopsAtFirstRejectedOperation() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(5U));
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const Operation prefix = insertOperation(104U, 4U);
    const Operation rejected = missingTransform(105U);
    const Operation must_not_run = insertOperation(106U, 6U);
    const std::vector<Operation> continuation{prefix, rejected, must_not_run};

    const ReplayResult result = ReplayCoordinator::replayAndFinalize(
        continuation, state, objects, ledger, generation, clock);

    EXPECT_FALSE(result.ready);
    EXPECT_EQ(result.applied, 1U);
    EXPECT_EQ(result.already_applied, 0U);
    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(result.failure->operation_index, 1U);
    EXPECT_EQ(result.failure->operation_id, rejected.id);
    EXPECT_EQ(result.failure->disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(state, DocumentRuntimeState::kFailed);
    EXPECT_EQ(generation.current(), SemanticGeneration(1U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(1U));
    EXPECT_TRUE(objects.contains(id(4U)));
    EXPECT_FALSE(objects.contains(id(6U)));
    EXPECT_TRUE(ledger.find(prefix.id).has_value());
    EXPECT_FALSE(ledger.find(rejected.id).has_value());
    EXPECT_FALSE(ledger.find(must_not_run.id).has_value());
}

template <typename Store>
void expectCommitBlockedIsFirstFailure(CommitBlockReason reason, CanonicalCommitClock clock) {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const Operation incoming = insertOperation(107U, 7U);
    const std::vector<Operation> continuation{incoming, insertOperation(108U, 8U)};

    const ReplayResult result = ReplayCoordinator::replayAndFinalize(
        continuation, state, objects, ledger, generation, clock);

    EXPECT_FALSE(result.ready);
    EXPECT_EQ(result.applied, 0U);
    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(result.failure->operation_index, 0U);
    EXPECT_EQ(result.failure->operation_id, incoming.id);
    EXPECT_EQ(result.failure->disposition, ApplyDisposition::kCommitBlocked);
    EXPECT_EQ(state, DocumentRuntimeState::kFailed);
    EXPECT_FALSE(objects.contains(id(7U)));
    EXPECT_FALSE(objects.contains(id(8U)));
    EXPECT_FALSE(ledger.find(incoming.id).has_value());
    EXPECT_EQ(reason, CommitBlockReason::kInvalidRuntimeEpoch == reason
                         ? CommitBlockReason::kInvalidRuntimeEpoch
                         : CommitBlockReason::kCommitLaneExhausted);
}

template <typename Store>
void expectDeleteClosureReplay() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(8U));
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const ObjectRecord root = group(20U);
    const ObjectRecord parent = group(21U, root.id);
    ObjectRecord child = shape(22U);
    child.placement.parent_id = parent.id;
    const ObjectRecord parent_connector = attachedConnector(30U, parent.id);
    const ObjectRecord child_connector = attachedConnector(31U, child.id);
    const ObjectRecord sentinel = shape(99U, 99U);
    for (const ObjectRecord& record : {root, parent, child, parent_connector, child_connector, sentinel}) {
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(objects, record));
    }
    const Operation deletion = operation(DeleteObjectsOp{{root.id}}, 109U);
    const std::vector<Operation> continuation{deletion};

    const ReplayResult result = ReplayCoordinator::replayAndFinalize(
        continuation, state, objects, ledger, generation, clock);

    ASSERT_TRUE(result.ready);
    EXPECT_EQ(result.applied, 1U);
    EXPECT_EQ(state, DocumentRuntimeState::kReady);
    EXPECT_FALSE(objects.contains(root.id));
    EXPECT_FALSE(objects.contains(parent.id));
    EXPECT_FALSE(objects.contains(child.id));
    EXPECT_FALSE(objects.contains(parent_connector.id));
    EXPECT_FALSE(objects.contains(child_connector.id));
    EXPECT_TRUE(objects.contains(sentinel.id));
    ASSERT_TRUE(ledger.find(deletion.id).has_value());
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
    }
}

} // namespace

TEST(G106Replay, EmptyLoadingContinuationBecomesReadyForBothProviders) {
    expectEmptyReplayReady<ReferenceObjectStore>();
    expectEmptyReplayReady<IndexedObjectStore>();
}

TEST(G106Replay, EveryNonLoadingLifecycleStateIsPreservedWithoutApplying) {
    constexpr std::array<DocumentRuntimeState, 6> states{
        DocumentRuntimeState::kConstructed, DocumentRuntimeState::kReady,
        DocumentRuntimeState::kSuspended, DocumentRuntimeState::kClosing,
        DocumentRuntimeState::kClosed, DocumentRuntimeState::kFailed};
    for (const DocumentRuntimeState state : states) {
        expectNonLoadingIsFenced<ReferenceObjectStore>(state);
        expectNonLoadingIsFenced<IndexedObjectStore>(state);
    }
}

TEST(G106Replay, AppliedAndAlreadyAppliedCountsAdvanceOnlyAppliedState) {
    expectAppliedAndAlreadyAppliedAccounting<ReferenceObjectStore>();
    expectAppliedAndAlreadyAppliedAccounting<IndexedObjectStore>();
}

TEST(G106Replay, FirstRejectedOperationFailsAfterPreservingSuccessfulPrefix) {
    expectFailureStopsAtFirstRejectedOperation<ReferenceObjectStore>();
    expectFailureStopsAtFirstRejectedOperation<IndexedObjectStore>();
}

TEST(G106Replay, InvalidRuntimeEpochAndExhaustedCommitLaneFailClosed) {
    expectCommitBlockedIsFirstFailure<ReferenceObjectStore>(
        CommitBlockReason::kInvalidRuntimeEpoch, CanonicalCommitClock{});
    expectCommitBlockedIsFirstFailure<IndexedObjectStore>(
        CommitBlockReason::kInvalidRuntimeEpoch, CanonicalCommitClock{});
    const CanonicalCommitClock exhausted(
        RuntimeEpoch(8U), CommitOrdinal(std::numeric_limits<std::uint64_t>::max()));
    expectCommitBlockedIsFirstFailure<ReferenceObjectStore>(
        CommitBlockReason::kCommitLaneExhausted, exhausted);
    const CanonicalCommitClock exhausted_indexed(
        RuntimeEpoch(8U), CommitOrdinal(std::numeric_limits<std::uint64_t>::max()));
    expectCommitBlockedIsFirstFailure<IndexedObjectStore>(
        CommitBlockReason::kCommitLaneExhausted, exhausted_indexed);
}

TEST(G106Replay, ResolvedDeleteClosureUsesCanonicalEngineAndPreservesSentinel) {
    expectDeleteClosureReplay<ReferenceObjectStore>();
    expectDeleteClosureReplay<IndexedObjectStore>();
}

TEST(G106Replay, AllReleasedOperationFamiliesReplayThroughRestoreSource) {
    expectAllReleasedFamiliesReplay<ReferenceObjectStore>();
    expectAllReleasedFamiliesReplay<IndexedObjectStore>();
}

} // namespace canvas::semantic
