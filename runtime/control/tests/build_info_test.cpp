#include "canvas/control/build_info.hpp"

#include <cassert>

int main() {
  const auto info = canvas::runtime::readRuntimeBuildInfo();
  assert(!info.sourceRevision.empty());
  assert(!info.platform.empty());
  assert(!info.configuration.empty());
  assert(!info.version.empty());
  return 0;
}
