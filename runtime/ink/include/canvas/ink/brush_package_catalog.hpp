#pragma once

#include "canvas/ink/brush_package.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace canvas::ink {

// Runtime-owned package catalog. Platform adapters select a logical profile
// and revision; they never provide package JSON or mutate a resolved package.
class BrushPackageCatalog final {
 public:
  [[nodiscard]] BrushPackageResult loadDefault(std::string_view profile,
                                                std::uint32_t revision = 1) const;
  [[nodiscard]] BrushPackageResult loadDirectory(const std::string& manifestPath,
                                                 const std::string& pipelinePath) const;
};

}  // namespace canvas::ink
