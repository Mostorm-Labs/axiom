#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace canvas::debug_ui {

enum class WorkspaceId : std::uint8_t {
  kDashboard = 0,
  kControl,
  kInspect,
  kRuntime,
  kPerformance,
  kScenarios,
  kCount,
};

inline constexpr std::array<std::string_view, static_cast<std::size_t>(WorkspaceId::kCount)>
    kWorkspaceLabels{{"Dashboard", "Control", "Inspect", "Runtime", "Performance", "Scenarios"}};

[[nodiscard]] constexpr bool isValidWorkspace(WorkspaceId workspace) noexcept {
  return static_cast<std::size_t>(workspace) < static_cast<std::size_t>(WorkspaceId::kCount);
}

[[nodiscard]] constexpr std::string_view workspaceLabel(WorkspaceId workspace) noexcept {
  return isValidWorkspace(workspace) ? kWorkspaceLabels[static_cast<std::size_t>(workspace)] : "Invalid";
}

}  // namespace canvas::debug_ui
