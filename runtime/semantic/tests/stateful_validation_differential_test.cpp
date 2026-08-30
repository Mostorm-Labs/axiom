#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

constexpr std::uint32_t kRandomSeed = 0xB9D1FF01U;
constexpr std::size_t kRandomTraceBound = 256U;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

Placement placement(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    return {parent, OrderKey({static_cast<std::uint8_t>(value ? value : 1U)})};
}

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kShape;
    result.kind_version = 1U;
    result.placement = placement(value, parent);
    result.content = ShapeContent{1U, 10.0, 20.0};
    return result;
}

ObjectRecord group(std::uint64_t value) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kGroup;
    result.kind_version = 1U;
    result.placement = placement(value);
    result.content = GroupContent{};
    return result;
}

VectorPathGeometry path() {
    return {FillRule::kNonZero, {MoveTo{{0.0, 0.0}}, LineTo{{1.0, 1.0}}}};
}

ObjectRecord vectorPath(std::uint64_t value) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kVectorPath;
    result.kind_version = 1U;
    result.placement = placement(value);
    result.content = VectorPathContent{path()};
    return result;
}

ImageContent imageContent() {
    return {ResourceId{id(800U)}, 100.0, 80.0, std::nullopt, ImageContentMode::kFit, 30.0, 40.0};
}

ObjectRecord image(std::uint64_t value) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kImage;
    result.kind_version = 1U;
    result.placement = placement(value);
    result.content = imageContent();
    return result;
}

StrokeRecord vectorStrokeRecord() {
    StrokeRecord stroke{};
    stroke.brush.brush_family_id = 1U;
    stroke.brush.brush_version = 1U;
    stroke.brush.color = {0.0F, 0.0F, 0.0F, 1.0F};
    stroke.brush.nominal_size = 1.0;
    stroke.brush.opacity = 1.0F;
    stroke.data = VectorStrokeData{{StrokeSample{{1.0, 2.0}, 1.0F, {0.0, 0.0}}}};
    return stroke;
}

ObjectRecord vectorStroke(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kVectorStroke;
    result.kind_version = 1U;
    result.placement = placement(value, parent);
    result.content = VectorStrokeContent{vectorStrokeRecord()};
    return result;
}

EraseMaskRecord mask(std::uint64_t value) {
    const EraseCubicSegment segment{
        EraseKnot{{0.0, 0.0}, 1.0},
        EraseKnot{{1.0, 1.0}, 1.0},
        {0.5, 0.5},
        {0.5, 0.5}};
    return {id(value), SweptCircleMask{{segment}}};
}

ParagraphStyle paragraphStyle() {
    return {ParagraphAlignment::kLeft, 1.0, 0.0, 0.0};
}

TextStyle textStyle() {
    TextStyle style{};
    style.font_resource_id = ResourceId{id(900U)};
    style.font_size = 12.0;
    style.weight = 400U;
    style.color = {1.0F, 1.0F, 1.0F, 1.0F};
    return style;
}

ObjectRecord richText(std::uint64_t value) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kRichText;
    result.kind_version = 1U;
    result.placement = placement(value);
    Paragraph paragraph{};
    paragraph.id = id(value * 10U);
    paragraph.style = paragraphStyle();
    paragraph.runs = {{"A", textStyle()}};
    result.content = RichTextContent{{{paragraph}}};
    return result;
}

ConnectorContent freeConnectorContent() {
    return {
        ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}},
        ConnectorEndpoint{FreePointEndpoint{{1.0, 1.0}}},
        ConnectorRouting::kStraight};
}

ConnectorContent autoAttachedConnectorContent(ObjectId target) {
    return {
        ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}},
        ConnectorEndpoint{AttachedEndpoint{target, AutoPerimeterAnchor{}}},
        ConnectorRouting::kStraight};
}

ConnectorContent stablePortConnectorContent(ObjectId target, std::uint32_t port_id = 1U) {
    return {
        ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}},
        ConnectorEndpoint{AttachedEndpoint{target, StablePortAnchor{port_id}}},
        ConnectorRouting::kStraight};
}

