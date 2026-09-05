#include "g1_06_digest.hpp"
#include "g1_06_projection.hpp"

#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/replay.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

using canvas::verification::g1_06::ProjectionDocumentId;
using canvas::verification::g1_06::digestCanonicalProjectionBytes;
using canvas::verification::g1_06::projectDocument;
using canvas::verification::g1_06::writeCanonicalProjectionJson;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

DocumentId documentId() { return DocumentId{id(0xd06U)}; }

ProjectionDocumentId projectionDocumentId() {
    ProjectionDocumentId result;
    std::copy(documentId().value().bytes.begin(), documentId().value().bytes.end(),
              result.bytes.begin());
    return result;
}

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt,
                   std::uint8_t order = 1U) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey({order})};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, static_cast<double>(value),
                                   static_cast<double>(value + 1U)};
    record.properties.entries = {{1U, PropertyValue{false}}};
    record.content = ShapeContent{1U, 10.0 + static_cast<double>(value),
                                  20.0 + static_cast<double>(value)};
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

ObjectRecord richText(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 3U);
    record.kind = ObjectKind::kRichText;
    TextStyle style{};
    style.font_resource_id = ResourceId{id(700U)};
    style.font_size = 14.0;
    style.weight = 700U;
    style.italic = true;
    style.underline = true;
    style.color = ColorValue{0.2F, 0.4F, 0.8F, 1.0F};
    Paragraph paragraph{};
    paragraph.id = id(value * 10U);
    paragraph.style = ParagraphStyle{ParagraphAlignment::kCenter, 1.2, 0.1, 0.2};
    paragraph.runs = {TextRun{"integrated semantic corpus", style}};
    record.content = RichTextContent{RichTextDocument{{paragraph}}};
    return record;
}

EraseMaskRecord eraseMask(std::uint64_t value) {
    return EraseMaskRecord{
        id(value),
        SweptCircleMask{{EraseCubicSegment{
            EraseKnot{Vec2{0.0, 0.0}, 1.0},
            EraseKnot{Vec2{1.0, 1.0}, 1.0},
            Vec2{0.5, 0.5},
            Vec2{0.5, 0.5}}}}};
}

ObjectRecord vectorStroke(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 4U);
    record.kind = ObjectKind::kVectorStroke;
    StrokeRecord stroke{};
    stroke.brush.brush_family_id = 1U;
    stroke.brush.brush_version = 1U;
    stroke.brush.color = ColorValue{0.9F, 0.1F, 0.3F, 1.0F};
    stroke.brush.nominal_size = 6.0;
    stroke.brush.opacity = 0.8F;
    stroke.deterministic_seed = 0x1234U;
    stroke.data = VectorStrokeData{{
        StrokeSample{Vec2{0.0, 0.0}, 1.0F, Vec2{0.0, 0.0}},
        StrokeSample{Vec2{1.0, 2.0}, 1.0F, Vec2{0.0, 0.0}}}};
    record.content = VectorStrokeContent{stroke};
    record.erase_masks = {eraseMask(401U)};
    return record;
}

ObjectRecord vectorPath(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 6U);
    record.kind = ObjectKind::kVectorPath;
    record.content = VectorPathContent{VectorPathGeometry{
        FillRule::kNonZero,
        {MoveTo{Vec2{0.0, 0.0}}, LineTo{Vec2{1.0, 1.0}}}}};
    return record;
}

ObjectRecord image(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 7U);
    record.kind = ObjectKind::kImage;
    record.content = ImageContent{
        ResourceId{id(800U)}, 10.0, 10.0, std::nullopt, ImageContentMode::kFit, 10.0, 10.0};
    return record;
}

ObjectRecord dabStroke(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 5U);
    record.kind = ObjectKind::kDabStroke;
    StrokeRecord stroke{};
    stroke.brush.brush_family_id = 3U;
    stroke.brush.brush_version = 1U;
    stroke.brush.nominal_size = 4.0;
    stroke.brush.opacity = 0.7F;
    stroke.brush.blend_mode = BrushBlendMode::kNormal;
    stroke.brush.texture_resource_id = ResourceId{id(800U)};
    stroke.deterministic_seed = 0x5678U;
    stroke.data = DabStrokeData{{DabInstance{Vec2{3.0, 4.0}, 4.0, 0.25F, 0.7F}}};
    record.content = DabStrokeContent{stroke};
    return record;
}

