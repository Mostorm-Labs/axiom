#include "canvas/semantic/operation_state_validator.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/staged_object_view.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::semantic {
namespace {

struct CaseRecord final {
    ObjectKind kind;
    std::uint32_t kind_version;
    std::optional<std::uint32_t> field_id;
    bool clear;
    bool expected_ok;
    StatefulIssue expected_issue;
};

ObjectRecord makeRecord(std::uint64_t id, ObjectKind kind, std::uint32_t kind_version = 1U) {
    ObjectRecord record{};
    record.id = ObjectId::fromUint64(id);
    record.kind = kind;
    record.kind_version = kind_version;
    record.placement = Placement{std::nullopt, OrderKey({0x10U})};
    switch (kind) {
        case ObjectKind::kShape:
            record.content = ShapeContent{7U, 1.0, 2.0};
            break;
        case ObjectKind::kImage:
            record.content = ImageContent{};
            break;
        case ObjectKind::kVectorPath:
            record.content = VectorPathContent{};
            break;
        case ObjectKind::kRichText:
            record.content = RichTextContent{};
            break;
        case ObjectKind::kVectorStroke:
            record.content = VectorStrokeContent{};
            break;
        case ObjectKind::kDabStroke:
            record.content = DabStrokeContent{};
            break;
        case ObjectKind::kConnector:
            record.content = ConnectorContent{};
            break;
        case ObjectKind::kSticky:
            record.content = StickyContent{10.0, 20.0};
            break;
        case ObjectKind::kGroup:
            record.content = GroupContent{};
            break;
    }
    return record;
}

class CountingObjectStore final : public ObjectStore {
  public:
    explicit CountingObjectStore(const ObjectStore& delegate) : delegate_(delegate) {}

    [[nodiscard]] std::size_t size() const noexcept override { return delegate_.size(); }
    [[nodiscard]] bool contains(const ObjectId& id) const noexcept override {
        ++contains_calls;
        return delegate_.contains(id);
    }
    [[nodiscard]] const ObjectRecord* find(const ObjectId& id) const noexcept override {
        ++find_calls;
        return delegate_.find(id);
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

    void reset() const noexcept {
        contains_calls = 0U;
        find_calls = 0U;
        all_objects_calls = 0U;
        children_calls = 0U;
    }

    mutable std::size_t contains_calls = 0U;
    mutable std::size_t find_calls = 0U;
    mutable std::size_t all_objects_calls = 0U;
    mutable std::size_t children_calls = 0U;

  private:
    const ObjectStore& delegate_;
};

template <typename Store>
void insertFresh(Store& store, const ObjectRecord& record) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
}

ObjectRecord shapeRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kShape, kind_version);
}

ObjectRecord imageRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kImage, kind_version);
}

ObjectRecord vectorPathRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kVectorPath, kind_version);
}

ObjectRecord richTextRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kRichText, kind_version);
}

ObjectRecord vectorStrokeRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kVectorStroke, kind_version);
}

ObjectRecord dabStrokeRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kDabStroke, kind_version);
}

ObjectRecord connectorRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kConnector, kind_version);
}

ObjectRecord stickyRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kSticky, kind_version);
}

ObjectRecord groupRecord(std::uint64_t id, std::uint32_t kind_version = 1U) {
    return makeRecord(id, ObjectKind::kGroup, kind_version);
}

const std::array<ObjectKind, 9> kKinds = {
    ObjectKind::kShape, ObjectKind::kImage, ObjectKind::kVectorPath, ObjectKind::kRichText,
    ObjectKind::kVectorStroke, ObjectKind::kDabStroke, ObjectKind::kConnector,
    ObjectKind::kSticky, ObjectKind::kGroup};

const std::array<std::uint32_t, 8> kFieldIds = {
    0x00000001U, 0x00000002U, 0x00000003U, 0x00000004U,
    0x00000100U, 0x00000101U, 0x00000200U, 0x00000201U};

