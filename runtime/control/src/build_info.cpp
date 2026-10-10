#include "canvas/control/build_info.hpp"

#ifndef AXIOM_BUILD_SOURCE_REVISION
#define AXIOM_BUILD_SOURCE_REVISION "unknown"
#endif
#ifndef AXIOM_BUILD_CONFIGURATION
#define AXIOM_BUILD_CONFIGURATION "unknown"
#endif
#ifndef AXIOM_BUILD_PLATFORM
#define AXIOM_BUILD_PLATFORM "unknown"
#endif
#ifndef AXIOM_BUILD_VERSION
#define AXIOM_BUILD_VERSION "0.1.0"
#endif
#ifndef AXIOM_BUILD_DIRTY
#define AXIOM_BUILD_DIRTY 1
#endif

namespace canvas::runtime {
RuntimeBuildInfo readRuntimeBuildInfo() noexcept {
  return {AXIOM_BUILD_SOURCE_REVISION, AXIOM_BUILD_CONFIGURATION,
          AXIOM_BUILD_PLATFORM, AXIOM_BUILD_VERSION, AXIOM_BUILD_DIRTY != 0};
}
}  // namespace canvas::runtime
