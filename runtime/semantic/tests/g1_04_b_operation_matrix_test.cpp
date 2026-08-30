#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace canvas::semantic {
namespace {

struct MatrixRow final {
    std::string_view operation_name;
    std::string_view polarity;
};

constexpr std::array<MatrixRow, 1> kRedMatrix{{
    {"InsertObjects", "positive"},
}};

} // namespace

TEST(G104B10OperationMatrix, FifteenFamiliesRequirePositiveAndNegativeRows) {
    // RED: B10 is incomplete until all 15 operation families have both a
    // released-authority positive Prepared row and a state-rejected row.
    EXPECT_EQ(kRedMatrix.size(), 30U);
}

} // namespace canvas::semantic