bool applies(std::uint32_t field_id, ObjectKind kind) {
    switch (field_id) {
        case 0x00000001U:
        case 0x00000002U:
            return true;
        case 0x00000003U:
        case 0x00000004U:
            return kind == ObjectKind::kShape || kind == ObjectKind::kImage ||
                   kind == ObjectKind::kVectorPath || kind == ObjectKind::kRichText ||
                   kind == ObjectKind::kConnector || kind == ObjectKind::kSticky;
        case 0x00000100U:
            return kind == ObjectKind::kShape || kind == ObjectKind::kVectorPath ||
                   kind == ObjectKind::kSticky;
        case 0x00000101U:
            return kind == ObjectKind::kShape || kind == ObjectKind::kVectorPath ||
                   kind == ObjectKind::kConnector || kind == ObjectKind::kSticky;
        case 0x00000200U:
        case 0x00000201U:
            return kind == ObjectKind::kConnector;
        default:
            return false;
    }
}

StatefulIssue expectedIssueForKindVersion(ObjectKind kind, std::uint32_t kind_version) {
    if (kind_version != 1U) return StatefulIssue::kInvalidKindVersion;
    switch (kind) {
        case ObjectKind::kShape:
        case ObjectKind::kImage:
        case ObjectKind::kVectorPath:
        case ObjectKind::kRichText:
        case ObjectKind::kVectorStroke:
        case ObjectKind::kDabStroke:
        case ObjectKind::kConnector:
        case ObjectKind::kSticky:
        case ObjectKind::kGroup:
            return StatefulIssue::kNone;
    }
    return StatefulIssue::kInvalidKindVersion;
}

StatefulIssue expectedIssueForStateRule(StateRule rule, ObjectKind kind) {
    switch (rule) {
        case StateRule::kCreateAbsent:
        case StateRule::kEditExisting:
        case StateRule::kPlacementTarget:
        case StateRule::kTransformTarget:
            return StatefulIssue::kNone;
        case StateRule::kSizeTarget:
            return (kind == ObjectKind::kShape || kind == ObjectKind::kImage ||
                    kind == ObjectKind::kSticky)
                       ? StatefulIssue::kNone
                       : StatefulIssue::kInvalidApplicability;
        case StateRule::kVectorPathTarget:
            return kind == ObjectKind::kVectorPath ? StatefulIssue::kNone : StatefulIssue::kInvalidApplicability;
        case StateRule::kImageTarget:
            return kind == ObjectKind::kImage ? StatefulIssue::kNone : StatefulIssue::kInvalidApplicability;
        case StateRule::kStrokeTarget:
            return (kind == ObjectKind::kVectorStroke || kind == ObjectKind::kDabStroke)
                       ? StatefulIssue::kNone
                       : StatefulIssue::kInvalidApplicability;
        case StateRule::kRichTextTarget:
            return kind == ObjectKind::kRichText ? StatefulIssue::kNone : StatefulIssue::kInvalidApplicability;
        case StateRule::kConnectorTarget:
            return kind == ObjectKind::kConnector ? StatefulIssue::kNone : StatefulIssue::kInvalidApplicability;
    }
    return StatefulIssue::kInvalidApplicability;
}

} // namespace

TEST(StatefulKindApplicability, MissingAndExistingMapToStateIssues) {
    ReferenceObjectStore base;
    const ObjectRecord existing = shapeRecord(1U);
    insertFresh(base, existing);
    StagedObjectView staged(base);

    EXPECT_EQ(requireExisting(staged, existing.id).issue, StatefulIssue::kNone);
    EXPECT_EQ(requireExisting(staged, ObjectId::fromUint64(99U)).issue, StatefulIssue::kObjectMissing);
    EXPECT_EQ(requireAbsent(staged, existing.id).issue, StatefulIssue::kObjectAlreadyExists);
    EXPECT_EQ(requireAbsent(staged, ObjectId::fromUint64(99U)).issue, StatefulIssue::kNone);
}