ObjectRecord connector(std::uint64_t value, std::optional<ObjectId> target = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kConnector;
    result.kind_version = 1U;
    result.placement = placement(value);
    result.content = target.has_value() ? autoAttachedConnectorContent(*target) : freeConnectorContent();
    return result;
}

std::uint64_t operationIdValue(const OperationId& operation_id) {
    const auto& bytes = operation_id.value().bytes;
    std::uint64_t result = 0U;
    for (std::size_t index = 0U; index < sizeof(result); ++index) {
        result |= static_cast<std::uint64_t>(bytes[index]) << (index * 8U);
    }
    return result;
}

Operation operation(OperationPayload payload, std::uint64_t operation_id) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(1000U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

std::string_view operationName(OperationKind kind) {
    switch (kind) {
        case OperationKind::kInsertObjects: return "InsertObjects";
        case OperationKind::kDeleteObjects: return "DeleteObjects";
        case OperationKind::kRestoreObjects: return "RestoreObjects";
        case OperationKind::kSetPlacements: return "SetPlacements";
        case OperationKind::kSetTransforms: return "SetTransforms";
        case OperationKind::kPatchProperties: return "PatchProperties";
        case OperationKind::kSetObjectSize: return "SetObjectSize";
        case OperationKind::kSetVectorPathGeometry: return "SetVectorPathGeometry";
        case OperationKind::kSetImageContent: return "SetImageContent";
        case OperationKind::kAddStroke: return "AddStroke";
        case OperationKind::kSplitStrokes: return "SplitStrokes";
        case OperationKind::kAddEraseMasks: return "AddEraseMasks";
        case OperationKind::kRemoveEraseMasks: return "RemoveEraseMasks";
        case OperationKind::kEditRichText: return "EditRichText";
        case OperationKind::kSetConnectorContent: return "SetConnectorContent";
    }
    return "Unknown";
}

std::vector<ObjectRecord> baseRecords() {
    auto stroke = vectorStroke(5U);
    stroke.erase_masks = {mask(50U)};
    return {
        group(1U),
        shape(2U, id(1U)),
        vectorPath(3U),
        image(4U),
        stroke,
        richText(6U),
        connector(7U),
        connector(8U, id(2U)),
    };
}

template <typename Store>
void seedStore(Store& store) {
    for (const auto& record : baseRecords()) {
        if (!internal::ObjectStoreMutator::insertFresh(store, record)) {
            ADD_FAILURE() << "failed to seed ObjectStore fixture";
        }
    }
}

class Applied final : public AppliedOperationView {
  public:
    std::optional<AppliedOperationEntry> find(const OperationId& operation_id) const override {
        ++lookups;
        const auto it = entries.find(operation_id);
        return it == entries.end() ? std::nullopt
                                   : std::optional<AppliedOperationEntry>(it->second);
    }

    std::map<OperationId, AppliedOperationEntry> entries;
    mutable std::size_t lookups = 0U;
};

struct ReadCounters final {
    std::size_t size = 0U;
    std::size_t contains = 0U;
    std::size_t find = 0U;
    std::size_t all_objects = 0U;
    std::size_t children = 0U;

    bool operator==(const ReadCounters&) const = default;
};

class CountingStore final : public ObjectStore {
  public:
    explicit CountingStore(const ObjectStore& delegate) : delegate_(delegate) {}

    std::size_t size() const noexcept override {
        ++counters_.size;
        return delegate_.size();
    }

    bool contains(const ObjectId& object_id) const noexcept override {
        ++counters_.contains;
        return delegate_.contains(object_id);
    }

    const ObjectRecord* find(const ObjectId& object_id) const noexcept override {
        ++counters_.find;
        return delegate_.find(object_id);
    }

    std::vector<ObjectRecord> allObjects() const override {
        ++counters_.all_objects;
        return delegate_.allObjects();
    }

    std::vector<ObjectRecord> children(const std::optional<ObjectId>& parent_id) const override {
        ++counters_.children;
        return delegate_.children(parent_id);
    }

    void reset() const noexcept { counters_ = {}; }
    [[nodiscard]] ReadCounters counters() const noexcept { return counters_; }

  private:
    const ObjectStore& delegate_;
    mutable ReadCounters counters_{};
};

struct Fixture final {
    std::string id;
    Operation current;
    std::optional<Operation> prior_applied;
    std::vector<std::optional<ObjectId>> relevant_parents;
    bool indexed_single_target = false;
};

std::vector<Fixture> buildFixtures() {
    std::vector<Fixture> fixtures;
    fixtures.push_back({"insert-absent", operation(InsertObjectsOp{{shape(100U)}}, 100U), std::nullopt, {std::nullopt}, false});
    fixtures.push_back({"delete-hierarchy-connector-cascade", operation(DeleteObjectsOp{{id(1U)}}, 101U), std::nullopt, {std::nullopt, id(1U)}, false});
    fixtures.push_back({"restore-staged-connector-target", operation(RestoreObjectsOp{{connector(202U, id(203U)), shape(203U)}}, 102U), std::nullopt, {std::nullopt}, false});
    fixtures.push_back({"placement-hierarchy", operation(SetPlacementsOp{{{id(2U), Placement{id(1U), OrderKey({22U})}}}}, 103U), std::nullopt, {std::nullopt, id(1U)}, false});
    fixtures.push_back({"transform-single-target", operation(SetTransformsOp{{{id(2U), Transform2D{2.0, 0.0, 0.0, 2.0, 4.0, 5.0}}}}, 104U), std::nullopt, {id(1U)}, true});
    fixtures.push_back({"property-single-target", operation(PatchPropertiesOp{{{id(2U), 1U, PropertyPatchAction::kSet, PropertyValue{false}}}}, 105U), std::nullopt, {id(1U)}, true});
    fixtures.push_back({"size-single-target", operation(SetObjectSizeOp{{{id(2U), 20.0, 30.0}}}, 106U), std::nullopt, {id(1U)}, true});
    fixtures.push_back({"vector-path-single-target", operation(SetVectorPathGeometryOp{id(3U), path()}, 107U), std::nullopt, {std::nullopt}, true});
    fixtures.push_back({"image-single-target", operation(SetImageContentOp{id(4U), imageContent()}, 108U), std::nullopt, {std::nullopt}, true});
    fixtures.push_back({"add-stroke-absent", operation(AddStrokeOp{vectorStroke(102U)}, 109U), std::nullopt, {std::nullopt}, false});
    fixtures.push_back({"split-stroke", operation(SplitStrokesOp{{{id(5U), {vectorStroke(103U), vectorStroke(104U)}}}}, 110U), std::nullopt, {std::nullopt}, false});
    fixtures.push_back({"add-erase-mask", operation(AddEraseMasksOp{{{id(5U), {mask(51U)}}}}, 111U), std::nullopt, {std::nullopt}, true});
    fixtures.push_back({"remove-erase-mask", operation(RemoveEraseMasksOp{{{id(5U), {id(50U)}}}}, 112U), std::nullopt, {std::nullopt}, true});
    fixtures.push_back({"edit-rich-text", operation(EditRichTextOp{id(6U), RichTextDelta{1U, {InsertTextStep{id(60U), 0U, "X", textStyle()}}}}, 113U), std::nullopt, {std::nullopt}, true});
    fixtures.push_back({"connector-stable-port", operation(SetConnectorContentOp{id(7U), stablePortConnectorContent(id(2U), 1U)}, 114U), std::nullopt, {std::nullopt, id(1U)}, true});

    const auto already_applied = operation(InsertObjectsOp{{shape(300U)}}, 115U);
    fixtures.push_back({"already-applied", already_applied, already_applied, {std::nullopt}, false});

    const auto collision_incoming = operation(DeleteObjectsOp{{id(2U)}}, 116U);
    const auto collision_prior = operation(InsertObjectsOp{{shape(301U)}}, 116U);
    fixtures.push_back({"operation-id-collision", collision_incoming, collision_prior, {std::nullopt, id(1U)}, false});

    fixtures.push_back({"missing-target-state-rejection", operation(SetTransformsOp{{{id(999U), Transform2D{2.0, 0.0, 0.0, 2.0, 4.0, 5.0}}}}, 117U), std::nullopt, {std::nullopt}, true});
    fixtures.push_back({"missing-parent-state-rejection", operation(SetPlacementsOp{{{id(2U), Placement{id(999U), OrderKey({23U})}}}}, 118U), std::nullopt, {std::nullopt, id(1U)}, false});
    fixtures.push_back({"missing-connector-target-state-rejection", operation(SetConnectorContentOp{id(7U), stablePortConnectorContent(id(999U), 1U)}, 119U), std::nullopt, {std::nullopt}, true});
    return fixtures;
}

struct DifferentialObservation final {
    std::string fixture_id;
    std::size_t operation_index = 0U;
    std::uint64_t operation_id_value = 0U;
    OperationKind kind = OperationKind::kInsertObjects;
    PrepareResult reference_result{};
    PrepareResult indexed_result{};
    std::vector<ObjectRecord> reference_before;
    std::vector<ObjectRecord> reference_after;
    std::vector<ObjectRecord> indexed_before;
    std::vector<ObjectRecord> indexed_after;
    std::vector<std::vector<ObjectRecord>> reference_children_before;
    std::vector<std::vector<ObjectRecord>> reference_children_after;
    std::vector<std::vector<ObjectRecord>> indexed_children_before;
    std::vector<std::vector<ObjectRecord>> indexed_children_after;
    bool indexed_index_matches_before = false;
    bool indexed_index_matches_after = false;
    ReadCounters indexed_prepare_reads{};
    bool indexed_single_target = false;
};

std::vector<std::vector<ObjectRecord>> captureChildren(
    const ObjectStore& store,
    const std::vector<std::optional<ObjectId>>& parents) {
    std::vector<std::vector<ObjectRecord>> result;
    result.reserve(parents.size());
    for (const auto& parent : parents) result.push_back(store.children(parent));
    return result;
}

DifferentialObservation observe(const Fixture& fixture, std::size_t operation_index) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    seedStore(reference);
    seedStore(indexed);

    Applied reference_applied;
    Applied indexed_applied;
    if (fixture.prior_applied.has_value()) {
        reference_applied.entries.emplace(
            fixture.prior_applied->id,
            AppliedOperationEntry{*fixture.prior_applied, std::nullopt});
        indexed_applied.entries.emplace(
            fixture.prior_applied->id,
            AppliedOperationEntry{*fixture.prior_applied, std::nullopt});
    }

    DifferentialObservation observation{};
    observation.fixture_id = fixture.id;
    observation.operation_index = operation_index;
    observation.operation_id_value = operationIdValue(fixture.current.id);
    observation.kind = fixture.current.kind();
    observation.reference_before = reference.allObjects();
    observation.indexed_before = indexed.allObjects();
    observation.reference_children_before = captureChildren(reference, fixture.relevant_parents);
    observation.indexed_children_before = captureChildren(indexed, fixture.relevant_parents);
    observation.indexed_index_matches_before = internal::ObjectStoreMutator::indexMatchesRebuild(indexed);

    CountingStore counted_indexed(indexed);
    counted_indexed.reset();
    observation.reference_result = OperationEngine{}.prepare(
        fixture.current, StatefulValidationContext{reference, reference_applied});
    observation.indexed_result = OperationEngine{}.prepare(
        fixture.current, StatefulValidationContext{counted_indexed, indexed_applied});
    observation.indexed_prepare_reads = counted_indexed.counters();

    observation.reference_after = reference.allObjects();
    observation.indexed_after = indexed.allObjects();
    observation.reference_children_after = captureChildren(reference, fixture.relevant_parents);
    observation.indexed_children_after = captureChildren(indexed, fixture.relevant_parents);
    observation.indexed_index_matches_after = internal::ObjectStoreMutator::indexMatchesRebuild(indexed);
    observation.indexed_single_target = fixture.indexed_single_target;
    return observation;
}

