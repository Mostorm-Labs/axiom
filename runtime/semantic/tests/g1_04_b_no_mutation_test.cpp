#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace canvas::semantic {
namespace {

struct NoMutationObservation final {
    std::string_view store;
    std::string_view disposition;
};

constexpr std::array<NoMutationObservation, 0> kRedObservations{};

} // namespace

TEST(G104B10NoMutation, CoversRejectedAlreadyAppliedAndPreparedOnBothStores) {
    // RED: B10 is incomplete until the three prepare dispositions are observed
    // without mutation on both ReferenceObjectStore and IndexedObjectStore.
    EXPECT_EQ(kRedObservations.size(), 6U);
}

} // namespace canvas::semantic
