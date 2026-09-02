#include "../tools/g1_04_c_fixture_runtime.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

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
    for (const auto* name : {
             "g1_04_c_fixture_decoder.cpp",
             "g1_04_c_fixture_runtime.cpp",
             "g1_04_c_observer.cpp",
         }) {
        const auto sourcePath = std::filesystem::path(G1_C_SOURCE_DIR) / "runtime/semantic/tools" / name;
        std::ifstream input(sourcePath);
        ASSERT_TRUE(input) << sourcePath;
        std::ostringstream source; source << input.rdbuf();
        const auto text = source.str();
        EXPECT_EQ(text.find("authoring/expected.json"), std::string::npos) << sourcePath;
        EXPECT_EQ(text.find("SemanticCodec"), std::string::npos) << sourcePath;
        EXPECT_EQ(text.find("codec.hpp"), std::string::npos) << sourcePath;
        if (std::string_view(name) == "g1_04_c_observer.cpp") {
            EXPECT_EQ(text.find("object_store_mutator.hpp"), std::string::npos) << sourcePath;
        }
    }
}

} // namespace canvas::verification::g1_04_c
