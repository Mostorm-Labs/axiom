#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::semantic {

namespace {

ObjectContent makeContent(ObjectKind kind, std::uint8_t content_tag) {
    const double value = static_cast<double>(content_tag);
    switch (kind) {
        case ObjectKind::kShape:
            return ShapeContent{content_tag, value, value + 1.0};
        case ObjectKind::kImage:
            return ImageContent{
                ResourceId{ObjectId::fromUint64(content_tag)}, value + 100.0, value + 50.0,
                NormalizedRect{0.0, 0.0, 1.0, 1.0}, ImageContentMode::kFit, value, value + 1.0};
        case ObjectKind::kVectorPath:
            return VectorPathContent{
                VectorPathGeometry{FillRule::kNonZero, {MoveTo{Vec2{value, value + 1.0}}}}};
        case ObjectKind::kRichText:
            return RichTextContent{RichTextDocument{{
                Paragraph{ObjectId::fromUint64(content_tag), ParagraphStyle{ParagraphAlignment::kLeft, 1.0, 0.0, 0.0},
                          {TextRun{"typed", TextStyle{}}}}}}};
        case ObjectKind::kVectorStroke:
            return VectorStrokeContent{
                StrokeRecord{BrushDescriptor{}, content_tag,
                             VectorStrokeData{{StrokeSample{Vec2{value, value}, 1.0F,
                                                            Vec2{0.0, 0.0}}}}}};
        case ObjectKind::kDabStroke:
            return DabStrokeContent{
                StrokeRecord{BrushDescriptor{}, content_tag,
                             DabStrokeData{{DabInstance{Vec2{value, value}, value + 1.0, 0.0F, 1.0F}}}}};
        case ObjectKind::kConnector:
            return ConnectorContent{
                ConnectorEndpoint{FreePointEndpoint{Vec2{value, 0.0}}},
                ConnectorEndpoint{FreePointEndpoint{Vec2{value + 1.0, 1.0}}},
                ConnectorRouting::kStraight};
        case ObjectKind::kSticky:
            return StickyContent{value + 10.0, value + 20.0};
        case ObjectKind::kGroup:
            return GroupContent{};
    }
    return ShapeContent{};
}

EraseMaskRecord makeEraseMask(std::uint64_t id, std::uint8_t content_tag) {
    const double value = static_cast<double>(content_tag);
    return EraseMaskRecord{
        ObjectId::fromUint64(id + 1000U),
        SweptCircleMask{{EraseCubicSegment{
            EraseKnot{Vec2{value, value}, value + 1.0},
            EraseKnot{Vec2{value + 2.0, value + 2.0}, value + 1.0},
            Vec2{value + 0.5, value},
            Vec2{value + 1.5, value + 2.0},
        }}}};
}

ObjectRecord makeRecord(
    std::uint64_t id,
    ObjectKind kind,
    std::optional<ObjectId> parent,
    std::vector<std::uint8_t> order_key,
    std::uint8_t content_tag) {
    ObjectRecord record{};
    record.id = ObjectId::fromUint64(id);
    record.kind = kind;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey(std::move(order_key))};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, static_cast<double>(id), -2.0};
    record.properties.entries = {
        PropertyEntry{17U, ColorValue{0.1F, static_cast<float>(content_tag), 0.3F, 1.0F}}};
    record.content = makeContent(kind, content_tag);
    record.erase_masks = {makeEraseMask(id, content_tag)};
    return record;
}

void expectEquivalent(
    const ReferenceObjectStore& reference,
    const IndexedObjectStore& indexed,
    const std::vector<std::optional<ObjectId>>& parent_scopes) {
    ASSERT_EQ(reference.size(), indexed.size());
    EXPECT_EQ(reference.allObjects(), indexed.allObjects());

    for (const ObjectRecord& record : reference.allObjects()) {
        EXPECT_TRUE(indexed.contains(record.id));
        ASSERT_NE(indexed.find(record.id), nullptr);
        EXPECT_EQ(*indexed.find(record.id), record);
    }
    EXPECT_EQ(indexed.find(ObjectId::fromUint64(999U)), nullptr);

    for (const auto& parent_id : parent_scopes) {
        EXPECT_EQ(reference.children(parent_id), indexed.children(parent_id));
    }
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

template <typename Store>
void insertFresh(Store& store, const ObjectRecord& record) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
}

template <typename Store>
void replaceExisting(Store& store, const ObjectRecord& record) {
    ASSERT_TRUE(internal::ObjectStoreMutator::replaceExisting(store, record));
}

template <typename Store>
void eraseExisting(Store& store, const ObjectId& id) {
    ASSERT_TRUE(internal::ObjectStoreMutator::eraseExisting(store, id));
}

} // namespace