ObjectRecord connector(std::uint64_t value, ObjectId target) {
    ObjectRecord record = shape(value, std::nullopt, 6U);
    record.kind = ObjectKind::kConnector;
    record.content = ConnectorContent{
        ConnectorEndpoint{AttachedEndpoint{target, StablePortAnchor{3U}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{9.0, 9.0}}},
        ConnectorRouting::kStraight};
    return record;
}

std::vector<ObjectRecord> corpusObjects() {
    const ObjectRecord root = group(1U);
    const ObjectRecord child = shape(2U, root.id, 2U);
    const ObjectRecord text = richText(3U, root.id);
    const ObjectRecord vector = vectorStroke(4U, root.id);
    const ObjectRecord dab = dabStroke(5U, root.id);
    const ObjectRecord edge = connector(10U, child.id);
    const ObjectRecord sentinel = shape(99U, std::nullopt, 99U);
    // Deliberately non-canonical physical order; projection order is the oracle.
    return {edge, text, sentinel, vector, child, dab, root};
}

Operation operation(OperationPayload payload, std::uint64_t operation_id) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = documentId();
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

std::vector<Operation> integratedOperations() {
    const std::vector<ObjectRecord> objects = corpusObjects();
    const EraseMaskRecord extra_mask = eraseMask(402U);
    const ConnectorContent changed_connector{
        ConnectorEndpoint{AttachedEndpoint{id(2U), StablePortAnchor{3U}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{11.0, 12.0}}},
        ConnectorRouting::kOrthogonal};
    return {
        operation(InsertObjectsOp{objects}, 6001U),
        operation(SetTransformsOp{{TransformItem{
            id(2U), Transform2D{1.0, 0.0, 0.0, 1.0, 22.0, 23.0}}}}, 6002U),
        operation(PatchPropertiesOp{{PropertyPatch{
            id(2U), 2U, PropertyPatchAction::kSet, PropertyValue{true}}}}, 6003U),
        operation(SetPlacementsOp{{PlacementItem{id(99U), Placement{id(1U), OrderKey({7U})}}}}, 6004U),
        operation(SetObjectSizeOp{{ObjectSizeItem{id(2U), 44.0, 55.0}}}, 6005U),
        operation(AddEraseMasksOp{{EraseMaskAddItem{id(4U), {extra_mask}}}}, 6006U),
        operation(SetConnectorContentOp{id(10U), changed_connector}, 6007U),
        operation(SetTransformsOp{{TransformItem{
            id(3U), Transform2D{1.0, 0.0, 0.0, 1.0, 33.0, 34.0}}}}, 6008U),
        operation(SetTransformsOp{{TransformItem{
            id(5U), Transform2D{1.0, 0.0, 0.0, 1.0, 55.0, 56.0}}}}, 6009U),
    };
}

