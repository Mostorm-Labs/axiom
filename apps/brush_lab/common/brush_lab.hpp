#pragma once

#include "canvas/ink/programmable_brush.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace canvas::brush_lab {

struct Workload final {
  std::size_t sampleCount = 0;
  std::size_t primitiveCount = 0;
  std::size_t estimatedBytes = 0;
};

struct Scenario final {
  std::string name;
  std::uint64_t programIdentity = 0;
  std::uint64_t commitDigest = 0;
  std::size_t primitiveCount = 0;
  bool canonicalMutation = false;
  std::string svg;
  Workload workload;
  [[nodiscard]] explicit operator bool() const noexcept {
    return !name.empty() && programIdentity != 0 && primitiveCount != 0;
  }
};

[[nodiscard]] Scenario evaluate(ink::BrushFamily family);
[[nodiscard]] std::string manifestJson();

}  // namespace canvas::brush_lab
