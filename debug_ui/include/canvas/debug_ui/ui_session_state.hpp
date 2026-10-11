#pragma once

#include "canvas/debug_ui/workspace.hpp"

namespace canvas::debug_ui {

class DebugUiSessionState final {
 public:
  [[nodiscard]] WorkspaceId selectedWorkspace() const noexcept { return workspace_; }
  [[nodiscard]] WorkspaceId workspace() const noexcept { return workspace_; }
  void setSelectedWorkspace(WorkspaceId workspace) noexcept { setWorkspace(workspace); }
  void setWorkspace(WorkspaceId workspace) noexcept {
    if (isValidWorkspace(workspace)) workspace_ = workspace;
  }
  [[nodiscard]] bool activityAutoScroll() const noexcept { return activityAutoScroll_; }
  void setActivityAutoScroll(bool enabled) noexcept { activityAutoScroll_ = enabled; }
  [[nodiscard]] bool controlSubmittedThisFrame() const noexcept { return controlSubmittedThisFrame_; }
  void markControlSubmitted() noexcept { controlSubmittedThisFrame_ = true; }
  void beginFrame() noexcept { controlSubmittedThisFrame_ = false; }

 private:
  WorkspaceId workspace_ = WorkspaceId::kDashboard;
  bool activityAutoScroll_ = true;
  bool controlSubmittedThisFrame_ = false;
};

}  // namespace canvas::debug_ui
