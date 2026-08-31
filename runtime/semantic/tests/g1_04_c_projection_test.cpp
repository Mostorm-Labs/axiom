#include "../tools/g1_04_c_fixture_runtime.hpp"
#include "../tools/g1_04_c_projection.hpp"

#include <gtest/gtest.h>

#include <algorithm>

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

TEST(G1CProjection, RecordsObjectValuedStoreAndLogicalPlanFacts) {
    const auto report = runAcceptedCorpus(
        std::string(G1_C_SOURCE_DIR) + "/verification/corpus/semantic/v1/g1-04-c/generated/manifest.json");
    ASSERT_TRUE(report.ok) << report.error;
    const auto planReady = std::find_if(report.observationFacts.begin(), report.observationFacts.end(), [](const auto& fact) {
        return fact.caseId == "C1-DELETE-VALID" && fact.provider == "reference";
    });
    ASSERT_NE(planReady, report.observationFacts.end());
    EXPECT_EQ(planReady->beforeProjection, planReady->afterProjection);
    ASSERT_TRUE(planReady->observedPlanProjection.has_value());
    EXPECT_FALSE(planReady->observedPlanProjection->deletes.empty());
}

} // namespace canvas::verification::g1_04_c