std::vector<Operation> allFamilyContinuation() {
    const ObjectId root_id = id(100U);
    const ObjectRecord root = group(100U);
    const ObjectRecord shape_record = shape(101U, root_id, 1U);
    const ObjectRecord vector_path_record = vectorPath(102U, root_id);
    const ObjectRecord image_record = image(103U, root_id);
    const ObjectRecord vector_stroke_record = vectorStroke(104U, root_id);
    const ObjectRecord rich_text_record = richText(105U, root_id);
    const ObjectRecord connector_record = connector(106U, shape_record.id);
    const ObjectRecord deleted_record = shape(107U, root_id, 8U);
    const ObjectRecord added_stroke = vectorStroke(108U, root_id);
    const ObjectRecord split_stroke_a = vectorStroke(109U, root_id);
    const ObjectRecord split_stroke_b = vectorStroke(110U, root_id);
    const EraseMaskRecord added_mask = eraseMask(411U);

    ObjectRecord placed = shape_record;
    placed.placement = Placement{root_id, OrderKey({20U})};
    ObjectRecord transformed = placed;
    transformed.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 101.0, 102.0};
    const VectorPathGeometry changed_geometry{
        FillRule::kEvenOdd, {MoveTo{Vec2{2.0, 3.0}}, LineTo{Vec2{4.0, 5.0}}}};
    const ImageContent changed_image{
        ResourceId{id(801U)}, 44.0, 55.0, std::nullopt, ImageContentMode::kFill, 66.0, 77.0};
    const ConnectorContent changed_connector{
        ConnectorEndpoint{AttachedEndpoint{shape_record.id, StablePortAnchor{3U}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{11.0, 12.0}}}, ConnectorRouting::kOrthogonal};

    return {
        operation(InsertObjectsOp{{root,
                                   shape_record,
                                   vector_path_record,
                                   image_record,
                                   vector_stroke_record,
                                   rich_text_record,
                                   connector_record,
                                   deleted_record}},
                  6101U),
        operation(DeleteObjectsOp{{deleted_record.id}}, 6102U),
        operation(RestoreObjectsOp{{deleted_record}}, 6103U),
        operation(SetPlacementsOp{{PlacementItem{shape_record.id, placed.placement}}}, 6104U),
        operation(SetTransformsOp{{TransformItem{shape_record.id, transformed.transform}}}, 6105U),
        operation(PatchPropertiesOp{{PropertyPatch{
            shape_record.id, 2U, PropertyPatchAction::kSet, PropertyValue{true}}}},
                  6106U),
        operation(SetObjectSizeOp{{ObjectSizeItem{shape_record.id, 44.0, 55.0}}}, 6107U),
        operation(SetVectorPathGeometryOp{vector_path_record.id, changed_geometry}, 6108U),
        operation(SetImageContentOp{image_record.id, changed_image}, 6109U),
        operation(AddStrokeOp{added_stroke}, 6110U),
        operation(SplitStrokesOp{{StrokeSplit{added_stroke.id, {split_stroke_a, split_stroke_b}}}},
                  6111U),
        operation(AddEraseMasksOp{{EraseMaskAddItem{vector_stroke_record.id, {added_mask}}}},
                  6112U),
        operation(RemoveEraseMasksOp{{EraseMaskRemoveItem{vector_stroke_record.id, {added_mask.id}}}},
                  6113U),
        operation(EditRichTextOp{rich_text_record.id, RichTextDelta{}}, 6114U),
        operation(SetConnectorContentOp{connector_record.id, changed_connector}, 6115U)};
}

struct Outcome final {
    bool valid = false;
    bool ready = false;
    std::size_t applied = 0U;
    std::size_t already_applied = 0U;
    std::string projection;
    std::uint64_t digest = 0U;
    SemanticGeneration generation{};
    CommitOrdinal ordinal{};
    bool index_matches = true;
};

template <typename Store>
Outcome summarize(const Store& objects, bool ready, std::size_t applied,
                  std::size_t already_applied, SemanticGeneration generation,
                  CommitOrdinal ordinal) {
    const std::string projection = writeCanonicalProjectionJson(
        projectDocument(projectionDocumentId(), 1U, objects));
    bool index_matches = true;
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        index_matches = internal::ObjectStoreMutator::indexMatchesRebuild(objects);
    }
    return Outcome{true,
                   ready,
                   applied,
                   already_applied,
                   projection,
                   digestCanonicalProjectionBytes(projection),
                   generation,
                   ordinal,
                   index_matches};
}

template <typename Store>
Outcome runSequential(const std::vector<Operation>& operations) {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(42U));
    OperationEngine engine;
    for (const Operation& incoming : operations) {
        const ApplyResult result = engine.apply(
            incoming, ApplySource::kLocalInteraction, objects, ledger, generation, clock);
        if (result.disposition != ApplyDisposition::kApplied) return {};
    }
    return summarize(objects, true, operations.size(), 0U, generation.current(),
                     clock.lastCommittedOrdinal());
}

bool roundTripSnapshot(const SemanticSnapshot& input, SemanticSnapshot& output) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    const CodecResult encoded = SnapshotCodec::encode(input);
    if (!encoded.ok()) return false;
    const SnapshotDecodeResult decoded = SnapshotCodec::decode(encoded.bytes);
    if (!decoded.ok()) return false;
    output = *decoded.snapshot;
    return true;
#else
    output = input;
    return true;
#endif
}

template <typename Store>
SnapshotBootstrapResult restoreSnapshot(const SemanticSnapshot& snapshot,
                                        DocumentRuntimeState& state, Store& objects,
                                        SemanticGenerationState& generation) {
    if constexpr (std::is_same_v<Store, ReferenceObjectStore>) {
        return SnapshotBootstrapper::restore(snapshot, state, objects, generation);
    } else {
        return SnapshotBootstrapper::restore(snapshot, state, objects, generation);
    }
}