TEST(StatefulKindApplicability, StageCreateDeleteAndReplaceControlVisibility) {
    ReferenceObjectStore base;
    const ObjectRecord existing = shapeRecord(1U);
    insertFresh(base, existing);
    StagedObjectView staged(base);

    ObjectRecord created = imageRecord(2U);
    ASSERT_TRUE(staged.stageCreate(created));
    ObjectRecord replacement = shapeRecord(1U);
    replacement.transform.tx = 7.0;
    ASSERT_TRUE(staged.stageReplace(replacement));
    ASSERT_TRUE(staged.stageDelete(existing.id));
    ASSERT_FALSE(staged.stageReplace(shapeRecord(3U)));

    EXPECT_EQ(requireExisting(staged, created.id).issue, StatefulIssue::kNone);
    EXPECT_EQ(requireExisting(staged, existing.id).issue, StatefulIssue::kObjectMissing);
    EXPECT_EQ(requireAbsent(staged, existing.id).issue, StatefulIssue::kNone);
}

TEST(StatefulKindApplicability, KindVersionFailsClosedForUnknownAndNonOneVersions) {
    const std::array<ObjectRecord, 9> records = {
        shapeRecord(1U, 0U), imageRecord(2U, 2U), vectorPathRecord(3U, 3U), richTextRecord(4U, 4U),
        vectorStrokeRecord(5U, 5U), dabStrokeRecord(6U, 6U), connectorRecord(7U, 7U),
        stickyRecord(8U, 8U), groupRecord(9U, 9U)};

    for (const auto& record : records) {
        EXPECT_EQ(requireKindVersion(record, record.kind, record.kind_version).issue,
                  expectedIssueForKindVersion(record.kind, record.kind_version));
    }

    const ObjectRecord released = shapeRecord(10U);
    EXPECT_EQ(requireKindVersion(released, static_cast<ObjectKind>(0U), 1U).issue,
              StatefulIssue::kInvalidKindVersion);
}

TEST(StatefulKindApplicability, StateRuleMatrixMatchesOracleForAllKinds) {
    const std::array<StateRule, 10> rules = {
        StateRule::kCreateAbsent, StateRule::kEditExisting, StateRule::kPlacementTarget,
        StateRule::kTransformTarget, StateRule::kSizeTarget, StateRule::kVectorPathTarget,
        StateRule::kImageTarget, StateRule::kStrokeTarget, StateRule::kRichTextTarget,
        StateRule::kConnectorTarget};

    for (const auto kind : kKinds) {
        const ObjectRecord record = makeRecord(42U, kind);
        for (const auto rule : rules) {
            const StatefulResult result = validateRecordStateForOperation(record, rule);
            const StatefulIssue expected = expectedIssueForStateRule(rule, kind);
            EXPECT_EQ(result.issue, expected) << static_cast<int>(record.kind) << " " << static_cast<int>(rule);
        }
    }
}

TEST(StatefulKindApplicability, FieldApplicabilityMatrixMatchesOracleForAllKinds) {
    for (const auto kind : kKinds) {
        const ObjectRecord record = makeRecord(10U, kind);
        for (const auto field_id : kFieldIds) {
            const bool expected = applies(field_id, kind);
            PropertyValue value = true;
            switch (field_id) {
                case 0x00000003U:
                    value = 1.0F;
                    break;
                case 0x00000004U:
                    value = BlendModeValue::kNormal;
                    break;
                case 0x00000100U:
                    value = FillStyleValue{NoFill{}};
                    break;
                case 0x00000101U:
                    value = StrokeStyleValue{NoStroke{}};
                    break;
                case 0x00000200U:
                case 0x00000201U:
                    value = ConnectorDecorationValue::kArrow;
                    break;
                default:
                    break;
            }
            EXPECT_EQ(requirePropertyApplicability(record, field_id, &value).issue,
                      expected ? StatefulIssue::kNone : StatefulIssue::kInvalidApplicability);
            EXPECT_EQ(requirePropertyApplicability(record, field_id, nullptr).issue,
                      expected ? StatefulIssue::kNone : StatefulIssue::kInvalidApplicability);
        }
    }
}

