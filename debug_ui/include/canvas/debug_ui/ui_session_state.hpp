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
  struct CameraDraft final {
    float panDeltaX = 0.0F;
    float panDeltaY = 0.0F;
    float zoomAnchorX = 0.0F;
    float zoomAnchorY = 0.0F;
    float zoomScaleDelta = 1.0F;
  };
  [[nodiscard]] CameraDraft& cameraDraft() noexcept { return cameraDraft_; }
  [[nodiscard]] const CameraDraft& cameraDraft() const noexcept { return cameraDraft_; }
  void beginFrame() noexcept { controlSubmittedThisFrame_ = false; }

 private:
  WorkspaceId workspace_ = WorkspaceId::kDashboard;
  bool activityAutoScroll_ = true;
  bool controlSubmittedThisFrame_ = false;
  CameraDraft cameraDraft_{};
};

}  // namespace canvas::debug_ui