template <typename Store>
ReplayResult replayContinuation(std::span<const Operation> continuation,
                                DocumentRuntimeState& state, Store& objects,
                                AppliedOperationLedger& ledger,
                                SemanticGenerationState& generation,
                                CanonicalCommitClock& clock) {
    if constexpr (std::is_same_v<Store, ReferenceObjectStore>) {
        return ReplayCoordinator::replayAndFinalize(
            continuation, state, objects, ledger, generation, clock);
    } else {
        return ReplayCoordinator::replayAndFinalize(
            continuation, state, objects, ledger, generation, clock);
    }
}

template <typename Store>
Outcome runSnapshotReplay(const std::vector<Operation>& operations,
                          std::size_t prefix_count, bool permute_snapshot,
                          bool use_codec) {
    if (prefix_count > operations.size()) return {};

    Store checkpoint_store;
    AppliedOperationLedger checkpoint_ledger;
    SemanticGenerationState checkpoint_generation;
    CanonicalCommitClock checkpoint_clock(RuntimeEpoch(42U));
    OperationEngine engine;
    for (std::size_t index = 0U; index < prefix_count; ++index) {
        const ApplyResult result = engine.apply(operations[index], ApplySource::kLocalInteraction,
                                                checkpoint_store, checkpoint_ledger,
                                                checkpoint_generation, checkpoint_clock);
        if (result.disposition != ApplyDisposition::kApplied) return {};
    }

    SemanticSnapshot snapshot;
    snapshot.document_id = documentId();
    snapshot.schema_version = 1U;
    snapshot.objects = checkpoint_store.allObjects();
    if (permute_snapshot) std::reverse(snapshot.objects.begin(), snapshot.objects.end());
    SemanticSnapshot materialized_snapshot;
    if (use_codec && !roundTripSnapshot(snapshot, materialized_snapshot)) return {};
    if (!use_codec) materialized_snapshot = snapshot;

    Store restored;
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    SemanticGenerationState generation{SemanticGeneration(prefix_count)};
    CanonicalCommitClock clock(RuntimeEpoch(42U), CommitOrdinal(prefix_count));
    AppliedOperationLedger replay_ledger;
    const SnapshotBootstrapResult bootstrap = restoreSnapshot(
        materialized_snapshot, state, restored, generation);
    if (!bootstrap.restored) return {};

    const std::span<const Operation> suffix(operations.data() + prefix_count,
                                            operations.size() - prefix_count);
    const ReplayResult replay = replayContinuation(
        suffix, state, restored, replay_ledger, generation, clock);
    if (!replay.ready || replay.failure.has_value()) {
        std::fprintf(stderr, "replay failed prefix=%zu ready=%d applied=%zu failure=%d\\n", prefix_count,
                     replay.ready ? 1 : 0, replay.applied, replay.failure.has_value() ? 1 : 0);
        return {};
    }
    return summarize(restored, replay.ready, replay.applied, replay.already_applied,
                     generation.current(), clock.lastCommittedOrdinal());
}

template <typename Store>
void expectSameStateOutcome(const Outcome& expected, const Outcome& actual) {
    ASSERT_TRUE(expected.valid);
    ASSERT_TRUE(actual.valid);
    EXPECT_TRUE(actual.ready);
    EXPECT_EQ(actual.projection, expected.projection);
    EXPECT_EQ(actual.digest, expected.digest);
    EXPECT_EQ(actual.generation, expected.generation);
    EXPECT_EQ(actual.ordinal, expected.ordinal);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>) {
        EXPECT_TRUE(actual.index_matches);
    }
}

template <typename Store>
void expectAlreadyAppliedReplay() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(42U));
    OperationEngine engine;
    const Operation first = operation(InsertObjectsOp{{shape(700U)}}, 6700U);
    ASSERT_EQ(engine.apply(first, ApplySource::kLocalInteraction, objects, ledger, generation, clock)
                  .disposition,
              ApplyDisposition::kApplied);
    const SemanticGeneration before_generation = generation.current();
    const CommitOrdinal before_ordinal = clock.lastCommittedOrdinal();
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const ReplayResult replay = ReplayCoordinator::replayAndFinalize(
        std::vector<Operation>{first}, state, objects, ledger, generation, clock);
    EXPECT_TRUE(replay.ready);
    EXPECT_EQ(replay.applied, 0U);
    EXPECT_EQ(replay.already_applied, 1U);
    EXPECT_FALSE(replay.failure.has_value());
    EXPECT_EQ(generation.current(), before_generation);
    EXPECT_EQ(clock.lastCommittedOrdinal(), before_ordinal);
}