TEST(StatefulKindApplicability, ApplicableFieldRejectsContradictoryTaggedValueType) {
    const ObjectRecord shape = shapeRecord(11U);
    const PropertyValue wrong_type = ConnectorDecorationValue::kArrow;
    EXPECT_EQ(requirePropertyApplicability(shape, 0x00000001U, &wrong_type).issue,
              StatefulIssue::kInvalidApplicability);
}

TEST(StatefulKindApplicability, UnknownFieldIdFailsClosed) {
    const ObjectRecord shape = shapeRecord(12U);
    const PropertyValue value = true;
    EXPECT_EQ(requirePropertyApplicability(shape, 0xDEADBEEFU, &value).issue,
              StatefulIssue::kInvalidApplicability);
    EXPECT_EQ(requirePropertyApplicability(shape, 0xDEADBEEFU, nullptr).issue,
              StatefulIssue::kInvalidApplicability);
}

TEST(StatefulKindApplicability, ClearPathChecksApplicabilityWithoutMaterializingDefaults) {
    ReferenceObjectStore base;
    const ObjectRecord connector = connectorRecord(1U);
    insertFresh(base, connector);
    StagedObjectView staged(base);
    const ObjectRecord before = *staged.find(connector.id);

    EXPECT_EQ(requirePropertyApplicability(connector, 0x00000200U, nullptr).issue, StatefulIssue::kNone);
    EXPECT_EQ(staged.find(connector.id)->properties, before.properties);
}

TEST(StatefulKindApplicability, ReferenceAndIndexedStoresProduceMatchingValidationResults) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    const ObjectRecord existing = stickyRecord(1U);
    insertFresh(reference, existing);
    insertFresh(indexed, existing);

    StagedObjectView reference_view(reference);
    StagedObjectView indexed_view(indexed);

    ASSERT_NE(reference_view.find(existing.id), nullptr);
    ASSERT_NE(indexed_view.find(existing.id), nullptr);
    EXPECT_EQ(*reference_view.find(existing.id), *indexed_view.find(existing.id));
    EXPECT_EQ(reference_view.allObjects(), indexed_view.allObjects());
    EXPECT_EQ(requireExisting(reference_view, existing.id).issue, requireExisting(indexed_view, existing.id).issue);
    EXPECT_EQ(validateRecordStateForOperation(*reference_view.find(existing.id), StateRule::kSizeTarget).issue,
              validateRecordStateForOperation(*indexed_view.find(existing.id), StateRule::kSizeTarget).issue);
}

TEST(StatefulKindApplicability, SingleTargetLookupsDoNotEnumerateAllObjects) {
    ReferenceObjectStore base;
    const ObjectRecord existing = connectorRecord(1U);
    insertFresh(base, existing);
    CountingObjectStore counting(base);
    StagedObjectView staged(counting);

    counting.reset();
    (void)requireExisting(staged, existing.id);
    EXPECT_EQ(counting.all_objects_calls, 0U);
    counting.reset();
    (void)requireAbsent(staged, ObjectId::fromUint64(99U));
    EXPECT_EQ(counting.all_objects_calls, 0U);
}

TEST(StatefulKindApplicability, BeforeAndAfterProjectionsStayUnchanged) {
    ReferenceObjectStore base;
    const ObjectRecord existing = groupRecord(1U);
    insertFresh(base, existing);
    const auto before = base.allObjects();
    StagedObjectView staged(base);
    ASSERT_TRUE(staged.stageCreate(stickyRecord(2U)));
    ASSERT_TRUE(staged.stageDelete(existing.id));
    const auto after = base.allObjects();

    EXPECT_EQ(before, after);
    EXPECT_EQ(before, base.allObjects());
}

TEST(StatefulKindApplicability, IndexedStoreParityAndRebuildStayStable) {
    IndexedObjectStore indexed;
    const ObjectRecord existing = vectorPathRecord(1U);
    insertFresh(indexed, existing);
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));

    const auto before = indexed.allObjects();
    StagedObjectView staged(indexed);
    ASSERT_TRUE(staged.stageCreate(stickyRecord(2U)));
    ASSERT_TRUE(staged.stageDelete(existing.id));

    EXPECT_EQ(before, indexed.allObjects());
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

} // namespace canvas::semantic
