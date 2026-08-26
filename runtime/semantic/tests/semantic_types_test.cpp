#include "canvas/semantic/canonical_numeric.hpp"
#include "canvas/semantic/canonical_commit_stamp.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

namespace canvas::semantic {

namespace {

struct ServerRevisionBoundaryType final {
    std::uint64_t value = 0;
};

struct ServerCursorBoundaryType final {
    std::uint64_t value = 0;
};

} // namespace

TEST(SemanticTypes, ObjectIdIsOpaqueSixteenBytesAndZeroIsInvalid) {
    static_assert(sizeof(ObjectId) == 16);
    ObjectId id = ObjectId::fromUint64(42);
    EXPECT_FALSE(id.isZero());
    EXPECT_TRUE(ObjectId{}.isZero());
}

TEST(SemanticTypes, OrderKeyAcceptsOnlyAuthorityBounds) {
    EXPECT_FALSE(OrderKey(std::vector<std::uint8_t>{}).isValid());
    EXPECT_TRUE(OrderKey(std::vector<std::uint8_t>(1, 0)).isValid());
    EXPECT_TRUE(OrderKey(std::vector<std::uint8_t>(32, 0)).isValid());
    EXPECT_FALSE(OrderKey(std::vector<std::uint8_t>(33, 0)).isValid());
}

TEST(SemanticTypes, OrderKeyUsesUnsignedLexicographicBytes) {
    const OrderKey low({0x01});
    const OrderKey high({0x80});
    EXPECT_LT(low, high);
}

TEST(SemanticTypes, NumericNormalizationRejectsNonFiniteAndCanonicalizesNegativeZero) {
    double output = -1.0;
    EXPECT_TRUE(normalizeFinite(-0.0, output));
    EXPECT_EQ(output, 0.0);
    EXPECT_FALSE(std::signbit(output));
    EXPECT_FALSE(normalizeFinite(std::numeric_limits<double>::quiet_NaN(), output));
    EXPECT_FALSE(normalizeFinite(std::numeric_limits<double>::infinity(), output));
}

TEST(SemanticTypes, RegistryIsExplicitAndClosed) {
    for (std::uint8_t value = 1; value <= 9; ++value) {
        EXPECT_TRUE(isKnownObjectKind(static_cast<ObjectKind>(value)));
    }
    EXPECT_FALSE(isKnownObjectKind(static_cast<ObjectKind>(0)));
    EXPECT_FALSE(isKnownObjectKind(static_cast<ObjectKind>(10)));

    for (std::uint8_t value = 1; value <= 15; ++value) {
        EXPECT_TRUE(isKnownOperationKind(static_cast<OperationKind>(value)));
    }
    EXPECT_FALSE(isKnownOperationKind(static_cast<OperationKind>(0)));
    EXPECT_FALSE(isKnownOperationKind(static_cast<OperationKind>(16)));
}

TEST(SemanticTypes, SemanticGenerationIsASeparateStrongRuntimeLocalToken) {
    static_assert(!std::is_convertible_v<OperationId, ObjectId>);
    static_assert(!std::is_convertible_v<ObjectId, OperationId>);
    static_assert(!std::is_convertible_v<OperationId, SemanticGeneration>);
    static_assert(!std::is_convertible_v<SemanticGeneration, OperationId>);
    static_assert(!std::is_convertible_v<SemanticGeneration, std::uint64_t>);
    static_assert(!std::is_convertible_v<SemanticGeneration, CanonicalCommitStamp>);
    static_assert(!std::is_convertible_v<SemanticGeneration, ServerRevisionBoundaryType>);
    static_assert(!std::is_convertible_v<SemanticGeneration, ServerCursorBoundaryType>);

    const SemanticGeneration baseline(0U);
    const SemanticGeneration successor(1U);
    EXPECT_LT(baseline, successor);
    EXPECT_EQ(successor.value(), 1U);
}

TEST(SemanticTypes, ChangeSetMergesObjectChangesInDeterministicOrder) {
    const ObjectId first = ObjectId::fromUint64(1U);
    const ObjectId second = ObjectId::fromUint64(2U);
    const ChangeSet changes = ChangeSet::fromChanges(
        SemanticGeneration(7U), SemanticGeneration(8U),
        {
            {second, SemanticChangeFlags::kTransform, {30U, 10U, 10U}},
            {first, SemanticChangeFlags::kProperties, {9U, 3U, 9U}},
            {second, SemanticChangeFlags::kProperties, {10U, 5U}},
        });

    EXPECT_EQ(changes.beforeGeneration(), SemanticGeneration(7U));
    EXPECT_EQ(changes.afterGeneration(), SemanticGeneration(8U));
    ASSERT_EQ(changes.objects().size(), 2U);
    EXPECT_EQ(changes.objects()[0].object_id, first);
    EXPECT_EQ(changes.objects()[1].object_id, second);
    EXPECT_EQ(changes.objects()[1].flags,
              SemanticChangeFlags::kTransform | SemanticChangeFlags::kProperties);
    EXPECT_EQ(changes.objects()[1].changed_fields,
              (std::vector<FieldId>{5U, 10U, 30U}));
}

TEST(SemanticTypes, ChangeSetExpressesCreatedAndDeletedObjects) {
    const ObjectId created = ObjectId::fromUint64(1U);
    const ObjectId deleted = ObjectId::fromUint64(2U);
    const ChangeSet changes = ChangeSet::fromChanges(
        SemanticGeneration(4U), SemanticGeneration(5U),
        {
            {deleted, SemanticChangeFlags::kDeleted, {}},
            {created, SemanticChangeFlags::kCreated, {}},
        });

    ASSERT_EQ(changes.objects().size(), 2U);
    EXPECT_EQ(changes.objects()[0].flags, SemanticChangeFlags::kCreated);
    EXPECT_EQ(changes.objects()[1].flags, SemanticChangeFlags::kDeleted);
}

TEST(SemanticTypes, PublicTypesDoNotRequireSceneOrRenderer) {
    static_assert(std::is_trivially_copyable_v<Operation>);
    ObjectRecord record{ObjectId::fromUint64(1), ObjectKind::kShape, OrderKey({1})};
    const ChangeSet changes = ChangeSet::fromChanges(
        SemanticGeneration(0U), SemanticGeneration(1U),
        {{record.id, SemanticChangeFlags::kCreated, {}}});
    EXPECT_EQ(changes.objects().size(), 1U);
}

} // namespace canvas::semantic