struct Mismatch final {
    std::string class_name;
    std::string path;
    std::string reference_summary;
    std::string indexed_summary;
};

std::string resultSummary(const PrepareResult& result) {
    std::ostringstream out;
    out << "disposition=" << static_cast<int>(result.disposition)
        << ",issue=" << static_cast<int>(result.error.issue)
        << ",plan=" << (result.plan.has_value() ? "present" : "absent");
    if (result.plan.has_value()) {
        out << ",creates=" << result.plan->creates.size()
            << ",replacements=" << result.plan->replacements.size()
            << ",deletes=" << result.plan->deletes.size()
            << ",delete_closure=" << (result.plan->delete_closure.has_value() ? "present" : "absent");
    }
    return out.str();
}

bool sameOperation(const Operation& first, const Operation& second) {
    return first.id == second.id &&
           first.document_id == second.document_id &&
           first.schema_version == second.schema_version &&
           first.payload_version == second.payload_version &&
           first.payload == second.payload;
}

std::optional<Mismatch> comparePrepareResults(
    const PrepareResult& reference,
    const PrepareResult& indexed) {
    const auto summaries = [&]() {
        return std::pair<std::string, std::string>{resultSummary(reference), resultSummary(indexed)};
    };
    if (reference.disposition != indexed.disposition) {
        const auto [left, right] = summaries();
        return Mismatch{"DISPOSITION_MISMATCH", "PrepareResult.disposition", left, right};
    }
    if (reference.error.issue != indexed.error.issue) {
        const auto [left, right] = summaries();
        return Mismatch{"STATE_ISSUE_MISMATCH", "PrepareResult.error.issue", left, right};
    }
    if (reference.plan.has_value() != indexed.plan.has_value()) {
        const auto [left, right] = summaries();
        return Mismatch{"PLAN_PRESENCE_MISMATCH", "PrepareResult.plan", left, right};
    }
    if (!reference.plan.has_value()) return std::nullopt;

    if (!sameOperation(reference.plan->operation, indexed.plan->operation)) {
        const auto [left, right] = summaries();
        return Mismatch{"PLAN_OPERATION_MISMATCH", "plan.operation", left, right};
    }
    if (reference.plan->creates != indexed.plan->creates) {
        const auto [left, right] = summaries();
        return Mismatch{"PLAN_CREATES_MISMATCH", "plan.creates", left, right};
    }
    if (reference.plan->replacements != indexed.plan->replacements) {
        const auto [left, right] = summaries();
        return Mismatch{"PLAN_REPLACEMENTS_MISMATCH", "plan.replacements", left, right};
    }
    if (reference.plan->deletes != indexed.plan->deletes) {
        const auto [left, right] = summaries();
        return Mismatch{"PLAN_DELETES_MISMATCH", "plan.deletes", left, right};
    }
    if (reference.plan->delete_closure.has_value() != indexed.plan->delete_closure.has_value()) {
        const auto [left, right] = summaries();
        return Mismatch{"DELETE_CLOSURE_PRESENCE_MISMATCH", "plan.delete_closure", left, right};
    }
    if (!reference.plan->delete_closure.has_value()) return std::nullopt;

    const auto& left_closure = *reference.plan->delete_closure;
    const auto& right_closure = *indexed.plan->delete_closure;
    if (left_closure.requested_delete_ids != right_closure.requested_delete_ids) {
        const auto [left, right] = summaries();
        return Mismatch{"DELETE_CLOSURE_REQUESTED_MISMATCH", "delete_closure.requested_delete_ids", left, right};
    }
    if (left_closure.resolved_hierarchy_closure != right_closure.resolved_hierarchy_closure) {
        const auto [left, right] = summaries();
        return Mismatch{"DELETE_CLOSURE_HIERARCHY_MISMATCH", "delete_closure.resolved_hierarchy_closure", left, right};
    }
    if (left_closure.resolved_connector_cascade_closure != right_closure.resolved_connector_cascade_closure) {
        const auto [left, right] = summaries();
        return Mismatch{"DELETE_CLOSURE_CONNECTOR_CASCADE_MISMATCH", "delete_closure.resolved_connector_cascade_closure", left, right};
    }
    if (left_closure.final_delete_set != right_closure.final_delete_set) {
        const auto [left, right] = summaries();
        return Mismatch{"DELETE_CLOSURE_FINAL_SET_MISMATCH", "delete_closure.final_delete_set", left, right};
    }
    return std::nullopt;
}

