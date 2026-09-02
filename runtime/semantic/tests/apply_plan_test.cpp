#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/staged_object_view.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

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

ObjectRecord connector(std::uint64_t value, std::optional<ObjectId> target = std::nullopt) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kConnector;
    result.kind_version = 1U;
    result.placement = placement(value);
    if (target.has_value()) {
        result.content = ConnectorContent{
            ConnectorEndpoint{FreePointEndpoint{{0.0, 0.0}}},
            ConnectorEndpoint{AttachedEndpoint{*target, AutoPerimeterAnchor{}}},
            ConnectorRouting::kStraight};
    } else {
        result.content = freeConnectorContent();
    }
    return result;
}

template <typename Store>
void insert(Store& store, const ObjectRecord& record) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
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

class CountingStore final : public ObjectStore {
  public:
    explicit CountingStore(const ObjectStore& delegate) : delegate_(delegate) {}

    std::size_t size() const noexcept override {
        ++size_calls;
        return delegate_.size();
    }
    bool contains(const ObjectId& object_id) const noexcept override {
        ++contains_calls;
        return delegate_.contains(object_id);
    }
    const ObjectRecord* find(const ObjectId& object_id) const noexcept override {
        ++find_calls;
        return delegate_.find(object_id);
    }
    std::vector<ObjectRecord> allObjects() const override {
        ++all_objects_calls;
        return delegate_.allObjects();
    }
    std::vector<ObjectRecord> children(const std::optional<ObjectId>& parent_id) const override {
        ++children_calls;
        return delegate_.children(parent_id);
    }

    [[nodiscard]] std::size_t totalReads() const noexcept {
        return size_calls + contains_calls + find_calls + all_objects_calls + children_calls;
    }

    mutable std::size_t size_calls = 0U;
    mutable std::size_t contains_calls = 0U;
    mutable std::size_t find_calls = 0U;
    mutable std::size_t all_objects_calls = 0U;
    mutable std::size_t children_calls = 0U;

  private:
    const ObjectStore& delegate_;
};

Operation operation(OperationPayload payload, std::uint64_t operation_id = 1U) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = DocumentId{id(1000U)};
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

void expectPreparedInvariant(const PrepareResult& result) {
    EXPECT_EQ(result.disposition, PrepareDisposition::kPrepared);
    EXPECT_EQ(result.error.issue, StatefulIssue::kNone);
    EXPECT_TRUE(result.plan.has_value());
}

void expectDeleteClosureEqual(const DeleteClosure& actual, const DeleteClosure& expected) {
    EXPECT_EQ(actual.requested_delete_ids, expected.requested_delete_ids);
    EXPECT_EQ(actual.resolved_hierarchy_closure, expected.resolved_hierarchy_closure);
    EXPECT_EQ(actual.resolved_connector_cascade_closure,
              expected.resolved_connector_cascade_closure);
    EXPECT_EQ(actual.final_delete_set, expected.final_delete_set);
}

void expectEquivalentPlans(const PrepareResult& first, const PrepareResult& second) {
    EXPECT_EQ(first.disposition, second.disposition);
    EXPECT_EQ(first.error.issue, second.error.issue);
    ASSERT_EQ(first.plan.has_value(), second.plan.has_value());
    if (!first.plan.has_value()) return;

    EXPECT_EQ(first.plan->operation.id, second.plan->operation.id);
    EXPECT_EQ(first.plan->operation.document_id, second.plan->operation.document_id);
    EXPECT_EQ(first.plan->operation.schema_version, second.plan->operation.schema_version);
    EXPECT_EQ(first.plan->operation.payload_version, second.plan->operation.payload_version);
    EXPECT_EQ(first.plan->operation.payload, second.plan->operation.payload);
    EXPECT_EQ(first.plan->creates, second.plan->creates);
    EXPECT_EQ(first.plan->replacements, second.plan->replacements);
    EXPECT_EQ(first.plan->deletes, second.plan->deletes);
    ASSERT_EQ(first.plan->delete_closure.has_value(), second.plan->delete_closure.has_value());
    if (first.plan->delete_closure.has_value()) {
        expectDeleteClosureEqual(*first.plan->delete_closure, *second.plan->delete_closure);
    }
}

} // namespace

TEST(ApplyPlan, AlreadyAppliedShortCircuitsBeforeObjectStoreLookup) {
    ReferenceObjectStore base;
    CountingStore store(base);
    Applied applied;
    const auto current = operation(InsertObjectsOp{{shape(101U)}}, 10U);
    applied.entries.emplace(current.id, AppliedOperationEntry{current, std::nullopt});

    const auto result = prepareApplyPlan(current, StatefulValidationContext{store, applied});

    EXPECT_EQ(result.disposition, PrepareDisposition::kAlreadyApplied);
    EXPECT_EQ(result.error.issue, StatefulIssue::kNone);
    EXPECT_FALSE(result.plan.has_value());
    EXPECT_EQ(applied.lookups, 1U);
    EXPECT_EQ(store.totalReads(), 0U);
}

