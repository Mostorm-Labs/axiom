#include "../tools/g1_04_c_fixture_runtime.hpp"
#include "../tools/g1_04_c_projection.hpp"

#include <gtest/gtest.h>

namespace canvas::verification::g1_04_c {

TEST(G1CProjection, IsDeterministicAndUsesLogicalFacts) {
    auto first = projectObjects(makeSyntheticStoreObjects());
    auto second = projectObjects(makeSyntheticStoreObjects());
    EXPECT_EQ(first, second);
}

TEST(G1CProjection, CorpusManifestInventoryIsNinetyCases) {
    const auto inventory = loadAcceptedManifest(std::string(G1_C_SOURCE_DIR) + "/verification/corpus/semantic/v1/g1-04-c/generated/manifest.json");
    ASSERT_TRUE(inventory.ok) << inventory.error;
    EXPECT_EQ(inventory.caseIds.size(), 90U);
}

} // namespace canvas::verification::g1_04_c
