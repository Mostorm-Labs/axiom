#include "../common/ink_playground_host.hpp"
#include "canvas/control/build_info.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
  const std::filesystem::path output = argc > 1 ? argv[1] : "dui-m1-qualification";
  std::error_code error;
  std::filesystem::create_directories(output, error);
  if (error) return 2;
  canvas::ink_playground::InkPlaygroundHost host;
  const bool bound = host.bindSurface(320, 240);
  const auto build = canvas::runtime::readRuntimeBuildInfo();
  std::ofstream result(output / "qualification.json");
  result << "{\"task_id\":\"GT-DUI-M1\",\"source_revision\":\""
         << build.sourceRevision << "\",\"platform\":\"" << build.platform
         << "\",\"configuration\":\"" << build.configuration
         << "\",\"dirty\":" << (build.dirty ? "true" : "false")
         << ",\"surface_bound\":" << (bound ? "true" : "false")
         << ",\"windows\":{\"build_ready\":true,\"physical\":\"PHYSICAL_PENDING\"}"
         << ",\"web\":{\"build_ready\":false,\"physical\":\"PHYSICAL_PENDING\"}"
         << ",\"android\":{\"build_ready\":false,\"physical\":\"PHYSICAL_PENDING\"}}\n";
  return bound ? 0 : 3;
}
