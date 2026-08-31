#include "../tools/g1_04_c_fixture_decoder.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <filesystem>
#include <variant>

namespace canvas::verification::g1_04_c {
using namespace canvas::semantic;

TEST(G1CFixtureDecoder, ReconstructsNonFiniteCarrierBits) {
    const auto result = decodeFixtureJson(
        R"({"format":"axiom-g1-04-c-input-v1","formatVersion":1,"caseId":"synthetic","operationFamily":"SetTransforms","initialState":{"objects":[],"priorOperations":[]},"operation":{"id":"01010101010101010101010101010101","document_id":"02020202020202020202020202020202","schema_version":1,"payload_version":1,"payload":{"variant":4,"value":{"items":[{"object_id":"03030303030303030303030303030303","transform":["f64:7ff8000000000000","f64:7ff0000000000000","f64:fff0000000000000","f64:8000000000000000",0.0,0.0]}]}}},"executionVariants":[]})");
    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_TRUE(result.fixture.has_value());
    const auto* payload = std::get_if<SetTransformsOp>(&result.fixture->operation.payload);
    ASSERT_NE(payload, nullptr);
    ASSERT_EQ(payload->items.size(), 1U);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(payload->items[0].transform.a), 0x7ff8000000000000ULL);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(payload->items[0].transform.b), 0x7ff0000000000000ULL);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(payload->items[0].transform.c), 0xfff0000000000000ULL);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(payload->items[0].transform.d), 0x8000000000000000ULL);
}

TEST(G1CFixtureDecoder, RejectsWrongTypedCarrierAsHarnessError) {
    const auto result = decodeFixtureJson(
        R"({"format":"axiom-g1-04-c-input-v1","formatVersion":1,"caseId":"synthetic","operationFamily":"SetObjectSize","initialState":{"objects":[],"priorOperations":[]},"operation":{"id":"01010101010101010101010101010101","document_id":"02020202020202020202020202020202","schema_version":1,"payload_version":1,"payload":{"variant":6,"value":{"items":[{"object_id":"03030303030303030303030303030303","width":{},"height":1.0}]}}},"executionVariants":[]})");
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.errorCode, "HARNESS_ERROR");
}

TEST(G1CFixtureDecoder, KeepsF64PrefixInUntypedString) {
    const auto result = decodeStringScalar("f64:7ff8000000000000");
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.value, "f64:7ff8000000000000");
}

TEST(G1CFixtureDecoder, RejectsMalformedCarrierSyntax) {
    const auto result = decodeFixtureJson(
        R"({"format":"axiom-g1-04-c-input-v1","formatVersion":1,"caseId":"synthetic","operationFamily":"SetTransforms","initialState":{"objects":[],"priorOperations":[]},"operation":{"id":"01010101010101010101010101010101","document_id":"02020202020202020202020202020202","schema_version":1,"payload_version":1,"payload":{"variant":4,"value":{"items":[{"object_id":"03030303030303030303030303030303","transform":["f64:7FF8000000000000",0.0,0.0,1.0,0.0,0.0]}]}}},"executionVariants":[]})");
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.errorCode, "HARNESS_ERROR");
}

TEST(G1CFixtureDecoder, ExpandsAuthorityGeometryRecipeAtDecoderBoundary) {
    const auto path = std::filesystem::path(G1_C_SOURCE_DIR) /
        "verification/corpus/semantic/v1/g1-04-c/generated/inputs/C1-GEOMETRY-N-1.json";
    const auto result = decodeFixtureFile(path.string());
    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_TRUE(result.fixture.has_value());
    const auto* payload = std::get_if<SetVectorPathGeometryOp>(&result.fixture->operation.payload);
    ASSERT_NE(payload, nullptr);
    ASSERT_EQ(payload->geometry.commands.size(), 1'999'996U);
    EXPECT_TRUE(std::holds_alternative<MoveTo>(payload->geometry.commands.front()));
    EXPECT_TRUE(std::holds_alternative<QuadTo>(payload->geometry.commands[payload->geometry.commands.size() - 2U]));
    EXPECT_TRUE(std::holds_alternative<CubicTo>(payload->geometry.commands.back()));
}

} // namespace canvas::verification::g1_04_c
