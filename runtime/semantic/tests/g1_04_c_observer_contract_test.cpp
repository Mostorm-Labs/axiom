#include "../tools/g1_04_c_fixture_runtime.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace canvas::verification::g1_04_c {

TEST(G1CObserverContract, AcceptedCorpusRunsBothProvidersWithoutMutation) {
    const auto report = runAcceptedCorpus(
        std::string(G1_C_SOURCE_DIR) + "/verification/corpus/semantic/v1/g1-04-c/generated/manifest.json");
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_EQ(report.acceptedCases, 90U);
    EXPECT_EQ(report.observations, 180U);
    EXPECT_EQ(report.unexpectedHarnessErrors, 0U);
    EXPECT_EQ(report.unchangedObservations, 180U);
}

TEST(G1CObserverContract, ObserverBoundaryDoesNotDependOnExpectedTruthOrMutationHelper) {
    const auto sourcePath = std::filesystem::path(__FILE__).parent_path() / "../tools/g1_04_c_observer.cpp";
    std::ifstream input(sourcePath);
    std::ostringstream source; source << input.rdbuf();
    const auto text = source.str();
    EXPECT_EQ(text.find("expected.json"), std::string::npos);
    EXPECT_EQ(text.find("object_store_mutator.hpp"), std::string::npos);
    EXPECT_EQ(text.find("PASS"), std::string::npos);
    EXPECT_EQ(text.find("FAIL"), std::string::npos);
}

} // namespace canvas::verification::g1_04_c