std::optional<Mismatch> compareObservation(const DifferentialObservation& observation) {
    if (const auto result_mismatch = comparePrepareResults(
            observation.reference_result, observation.indexed_result)) {
        return result_mismatch;
    }
    if (observation.reference_before != observation.indexed_before) {
        return Mismatch{"BASE_PROJECTION_MISMATCH", "canonical.before", "reference-before", "indexed-before"};
    }
    if (observation.reference_before != observation.reference_after ||
        observation.indexed_before != observation.indexed_after) {
        return Mismatch{"PLANNING_MUTATION_OBSERVED", "canonical.before_after", "reference-before-after", "indexed-before-after"};
    }
    if (observation.reference_after != observation.indexed_after) {
        return Mismatch{"INDEX_PROJECTION_MISMATCH", "canonical.after", "reference-after", "indexed-after"};
    }
    if (!observation.indexed_index_matches_before || !observation.indexed_index_matches_after) {
        return Mismatch{"INDEX_INVARIANT_MISMATCH", "ObjectIndex.indexMatchesRebuild", observation.indexed_index_matches_before ? "true" : "false", observation.indexed_index_matches_after ? "true" : "false"};
    }
    if (observation.reference_children_before != observation.indexed_children_before ||
        observation.reference_children_after != observation.indexed_children_after ||
        observation.reference_children_before != observation.reference_children_after ||
        observation.indexed_children_before != observation.indexed_children_after) {
        return Mismatch{"INDEX_PROJECTION_MISMATCH", "children(parent)", "reference-children", "indexed-children"};
    }
    if (observation.indexed_single_target && observation.indexed_prepare_reads.all_objects != 0U) {
        return Mismatch{"INDEXED_SINGLE_TARGET_FULL_ENUMERATION", "lookup.allObjects", "expected=0", "actual>0"};
    }
    return std::nullopt;
}

