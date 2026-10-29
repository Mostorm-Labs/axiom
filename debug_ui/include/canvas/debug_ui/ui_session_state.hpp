#pragma once

#include "canvas/debug_ui/workspace.hpp"
#include "canvas/runtime/canvas_control_types.hpp"
#if AXIOM_DEBUG_UI_HAS_MULTI_SELECTION
#include "canvas/runtime/internal/multi_selection_port.hpp"
#endif

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
  [[nodiscard]] bool advancedControls() const noexcept { return advancedControls_; }
  void setAdvancedControls(bool enabled) noexcept {
    advancedControls_ = enabled;
    if (!enabled && (workspace_ == WorkspaceId::kDashboard || workspace_ == WorkspaceId::kRuntime ||
                     workspace_ == WorkspaceId::kScenarios)) workspace_ = WorkspaceId::kControl;
  }
  void setActivityAutoScroll(bool enabled) noexcept { activityAutoScroll_ = enabled; }
  [[nodiscard]] bool controlSubmittedThisFrame() const noexcept { return controlSubmittedThisFrame_; }
  void markControlSubmitted() noexcept { controlSubmittedThisFrame_ = true; }
#if AXIOM_DEBUG_UI_HAS_MULTI_SELECTION
  struct MultiSelectionDraft final {
    canvas::runtime::MultiSelectionStamp stamp{};
    bool initialized = false;
    bool visible = true, locked = false;
    float opacity = 1.0F;
    float fill[4] = {0, 0, 0, 1};
    bool noFill = false;
    double strokeWidth = 1.0;
    int startDecoration = 0, endDecoration = 0;
  };
  [[nodiscard]] MultiSelectionDraft& multiSelectionDraft() noexcept { return multiSelectionDraft_; }
#endif
  struct CameraDraft final {
    float panDeltaX = 0.0F;
    float panDeltaY = 0.0F;
    float zoomAnchorX = 0.0F;
    float zoomAnchorY = 0.0F;
    float zoomScaleDelta = 1.0F;
  };
  [[nodiscard]] CameraDraft& cameraDraft() noexcept { return cameraDraft_; }
  [[nodiscard]] const CameraDraft& cameraDraft() const noexcept { return cameraDraft_; }
  [[nodiscard]] canvas::runtime::CanvasCreationPreset creationPresetDraft() const noexcept {
    return creationPresetDraft_;
  }
  void setCreationPresetDraft(canvas::runtime::CanvasCreationPreset preset) noexcept {
    creationPresetDraft_ = preset;
  }
  void beginFrame() noexcept { controlSubmittedThisFrame_ = false; }
  struct ImageDraft final {
    std::uint64_t token = 0;
    double rect[4] = {0, 0, 1, 1};
    double width = 0, height = 0;
    float scale = 1;
  };
  [[nodiscard]] ImageDraft& imageDraft() noexcept { return imageDraft_; }

 private:
  WorkspaceId workspace_ = WorkspaceId::kControl;
  bool advancedControls_ = false;
  bool activityAutoScroll_ = true;
  bool controlSubmittedThisFrame_ = false;
#if AXIOM_DEBUG_UI_HAS_MULTI_SELECTION
  MultiSelectionDraft multiSelectionDraft_{};
#endif
  CameraDraft cameraDraft_{};
  ImageDraft imageDraft_{};
  canvas::runtime::CanvasCreationPreset creationPresetDraft_ =
      canvas::runtime::CanvasCreationPreset::kRectangle;
};

}  // namespace canvas::debug_ui
