// Verification-only seam: compile the exact production validator translation
// unit here so its anonymous checked-arithmetic helpers execute directly.
#include "../src/validator.cpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>

namespace canvas::semantic {

TEST(G1CGeometryOverflowSource, CheckedAdditionRejectsUnsignedOverflow) {
    const GeometryCount total{std::numeric_limits<std::size_t>::max(), ValidationIssue::kNone};
    const GeometryCount next{1U, ValidationIssue::kNone};
    EXPECT_EQ(addGeometryUnits(total, next).issue, ValidationIssue::kIntegerOverflow);
}

TEST(G1CGeometryOverflowSource, CheckedMultiplicationRejectsDabOverflow) {
    const auto count = std::numeric_limits<std::size_t>::max() / geometry_accounting_v1::kDabInstance + 1U;
    EXPECT_EQ(multiplyGeometryUnits(count, geometry_accounting_v1::kDabInstance).issue,
              ValidationIssue::kIntegerOverflow);
}

TEST(G1CGeometryOverflowSource, CheckedMultiplicationRejectsEraseOverflow) {
    const auto count = std::numeric_limits<std::size_t>::max() / geometry_accounting_v1::kEraseCubicSegment + 1U;
    EXPECT_EQ(multiplyGeometryUnits(count, geometry_accounting_v1::kEraseCubicSegment).issue,
              ValidationIssue::kIntegerOverflow);
}

} // namespace canvas::semantic