std::string mismatchDiagnostic(
    const DifferentialObservation& observation,
    const Mismatch& mismatch,
    std::optional<std::uint32_t> seed = std::nullopt) {
    std::ostringstream out;
    out << "first_operation_index=" << observation.operation_index
        << " operation_id=" << observation.operation_id_value
        << " operation=" << operationName(observation.kind)
        << " fixture=" << observation.fixture_id;
    if (seed.has_value()) out << " seed=0x" << std::hex << *seed << std::dec;
    out << " class=" << mismatch.class_name
        << " path=" << mismatch.path
        << " reference=" << mismatch.reference_summary
        << " indexed=" << mismatch.indexed_summary;
    return out.str();
}

void expectParity(
    const DifferentialObservation& observation,
    std::optional<std::uint32_t> seed = std::nullopt) {
    const auto mismatch = compareObservation(observation);
    EXPECT_FALSE(mismatch.has_value())
        << (mismatch.has_value() ? mismatchDiagnostic(observation, *mismatch, seed) : std::string{});
}

std::vector<std::size_t> randomizedFixtureIndices(std::size_t fixture_count) {
    std::mt19937 random(kRandomSeed);
    std::uniform_int_distribution<std::size_t> select(0U, fixture_count - 1U);
    std::vector<std::size_t> indices;
    indices.reserve(kRandomTraceBound);
    for (std::size_t i = 0U; i < kRandomTraceBound; ++i) indices.push_back(select(random));
    return indices;
}