TEST(ObjectStoreDifferential, MatchesReferenceAfterPreparedValidMutations) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;

    const ObjectRecord root_later = makeRecord(20U, ObjectKind::kSticky, std::nullopt, {0x20U}, 2U);
    const ObjectRecord root_earlier = makeRecord(10U, ObjectKind::kGroup, std::nullopt, {0x10U}, 1U);
    const ObjectRecord child_later = makeRecord(
        30U, ObjectKind::kShape, root_earlier.id, {0x30U}, 3U);
    const ObjectRecord child_earlier = makeRecord(
        40U, ObjectKind::kVectorStroke, root_earlier.id, {0x10U}, 4U);
    const ObjectRecord nested_child = makeRecord(
        50U, ObjectKind::kConnector, child_earlier.id, {0x10U}, 5U);
    const std::vector<std::optional<ObjectId>> scopes = {
        std::nullopt, root_earlier.id, root_later.id, child_earlier.id};

    expectEquivalent(reference, indexed, scopes);

    insertFresh(reference, root_later);
    insertFresh(indexed, root_later);
    insertFresh(reference, root_earlier);
    insertFresh(indexed, root_earlier);
    expectEquivalent(reference, indexed, scopes);
    ASSERT_EQ(indexed.children(std::nullopt).size(), 2U);
    EXPECT_EQ(indexed.children(std::nullopt)[0].id, root_earlier.id);
    EXPECT_EQ(indexed.children(std::nullopt)[1].id, root_later.id);

    insertFresh(reference, child_later);
    insertFresh(indexed, child_later);
    insertFresh(reference, child_earlier);
    insertFresh(indexed, child_earlier);
    insertFresh(reference, nested_child);
    insertFresh(indexed, nested_child);
    expectEquivalent(reference, indexed, scopes);
    ASSERT_EQ(indexed.children(root_earlier.id).size(), 2U);
    EXPECT_EQ(indexed.children(root_earlier.id)[0].id, child_earlier.id);
    EXPECT_EQ(indexed.children(root_earlier.id)[1].id, child_later.id);

    ObjectRecord reordered = child_later;
    reordered.placement.order_key = OrderKey({0x05U});
    replaceExisting(reference, reordered);
    replaceExisting(indexed, reordered);
    expectEquivalent(reference, indexed, scopes);
    ASSERT_EQ(indexed.children(root_earlier.id).size(), 2U);
    EXPECT_EQ(indexed.children(root_earlier.id)[0].id, reordered.id);

    ObjectRecord reparented = child_earlier;
    reparented.placement = Placement{root_later.id, OrderKey({0x08U})};
    replaceExisting(reference, reparented);
    replaceExisting(indexed, reparented);
    expectEquivalent(reference, indexed, scopes);
    EXPECT_TRUE(indexed.children(root_earlier.id).size() == 1U);
    ASSERT_EQ(indexed.children(root_later.id).size(), 1U);
    EXPECT_EQ(indexed.children(root_later.id)[0].id, reparented.id);

    ObjectRecord content_only = reordered;
    content_only.transform.tx = 123.0;
    content_only.properties.entries[0].value = ColorValue{0.9F, 0.8F, 0.7F, 1.0F};
    std::get<ShapeContent>(content_only.content).width = 250.0;
    std::get<SweptCircleMask>(content_only.erase_masks[0].geometry)
        .segments[0]
        .p0.radius = 42.0;
    replaceExisting(reference, content_only);
    replaceExisting(indexed, content_only);
    expectEquivalent(reference, indexed, scopes);

    eraseExisting(reference, nested_child.id);
    eraseExisting(indexed, nested_child.id);
    expectEquivalent(reference, indexed, scopes);
    EXPECT_TRUE(indexed.children(child_earlier.id).empty());

    eraseExisting(reference, reparented.id);
    eraseExisting(indexed, reparented.id);
    expectEquivalent(reference, indexed, scopes);
    EXPECT_TRUE(indexed.children(root_later.id).empty());
}

TEST(ObjectStoreDifferential, UsesObjectIdOnlyAsImplementationTieBreakForEqualOrderKeys) {
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    const ObjectRecord parent = makeRecord(100U, ObjectKind::kGroup, std::nullopt, {0x10U}, 1U);
    const ObjectRecord later_id = makeRecord(102U, ObjectKind::kShape, parent.id, {0x20U}, 2U);
    const ObjectRecord earlier_id = makeRecord(101U, ObjectKind::kShape, parent.id, {0x20U}, 3U);

    insertFresh(reference, parent);
    insertFresh(indexed, parent);
    insertFresh(reference, later_id);
    insertFresh(indexed, later_id);
    insertFresh(reference, earlier_id);
    insertFresh(indexed, earlier_id);

    expectEquivalent(reference, indexed, {std::nullopt, parent.id});
    const auto children = indexed.children(parent.id);
    ASSERT_EQ(children.size(), 2U);
    EXPECT_EQ(children[0].id, earlier_id.id);
    EXPECT_EQ(children[1].id, later_id.id);
}

} // namespace canvas::semantic