TEST(ApplyPlan, CollisionShortCircuitsBeforeObjectStoreLookupWithExactIssue) {
    ReferenceObjectStore base;
    CountingStore store(base);
    Applied applied;
    const auto existing = operation(InsertObjectsOp{{shape(101U)}}, 11U);
    applied.entries.emplace(existing.id, AppliedOperationEntry{existing, std::nullopt});
    const auto incoming = operation(DeleteObjectsOp{{id(101U)}}, 11U);

    const auto result = prepareApplyPlan(incoming, StatefulValidationContext{store, applied});

    EXPECT_EQ(result.disposition, PrepareDisposition::kRejected);
    EXPECT_EQ(result.error.issue, StatefulIssue::kOperationIdCollision);
    EXPECT_FALSE(result.plan.has_value());
    EXPECT_EQ(applied.lookups, 1U);
    EXPECT_EQ(store.totalReads(), 0U);
}

TEST(ApplyPlan, LowerLevelRejectionPreservesIssueAndLeaksNoPartialPlan) {
    ReferenceObjectStore store;
    insert(store, shape(1U));
    Applied applied;
    const auto incoming = operation(InsertObjectsOp{{shape(1U), shape(2U)}}, 12U);

    const auto result = prepareApplyPlan(incoming, StatefulValidationContext{store, applied});

    EXPECT_EQ(result.disposition, PrepareDisposition::kRejected);
    EXPECT_EQ(result.error.issue, StatefulIssue::kObjectAlreadyExists);
    EXPECT_FALSE(result.plan.has_value());
}

TEST(ApplyPlan, NewInsertProjectsCreatesAndPreparedInvariant) {
    ReferenceObjectStore store;
    Applied applied;
    const auto created = shape(20U);

    const auto result = prepareApplyPlan(
        operation(InsertObjectsOp{{created}}, 13U), StatefulValidationContext{store, applied});

    expectPreparedInvariant(result);
    ASSERT_TRUE(result.plan.has_value());
    EXPECT_EQ(result.plan->creates, std::vector<ObjectRecord>({created}));
    EXPECT_TRUE(result.plan->replacements.empty());
    EXPECT_TRUE(result.plan->deletes.empty());
    EXPECT_FALSE(result.plan->delete_closure.has_value());
}

TEST(ApplyPlan, DeleteObjectsProjectsAcceptedB5ClosureExactly) {
    ReferenceObjectStore store;
    insert(store, group(1U));
    insert(store, shape(2U, id(1U)));
    insert(store, connector(10U, id(2U)));
    Applied applied;

    StagedObjectView staged(store);
    DeleteClosure expected;
    const std::vector<ObjectId> requested{id(1U)};
    ASSERT_TRUE(resolveDeleteClosure(staged, std::span<const ObjectId>(requested), &expected).ok());

    const auto result = prepareApplyPlan(
        operation(DeleteObjectsOp{requested}, 14U), StatefulValidationContext{store, applied});

    expectPreparedInvariant(result);
    ASSERT_TRUE(result.plan.has_value());
    ASSERT_TRUE(result.plan->delete_closure.has_value());
    EXPECT_EQ(result.plan->deletes, expected.final_delete_set);
    expectDeleteClosureEqual(*result.plan->delete_closure, expected);
    EXPECT_TRUE(result.plan->creates.empty());
    EXPECT_TRUE(result.plan->replacements.empty());
}

TEST(ApplyPlan, RestoreObjectsProjectsCreates) {
    ReferenceObjectStore store;
    Applied applied;
    const auto restored = shape(30U);

    const auto result = prepareApplyPlan(
        operation(RestoreObjectsOp{{restored}}, 15U), StatefulValidationContext{store, applied});

    expectPreparedInvariant(result);
    ASSERT_TRUE(result.plan.has_value());
    EXPECT_EQ(result.plan->creates, std::vector<ObjectRecord>({restored}));
    EXPECT_TRUE(result.plan->deletes.empty());
    EXPECT_FALSE(result.plan->delete_closure.has_value());
}

TEST(ApplyPlan, SplitStrokesUsesGenericDeletesAndCreatesWithoutDeleteClosure) {
    ReferenceObjectStore store;
    const auto source = vectorStroke(40U);
    const auto replacement_a = vectorStroke(41U);
    const auto replacement_b = vectorStroke(42U);
    insert(store, source);
    Applied applied;

    const auto result = prepareApplyPlan(
        operation(SplitStrokesOp{{{source.id, {replacement_a, replacement_b}}}}, 16U),
        StatefulValidationContext{store, applied});

    expectPreparedInvariant(result);
    ASSERT_TRUE(result.plan.has_value());
    EXPECT_EQ(result.plan->deletes, std::vector<ObjectId>({source.id}));
    EXPECT_EQ(result.plan->creates,
              std::vector<ObjectRecord>({replacement_a, replacement_b}));
    EXPECT_FALSE(result.plan->delete_closure.has_value());
    EXPECT_TRUE(result.plan->replacements.empty());
}