template <typename Store>
void expectRejectedReplayStopsAndPreservesState() {
    Store objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(42U));
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    const Operation prefix = operation(InsertObjectsOp{{shape(701U)}}, 6701U);
    const Operation rejected = operation(
        SetTransformsOp{{TransformItem{id(9999U), Transform2D{2.0, 0.0, 0.0, 2.0, 1.0, 1.0}}}},
        6702U);
    const Operation later = operation(InsertObjectsOp{{shape(702U)}}, 6703U);
    const std::vector<Operation> continuation{prefix, rejected, later};
    const ReplayResult replay = ReplayCoordinator::replayAndFinalize(
        continuation, state, objects, ledger, generation, clock);
    EXPECT_FALSE(replay.ready);
    EXPECT_EQ(replay.applied, 1U);
    ASSERT_TRUE(replay.failure.has_value());
    EXPECT_EQ(replay.failure->operation_index, 1U);
    EXPECT_EQ(replay.failure->operation_id, rejected.id);
    EXPECT_EQ(replay.failure->disposition, ApplyDisposition::kRejected);
    EXPECT_EQ(state, DocumentRuntimeState::kFailed);
    EXPECT_TRUE(objects.contains(id(701U)));
    EXPECT_FALSE(objects.contains(id(702U)));
    EXPECT_EQ(generation.current(), SemanticGeneration(1U));
    EXPECT_EQ(clock.lastCommittedOrdinal(), CommitOrdinal(1U));
}

TEST(G106Integration, IntegratedRouteProviderCorpusHasProjectionFirstFourWayEquality) {
    const std::vector<Operation> operations = integratedOperations();
    const Outcome direct_reference = runSequential<ReferenceObjectStore>(operations);
    const Outcome direct_indexed = runSequential<IndexedObjectStore>(operations);
    ASSERT_TRUE(direct_reference.valid);
    ASSERT_TRUE(direct_indexed.valid);
    ASSERT_EQ(direct_reference.projection, direct_indexed.projection);
    ASSERT_EQ(direct_reference.digest, direct_indexed.digest);
    ASSERT_EQ(direct_reference.generation, direct_indexed.generation);
    ASSERT_EQ(direct_reference.ordinal, direct_indexed.ordinal);
    EXPECT_TRUE(direct_indexed.index_matches);

    for (const std::size_t checkpoint : {std::size_t{1U}, std::size_t{3U}, std::size_t{6U}}) {
        const Outcome replay_reference = runSnapshotReplay<ReferenceObjectStore>(
            operations, checkpoint, false, checkpoint == 1U);
        const Outcome replay_indexed = runSnapshotReplay<IndexedObjectStore>(
            operations, checkpoint, checkpoint == 3U, false);
        expectSameStateOutcome<ReferenceObjectStore>(direct_reference, replay_reference);
        expectSameStateOutcome<IndexedObjectStore>(direct_reference, replay_indexed);
    }
}

