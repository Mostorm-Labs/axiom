#include "canvas/semantic/canonical_numeric.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/operation.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

namespace canvas::semantic {

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

TEST(SemanticTypes, PublicTypesDoNotRequireSceneOrRenderer) {
    static_assert(std::is_trivially_copyable_v<Operation>);
    ObjectRecord record{ObjectId::fromUint64(1), ObjectKind::kShape, OrderKey({1})};
    SemanticChangeSet changes;
    changes.added.push_back(record.id);
    EXPECT_EQ(changes.added.size(), 1U);
}

} // namespace canvas::semantic
