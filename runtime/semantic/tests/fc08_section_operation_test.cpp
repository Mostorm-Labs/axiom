#include "canvas/semantic/codec.hpp"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
using Json=nlohmann::json;
const std::filesystem::path root(AXIOM_SOURCE_ROOT);
Json read(const std::filesystem::path& path) { std::ifstream f(path);return Json::parse(f); }
std::vector<std::uint8_t> bytes(const std::filesystem::path& path) { std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}}; }
std::vector<Json> wireCases() {
 std::vector<Json> rows;
 for(const auto& r:read(root/".aegis/materialized/FC-08/attempts/v0.3-control-remediation-02/corpus-manifest.json")["variants"])
  if(r["case_id"]=="FC08-W03" || r["case_id"]=="FC08-W05")rows.push_back(r);
 return rows;
}
class SectionOperationsWire:public testing::TestWithParam<Json>{};
TEST_P(SectionOperationsWire,FrozenWireAdmission) {
 const auto& row=GetParam();const auto expected=read(root/row["expected"]["path"].get<std::string>());
 const auto input=bytes(root/row["wire"][0]["path"].get<std::string>());
 const auto actual=canvas::semantic::SemanticCodec::observeGoldenFixture("Operation",input,true);
 EXPECT_EQ(actual.accepted,expected["predicates"][0]["expected"].get<bool>())<<row["variant_id"];
}
INSTANTIATE_TEST_SUITE_P(Frozen,SectionOperationsWire,testing::ValuesIn(wireCases()),[](const testing::TestParamInfo<Json>& i){return std::to_string(i.index);});
} // namespace