bool deterministicObservationEqual(
    const DifferentialObservation& first,
    const DifferentialObservation& second) {
    return first.fixture_id == second.fixture_id &&
           first.operation_index == second.operation_index &&
           first.operation_id_value == second.operation_id_value &&
           first.kind == second.kind &&
           !comparePrepareResults(first.reference_result, second.reference_result).has_value() &&
           !comparePrepareResults(first.indexed_result, second.indexed_result).has_value() &&
           first.reference_before == second.reference_before &&
           first.reference_after == second.reference_after &&
           first.indexed_before == second.indexed_before &&
           first.indexed_after == second.indexed_after &&
           first.reference_children_before == second.reference_children_before &&
           first.reference_children_after == second.reference_children_after &&
           first.indexed_children_before == second.indexed_children_before &&
           first.indexed_children_after == second.indexed_children_after &&
           first.indexed_index_matches_before == second.indexed_index_matches_before &&
           first.indexed_index_matches_after == second.indexed_index_matches_after &&
           first.indexed_prepare_reads == second.indexed_prepare_reads &&
           first.indexed_single_target == second.indexed_single_target;
}

} // namespace

TEST(StatefulValidationDifferential, HandAuthoredTraceCoversAllFifteenAndStateFamilies) {
    const auto fixtures = buildFixtures();
    ASSERT_GE(fixtures.size(), 20U);

    std::array<bool, 15U> operation_kinds_seen{};
    for (std::size_t i = 0U; i < fixtures.size(); ++i) {
        const auto observation = observe(fixtures[i], i);
        expectParity(observation);
        const auto ordinal = static_cast<std::size_t>(observation.kind);
        ASSERT_GE(ordinal, 1U);
        ASSERT_LE(ordinal, operation_kinds_seen.size());
        operation_kinds_seen[ordinal - 1U] = true;
    }
    for (const bool seen : operation_kinds_seen) EXPECT_TRUE(seen);
    std::cout << "B9_HAND_AUTHORED observations=" << fixtures.size()
              << " operation_names=15\n";

    const auto delete_observation = observe(fixtures[1], 1U);
    ASSERT_EQ(delete_observation.kind, OperationKind::kDeleteObjects);
    ASSERT_TRUE(delete_observation.reference_result.plan.has_value());
    ASSERT_TRUE(delete_observation.indexed_result.plan.has_value());
    ASSERT_TRUE(delete_observation.reference_result.plan->delete_closure.has_value());
    ASSERT_TRUE(delete_observation.indexed_result.plan->delete_closure.has_value());
    EXPECT_FALSE(delete_observation.reference_result.plan->delete_closure->requested_delete_ids.empty());
    EXPECT_FALSE(delete_observation.reference_result.plan->delete_closure->resolved_hierarchy_closure.empty());
    EXPECT_FALSE(delete_observation.reference_result.plan->delete_closure->resolved_connector_cascade_closure.empty());
    EXPECT_FALSE(delete_observation.reference_result.plan->delete_closure->final_delete_set.empty());

    const auto split_observation = observe(fixtures[10], 10U);
    ASSERT_EQ(split_observation.kind, OperationKind::kSplitStrokes);
    ASSERT_TRUE(split_observation.reference_result.plan.has_value());
    ASSERT_TRUE(split_observation.indexed_result.plan.has_value());
    EXPECT_EQ(split_observation.reference_result.plan->deletes, std::vector<ObjectId>({id(5U)}));
    EXPECT_EQ(split_observation.indexed_result.plan->deletes, std::vector<ObjectId>({id(5U)}));
    EXPECT_EQ(split_observation.reference_result.plan->creates.size(), 2U);
    EXPECT_EQ(split_observation.indexed_result.plan->creates.size(), 2U);
    EXPECT_FALSE(split_observation.reference_result.plan->delete_closure.has_value());
    EXPECT_FALSE(split_observation.indexed_result.plan->delete_closure.has_value());
}

