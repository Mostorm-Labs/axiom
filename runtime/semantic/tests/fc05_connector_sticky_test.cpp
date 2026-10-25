#include "canvas/semantic/connector_validation.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>

namespace {
using namespace canvas::semantic;
using canvas::foundation::ObjectId;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ObjectRecord shape(std::uint64_t value, std::uint32_t shapeKind,
                   std::uint32_t kindVersion) {
    ObjectRecord result{};
    result.id = id(value);
    result.kind = ObjectKind::kShape;
    result.kind_version = kindVersion;
    result.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    result.content = ShapeContent{shapeKind, 100.0, 80.0};
    return result;
}

ConnectorContent attached(std::uint64_t target) {
    ConnectorContent result{};
    result.start.value = AttachedEndpoint{id(target), AutoPerimeterAnchor{}};
    result.end.value = FreePointEndpoint{{200.0, 100.0}};
    return result;
}
}  // namespace

TEST(FC05Connector, TriangleShapeV2IsConnectable) {
    ReferenceObjectStore store;
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, shape(1, 3, 2)));
    StagedObjectView staged(store);
    EXPECT_EQ(validateConnectorReferences(staged, attached(1)).issue,
              StatefulIssue::kNone);
}

TEST(FC05Connector, RichShapeV3IsConnectable) {
    ReferenceObjectStore store;
    auto target = shape(1, 3, 3);
    RichTextContent label{};
    target.content = ShapeContent{3, 100.0, 80.0, label};
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, target));
    StagedObjectView staged(store);
    EXPECT_EQ(validateConnectorReferences(staged, attached(1)).issue,
              StatefulIssue::kNone);
}

TEST(FC05Connector, ShapeV1TriangleAndShapeV3WithoutLabelAreRejected) {
    for (auto target : {shape(1, 3, 1), shape(1, 1, 3)}) {
        ReferenceObjectStore store;
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, target));
        StagedObjectView staged(store);
        EXPECT_EQ(validateConnectorReferences(staged, attached(1)).issue,
                  StatefulIssue::kInvalidKindVersion);
    }
}

TEST(FC05Connector, StablePortOutsideReleasedNamespaceIsRejected) {
    ReferenceObjectStore store;
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, shape(1, 3, 2)));
    StagedObjectView staged(store);
    auto content = attached(1);
    content.start.value = AttachedEndpoint{id(1), StablePortAnchor{5U}};
    EXPECT_EQ(validateConnectorReferences(staged, content).issue,
              StatefulIssue::kConnectorInvalid);
}
