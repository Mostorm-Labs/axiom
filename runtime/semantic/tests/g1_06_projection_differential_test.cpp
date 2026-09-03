#include "g1_06_projection.hpp"

#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <utility>
#include <vector>

namespace canvas::verification::g1_06 {
namespace {

using namespace canvas::semantic;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ProjectionDocumentId documentId() {
    ProjectionDocumentId result;
    result.bytes[15] = 0x7fU;
    return result;
}

ObjectRecord shape(std::uint64_t value) {
    ObjectRecord record;
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{std::nullopt, OrderKey({static_cast<std::uint8_t>(value)})};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, static_cast<double>(value), 0.0};
    record.properties.entries = {{3U, ColorValue{0.1F, 0.2F, 0.3F, 1.0F}}};
    record.content = ShapeContent{1U, 10.0 + value, 20.0};
    record.erase_masks = {EraseMaskRecord{id(100U + value), FilledPathMask{
        VectorPathGeometry{FillRule::kNonZero, {MoveTo{Vec2{1.0, 2.0}}, ClosePath{}}}}}};
    return record;
}

ObjectRecord baseRecord(std::uint64_t value, ObjectKind kind, ObjectContent content) {
    ObjectRecord record = shape(value);
    record.kind = kind;
    record.content = std::move(content);
    return record;
}

template <typename Store>
void insert(Store& store, const ObjectRecord& record) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, record));
}

template <typename Store>
SemanticProjection projectionOf(const ObjectRecord& record) {
    Store store;
    insert(store, record);
    return projectDocument(documentId(), 1U, store);
}

TEST(G106ProjectionDifferential, ProvidersAndPhysicalInsertionOrdersProduceExplicitCanonicalTruth) {
    const ObjectRecord first = shape(1U);
    const ObjectRecord second = shape(2U);
    const ObjectRecord third = shape(3U);
    ReferenceObjectStore reference;
    IndexedObjectStore indexed;
    insert(reference, third);
    insert(reference, first);
    insert(reference, second);
    insert(indexed, second);
    insert(indexed, third);
    insert(indexed, first);

    const auto reference_projection = projectDocument(documentId(), 1U, reference);
    const auto indexed_projection = projectDocument(documentId(), 1U, indexed);
    ASSERT_EQ(reference_projection.objects, std::vector<ObjectRecord>({first, second, third}));
    EXPECT_EQ(indexed_projection.objects, std::vector<ObjectRecord>({first, second, third}));
    EXPECT_EQ(reference_projection, indexed_projection);
    EXPECT_EQ(writeCanonicalProjectionJson(reference_projection), writeCanonicalProjectionJson(indexed_projection));
    EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(indexed));
}

TEST(G106ProjectionDifferential, EveryRequiredSemanticMutationChangesTypedAndJsonProjection) {
    const ObjectRecord base = shape(1U);
    const auto base_projection = projectionOf<ReferenceObjectStore>(base);
    const std::string base_json = writeCanonicalProjectionJson(base_projection);
    const auto expect_changed = [&](const ObjectRecord& changed) {
        const auto changed_projection = projectionOf<ReferenceObjectStore>(changed);
        EXPECT_NE(changed_projection, base_projection);
        EXPECT_NE(writeCanonicalProjectionJson(changed_projection), base_json);
    };

    auto transform = base;
    transform.transform.tx = -0.0;
    expect_changed(transform);

    auto property = base;
    property.properties.entries[0].value = ColorValue{0.9F, 0.2F, 0.3F, 1.0F};
    expect_changed(property);

    auto placement_parent = base;
    placement_parent.placement.parent_id = id(77U);
    expect_changed(placement_parent);

    auto placement_order = base;
    placement_order.placement.order_key = OrderKey({0x7fU});
    expect_changed(placement_order);

    auto content = base;
    std::get<ShapeContent>(content.content).width = 99.0;
    expect_changed(content);

    auto mask = base;
    std::get<FilledPathMask>(mask.erase_masks[0].geometry).path.fill_rule = FillRule::kEvenOdd;
    expect_changed(mask);

    auto rich_text = baseRecord(1U, ObjectKind::kRichText, RichTextContent{RichTextDocument{{
        Paragraph{id(5U), ParagraphStyle{ParagraphAlignment::kLeft, 1.0, 0.0, 0.0},
                  {TextRun{"before", TextStyle{std::nullopt, 12.0, 400U, false, false, ColorValue{}}}}}}}});
    const auto rich_before = projectionOf<ReferenceObjectStore>(rich_text);
    std::get<RichTextContent>(rich_text.content).document.paragraphs[0].runs[0].text = "after";
    const auto rich_after = projectionOf<ReferenceObjectStore>(rich_text);
    EXPECT_NE(rich_before, rich_after);
    EXPECT_NE(writeCanonicalProjectionJson(rich_before), writeCanonicalProjectionJson(rich_after));

    auto rich_style = rich_text;
    std::get<RichTextContent>(rich_style.content).document.paragraphs[0].runs[0].style.underline = true;
    const auto rich_style_projection = projectionOf<ReferenceObjectStore>(rich_style);
    EXPECT_NE(rich_after, rich_style_projection);
    EXPECT_NE(writeCanonicalProjectionJson(rich_after), writeCanonicalProjectionJson(rich_style_projection));

    auto connector = baseRecord(1U, ObjectKind::kConnector, ConnectorContent{
        ConnectorEndpoint{FreePointEndpoint{Vec2{1.0, 2.0}}},
        ConnectorEndpoint{AttachedEndpoint{id(2U), StablePortAnchor{3U}}},
        ConnectorRouting::kStraight});
    const auto connector_before = projectionOf<ReferenceObjectStore>(connector);
    std::get<AttachedEndpoint>(std::get<ConnectorContent>(connector.content).end.value).target_object_id = id(4U);
    const auto connector_after = projectionOf<ReferenceObjectStore>(connector);
    EXPECT_NE(connector_before, connector_after);
    EXPECT_NE(writeCanonicalProjectionJson(connector_before), writeCanonicalProjectionJson(connector_after));
}

} // namespace
} // namespace canvas::verification::g1_06
