#include "../tools/g1_04_c_fixture_runtime.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace canvas::verification::g1_04_c {

TEST(G1CObserverCoverage, NonFiniteCasesReachProductionLane) {
    const auto report = runAcceptedCorpus(
        std::string(G1_C_SOURCE_DIR) + "/verification/corpus/semantic/v1/g1-04-c/generated/manifest.json");
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.caseObserved("C1-TRANSFORM-NAN-INF"));
    EXPECT_TRUE(report.caseObserved("C1-SIZE-NONFINITE"));
    EXPECT_TRUE(report.caseObserved("C1-TRANSFORM-NEGATIVE-ZERO"));
    EXPECT_EQ(report.expectedTruthReads, 0U);
    EXPECT_EQ(report.semanticCodecCalls, 0U);
    const auto nanInf = std::find_if(report.observationFacts.begin(), report.observationFacts.end(), [](const auto& fact) {
        return fact.caseId == "C1-TRANSFORM-NAN-INF" && fact.provider == "reference";
    });
    ASSERT_NE(nanInf, report.observationFacts.end());
    EXPECT_NE(std::find(nanInf->f64Bits.begin(), nanInf->f64Bits.end(), 0x7ff8000000000000ULL), nanInf->f64Bits.end());
    EXPECT_NE(std::find(nanInf->f64Bits.begin(), nanInf->f64Bits.end(), 0x7ff0000000000000ULL), nanInf->f64Bits.end());
    EXPECT_NE(std::find(nanInf->f64Bits.begin(), nanInf->f64Bits.end(), 0xfff0000000000000ULL), nanInf->f64Bits.end());
}

} // namespace canvas::verification::g1_04_c
