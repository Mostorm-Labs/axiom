#include "ink_playground_host.hpp"
#include "platform_brush_baseline_observation.hpp"

#include <fstream>
#include <string>

namespace {
using canvas::ink_playground::InkPlaygroundHost;

void writeObservation(const std::string& path, const std::string& platform,
                      const std::string& fixtureDigest, const InkPlaygroundHost& host) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  (void)fixtureDigest;
  out << canvas::ink_playground::platformBrushBaselineObservationJson(
      platform, "HOSTED", "", host,
      {"fixture→PlatformPointerBatch", "C++ InkPlaygroundHost", "host-only", "none",
       0, 0, 0, 0, "unknown"});
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) return 2;
  InkPlaygroundHost host;
  if (!canvas::ink_playground::runPlatformBrushBaselineFixture(host)) return 3;
  writeObservation(argv[3], argv[1], argv[2], host);
  return 0;
}