TEST(StatefulValidationDifferential, FixedSeedRandomizedTraceRuns256ValidPrepareObservations) {
    const auto fixtures = buildFixtures();
    const auto indices = randomizedFixtureIndices(fixtures.size());
    ASSERT_EQ(indices.size(), kRandomTraceBound);

    for (std::size_t i = 0U; i < indices.size(); ++i) {
        const auto observation = observe(fixtures[indices[i]], i);
        expectParity(observation, kRandomSeed);
    }
    std::cout << "B9_RANDOM seed=0x" << std::hex << kRandomSeed << std::dec
              << " observations=" << indices.size() << " first_mismatch=null\n";
}

TEST(StatefulValidationDifferential, FixedSeedTraceIsDeterministicAcrossRepeatedRuns) {
    const auto fixtures = buildFixtures();
    const auto first_indices = randomizedFixtureIndices(fixtures.size());
    const auto second_indices = randomizedFixtureIndices(fixtures.size());
    ASSERT_EQ(first_indices, second_indices);
    ASSERT_EQ(first_indices.size(), kRandomTraceBound);

    for (std::size_t i = 0U; i < first_indices.size(); ++i) {
        const auto first = observe(fixtures[first_indices[i]], i);
        const auto second = observe(fixtures[second_indices[i]], i);
        EXPECT_TRUE(deterministicObservationEqual(first, second))
            << "seed=0x" << std::hex << kRandomSeed << std::dec
            << " operation_index=" << i
            << " fixture=" << first.fixture_id;
    }
}