TEST(ApplyPlan, RepresentativeB7CreateProjectionUsesAddStroke) {
    ReferenceObjectStore store;
    Applied applied;
    const auto stroke = vectorStroke(50U);

    const auto result = prepareApplyPlan(
        operation(AddStrokeOp{stroke}, 17U), StatefulValidationContext{store, applied});

    expectPreparedInvariant(result);
    ASSERT_TRUE(result.plan.has_value());
    EXPECT_EQ(result.plan->creates, std::vector<ObjectRecord>({stroke}));
}

TEST(ApplyPlan, RepresentativeB7ReplacementProjectionUsesSetTransforms) {
    ReferenceObjectStore store;
    const auto original = shape(60U);
    insert(store, original);
    Applied applied;
    const Transform2D transform{2.0, 0.0, 0.0, 2.0, 4.0, 5.0};
    auto expected = original;
    expected.transform = transform;

    const auto result = prepareApplyPlan(
        operation(SetTransformsOp{{{original.id, transform}}}, 18U),
        StatefulValidationContext{store, applied});

    expectPreparedInvariant(result);
    ASSERT_TRUE(result.plan.has_value());
    EXPECT_EQ(result.plan->replacements, std::vector<ObjectRecord>({expected}));
    EXPECT_TRUE(result.plan->creates.empty());
    EXPECT_TRUE(result.plan->deletes.empty());
}

TEST(ApplyPlan, AllFifteenOperationKindsDispatchSmoke) {
    ReferenceObjectStore store;
    insert(store, group(1U));
    insert(store, shape(2U, id(1U)));
    insert(store, vectorPath(3U));
    insert(store, image(4U));
    auto stroke = vectorStroke(5U);
    stroke.erase_masks = {mask(50U)};
    insert(store, stroke);
    insert(store, richText(6U));
    insert(store, connector(7U));
    Applied applied;
    const auto before = store.allObjects();

    std::vector<Operation> operations;
    operations.push_back(operation(InsertObjectsOp{{shape(100U)}}, 100U));
    operations.push_back(operation(DeleteObjectsOp{{id(2U)}}, 101U));
    operations.push_back(operation(RestoreObjectsOp{{shape(101U)}}, 102U));
    operations.push_back(operation(
        SetPlacementsOp{{{id(2U), Placement{id(1U), OrderKey({22U})}}}}, 103U));
    operations.push_back(operation(
        SetTransformsOp{{{id(2U), Transform2D{2.0, 0.0, 0.0, 2.0, 4.0, 5.0}}}},
        104U));
    operations.push_back(operation(
        PatchPropertiesOp{{{id(2U), 1U, PropertyPatchAction::kSet, PropertyValue{false}}}},
        105U));
    operations.push_back(operation(SetObjectSizeOp{{{id(2U), 20.0, 30.0}}}, 106U));
    operations.push_back(operation(SetVectorPathGeometryOp{id(3U), path()}, 107U));
    operations.push_back(operation(SetImageContentOp{id(4U), imageContent()}, 108U));
    operations.push_back(operation(AddStrokeOp{vectorStroke(102U)}, 109U));
    operations.push_back(operation(
        SplitStrokesOp{{{id(5U), {vectorStroke(103U)}}}}, 110U));
    operations.push_back(operation(
        AddEraseMasksOp{{{id(5U), {mask(51U)}}}}, 111U));
    operations.push_back(operation(
        RemoveEraseMasksOp{{{id(5U), {id(50U)}}}}, 112U));
    operations.push_back(operation(
        EditRichTextOp{id(6U), RichTextDelta{1U, {InsertTextStep{id(60U), 0U, "X", textStyle()}}}},
        113U));
    operations.push_back(operation(SetConnectorContentOp{id(7U), freeConnectorContent()}, 114U));

    ASSERT_EQ(operations.size(), 15U);
    for (const auto& current : operations) {
        const auto result = prepareApplyPlan(current, StatefulValidationContext{store, applied});
        expectPreparedInvariant(result);
    }
    EXPECT_EQ(store.allObjects(), before);
}

TEST(ApplyPlan, RepeatedPlanningAgainstUnchangedStateIsDeterministic) {
    ReferenceObjectStore store;
    insert(store, group(1U));
    insert(store, shape(2U, id(1U)));
    insert(store, connector(10U, id(2U)));
    Applied applied;
    const auto before = store.allObjects();
    const auto current = operation(DeleteObjectsOp{{id(1U)}}, 200U);

    const auto first = prepareApplyPlan(current, StatefulValidationContext{store, applied});
    const auto second = prepareApplyPlan(current, StatefulValidationContext{store, applied});

    expectPreparedInvariant(first);
    expectPreparedInvariant(second);
    expectEquivalentPlans(first, second);
    EXPECT_EQ(store.allObjects(), before);
}

} // namespace canvas::semantic
