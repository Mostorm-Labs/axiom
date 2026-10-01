#include "brush_lab.hpp"

#include <array>
#include <cassert>
#include <fstream>
#include <sstream>
#include <string_view>

int main() {
  const std::array families{
      canvas::ink::BrushFamily::kPen, canvas::ink::BrushFamily::kPencil,
      canvas::ink::BrushFamily::kChalk, canvas::ink::BrushFamily::kMarker,
      canvas::ink::BrushFamily::kWaterColorLite,
      canvas::ink::BrushFamily::kHighlighter,
      canvas::ink::BrushFamily::kLaser};
  std::uint64_t lastProgram = 0;
  for (const auto family : families) {
    const auto scenario = canvas::brush_lab::evaluate(family);
    assert(scenario);
    assert(!scenario.name.empty());
    assert(scenario.programIdentity != 0);
    assert(scenario.programIdentity != lastProgram);
    assert(scenario.primitiveCount == 5);
    assert(scenario.svg.find("<svg") != std::string::npos);
    assert(scenario.svg.find("data-brush=") != std::string::npos);
    assert(scenario.workload.sampleCount == 5);
    assert(scenario.workload.primitiveCount == 5);
    std::ifstream golden(std::string(G45_GOLDEN_ROOT) + "/" + scenario.name + ".svg",
                         std::ios::binary);
    std::ostringstream expected;
    expected << golden.rdbuf();
    assert(golden.good() || golden.eof());
    assert(expected.str() == scenario.svg + "\n");
    if (family == canvas::ink::BrushFamily::kLaser) {
      assert(!scenario.canonicalMutation);
      assert(scenario.commitDigest != 0);
    } else {
      assert(scenario.canonicalMutation);
      assert(scenario.commitDigest != 0);
    }
    lastProgram = scenario.programIdentity;
  }

  const auto manifest = canvas::brush_lab::manifestJson();
  assert(manifest.find("\"schema_version\":\"0.1\"") != std::string::npos);
  assert(manifest.find("\"scenario_count\":7") != std::string::npos);
  assert(manifest.find("\"laser\"") != std::string::npos);
  std::ifstream goldenManifest(std::string(G45_GOLDEN_ROOT) + "/manifest.json",
                               std::ios::binary);
  std::ostringstream expectedManifest;
  expectedManifest << goldenManifest.rdbuf();
  assert(goldenManifest.good() || goldenManifest.eof());
  assert(expectedManifest.str() == manifest + "\n");
  return 0;
}