TEST(StatefulValidationDifferential, ComparatorDetectsDeliberateTestOnlyDivergence) {
    const auto fixtures = buildFixtures();
    auto observation = observe(fixtures.front(), 0U);
    ASSERT_TRUE(observation.indexed_result.plan.has_value());
    ASSERT_FALSE(observation.indexed_result.plan->creates.empty());
    observation.indexed_result.plan->creates.front().transform.tx += 1.0;

    const auto mismatch = compareObservation(observation);
    ASSERT_TRUE(mismatch.has_value());
    EXPECT_EQ(mismatch->class_name, "PLAN_CREATES_MISMATCH");
    EXPECT_EQ(mismatch->path, "plan.creates");
    const auto diagnostic = mismatchDiagnostic(observation, *mismatch, kRandomSeed);
    EXPECT_NE(diagnostic.find("first_operation_index=0"), std::string::npos);
    EXPECT_NE(diagnostic.find("operation_id=100"), std::string::npos);
    EXPECT_NE(diagnostic.find("operation=InsertObjects"), std::string::npos);
    EXPECT_NE(diagnostic.find("fixture=insert-absent"), std::string::npos);
    EXPECT_NE(diagnostic.find("class=PLAN_CREATES_MISMATCH"), std::string::npos);
    EXPECT_NE(diagnostic.find("path=plan.creates"), std::string::npos);
    EXPECT_NE(diagnostic.find("reference="), std::string::npos);
    EXPECT_NE(diagnostic.find("indexed="), std::string::npos);
}

TEST(StatefulValidationDifferential, IndexedSingleTargetPrepareWindowNeverEnumeratesAllObjects) {
    const auto fixtures = buildFixtures();
    std::size_t asserted = 0U;
    for (std::size_t i = 0U; i < fixtures.size(); ++i) {
        if (!fixtures[i].indexed_single_target) continue;
        const auto observation = observe(fixtures[i], i);
        ++asserted;
        EXPECT_EQ(observation.indexed_prepare_reads.all_objects, 0U)
            << "fixture=" << observation.fixture_id;
        expectParity(observation);
    }
    EXPECT_GE(asserted, 8U);

    ReadCounters aggregate{};
    for (std::size_t i = 0U; i < fixtures.size(); ++i) {
        if (!fixtures[i].indexed_single_target) continue;
        const auto observation = observe(fixtures[i], i);
        aggregate.size += observation.indexed_prepare_reads.size;
        aggregate.contains += observation.indexed_prepare_reads.contains;
        aggregate.find += observation.indexed_prepare_reads.find;
        aggregate.all_objects += observation.indexed_prepare_reads.all_objects;
        aggregate.children += observation.indexed_prepare_reads.children;
    }
    std::cout << "B9_LOOKUP single_target_fixtures=" << asserted
              << " size=" << aggregate.size
              << " contains=" << aggregate.contains
              << " find=" << aggregate.find
              << " allObjects=" << aggregate.all_objects
              << " children=" << aggregate.children << "\n";
}

} // namespace canvas::semantic