TEST(G106Integration, FullFifteenFamilyContinuationReplaysInOneInvocationWithFourWayProjectionEquality) {
    const std::vector<Operation> operations = allFamilyContinuation();
    ASSERT_EQ(operations.size(), 15U);
    for (std::size_t index = 0U; index < operations.size(); ++index) {
        EXPECT_EQ(operations[index].payload.index(), index);
    }

    const Outcome direct_reference = runSequential<ReferenceObjectStore>(operations);
    const Outcome direct_indexed = runSequential<IndexedObjectStore>(operations);
    const Outcome replay_reference = runSnapshotReplay<ReferenceObjectStore>(
        operations, 0U, false, true);
    const Outcome replay_indexed = runSnapshotReplay<IndexedObjectStore>(
        operations, 0U, false, true);

    ASSERT_TRUE(direct_reference.valid);
    ASSERT_TRUE(direct_indexed.valid);
    ASSERT_TRUE(replay_reference.valid);
    ASSERT_TRUE(replay_indexed.valid);
    EXPECT_EQ(direct_reference.applied, 15U);
    EXPECT_EQ(direct_indexed.applied, 15U);
    EXPECT_EQ(replay_reference.applied, 15U);
    EXPECT_EQ(replay_indexed.applied, 15U);
    EXPECT_EQ(direct_reference.projection, direct_indexed.projection);
    EXPECT_EQ(direct_reference.projection, replay_reference.projection);
    EXPECT_EQ(direct_reference.projection, replay_indexed.projection);
    EXPECT_EQ(direct_reference.digest, direct_indexed.digest);
    EXPECT_EQ(direct_reference.digest, replay_reference.digest);
    EXPECT_EQ(direct_reference.digest, replay_indexed.digest);
    EXPECT_EQ(direct_reference.generation, SemanticGeneration(15U));
    EXPECT_EQ(direct_reference.ordinal, CommitOrdinal(15U));
    EXPECT_EQ(direct_indexed.generation, direct_reference.generation);
    EXPECT_EQ(replay_reference.generation, direct_reference.generation);
    EXPECT_EQ(replay_indexed.generation, direct_reference.generation);
    EXPECT_EQ(direct_indexed.ordinal, direct_reference.ordinal);
    EXPECT_EQ(replay_reference.ordinal, direct_reference.ordinal);
    EXPECT_EQ(replay_indexed.ordinal, direct_reference.ordinal);
    EXPECT_TRUE(direct_indexed.index_matches);
    EXPECT_TRUE(replay_indexed.index_matches);

    for (const std::size_t checkpoint : {std::size_t{5U}, std::size_t{10U},
                                         std::size_t{15U}}) {
        const Outcome checkpoint_reference = runSnapshotReplay<ReferenceObjectStore>(
            operations, checkpoint, checkpoint == 10U, true);
        const Outcome checkpoint_indexed = runSnapshotReplay<IndexedObjectStore>(
            operations, checkpoint, checkpoint == 10U, true);
        expectSameStateOutcome<ReferenceObjectStore>(direct_reference, checkpoint_reference);
        expectSameStateOutcome<IndexedObjectStore>(direct_reference, checkpoint_indexed);
        EXPECT_EQ(checkpoint_reference.applied, operations.size() - checkpoint);
        EXPECT_EQ(checkpoint_indexed.applied, operations.size() - checkpoint);
    }
}

TEST(G106Integration, EmptySnapshotAndEmptyAuthoritativeContinuationRemainReady) {
    const Outcome outcome = runSnapshotReplay<ReferenceObjectStore>({}, 0U, false, false);
    ASSERT_TRUE(outcome.valid);
    EXPECT_TRUE(outcome.ready);
    EXPECT_EQ(outcome.projection,
              writeCanonicalProjectionJson(projectDocument(projectionDocumentId(), 1U,
                                                            ReferenceObjectStore{})));
    EXPECT_EQ(outcome.generation, SemanticGeneration(0U));
    EXPECT_EQ(outcome.ordinal, CommitOrdinal(0U));
}

TEST(G106Integration, OneObjectMinimumAndPhysicalInsertionPermutationHaveCanonicalParity) {
    const std::vector<Operation> operations = {
        operation(InsertObjectsOp{{shape(710U)}}, 6710U)};
    const Outcome direct = runSequential<ReferenceObjectStore>(operations);
    const Outcome permutation = runSnapshotReplay<ReferenceObjectStore>(operations, 1U, true, false);
    ASSERT_TRUE(direct.valid);
    expectSameStateOutcome<ReferenceObjectStore>(direct, permutation);
}

TEST(G106Integration, RestoreObjectsContinuationWorksWithoutTombstoneLedger) {
    const std::vector<Operation> operations = {
        operation(RestoreObjectsOp{{shape(720U)}}, 6720U)};
    const Outcome direct = runSequential<ReferenceObjectStore>(operations);
    const Outcome replay = runSnapshotReplay<ReferenceObjectStore>(operations, 0U, false, false);
    ASSERT_TRUE(direct.valid);
    expectSameStateOutcome<ReferenceObjectStore>(direct, replay);
}

TEST(G106Integration, AlreadyAppliedReplayDoesNotAdvanceGenerationOrCommitOrdinal) {
    expectAlreadyAppliedReplay<ReferenceObjectStore>();
    expectAlreadyAppliedReplay<IndexedObjectStore>();
}

TEST(G106Integration, SuppliedRejectedOperationFailsClosedAndDoesNotRunLaterItems) {
    expectRejectedReplayStopsAndPreservesState<ReferenceObjectStore>();
    expectRejectedReplayStopsAndPreservesState<IndexedObjectStore>();
}

} // namespace
} // namespace canvas::semantic
