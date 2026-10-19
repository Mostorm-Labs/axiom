#pragma once

#include <cstdint>
#include <string_view>

namespace canvas::runtime {

struct RuntimeBuildInfo final {
  std::string_view sourceRevision{};
  std::string_view configuration{};
  std::string_view platform{};
  std::string_view version{};
  bool dirty = false;
};

[[nodiscard]] RuntimeBuildInfo readRuntimeBuildInfo() noexcept;

}  // namespace canvas::runtime
