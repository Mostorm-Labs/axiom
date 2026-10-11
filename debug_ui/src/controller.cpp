#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/builtin_panels.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <utility>

namespace canvas::debug_ui {
DebugController::DebugController(DebugControllerContext context)
    : context_(context), router_(context.runtime, context.axiomDebug, context.platformDebug, &activity_) {
  registerBuiltinPanels(registry_);
}

DebugSnapshot DebugController::snapshot() const { return latest_; }
DebugUiSessionState& DebugController::uiState() noexcept { return ui_; }

bool DebugController::buildImGuiFrame() {
  DebugSnapshotSources sources{context_.runtime, context_.axiom, context_.arc,
      context_.platformDiagnostics, context_.telemetry, nullptr};
  auto captured = assembler_.capture(sources);
  router_.beginFrame(captured);
  router_.refreshReceipts();
  captured.activity = activity_.snapshot();
  latest_ = std::move(captured);
  return workbench_.render(latest_, router_, ui_, registry_);
}

bool buildImGuiPanels(const DebugSnapshot& snapshot, int selectedTool, DebugControlRouter* router) {
  (void)selectedTool;
  PanelRegistry registry;
  DebugUiSessionState ui;
  // The temporary host path persists UI-local navigation in this ImGui
  // context's Workbench window, never in a process-global session/registry.
  const ImGuiID workspaceKey = ImHashStr("PX0.SelectedWorkspace");
  const ImGuiID scrollKey = ImHashStr("PX0.ActivityAutoScroll");
  if (auto* window = ImGui::FindWindowByName("Axiom Debug Workbench")) {
    ui.setWorkspace(static_cast<WorkspaceId>(window->StateStorage.GetInt(workspaceKey, 0)));
    ui.setActivityAutoScroll(window->StateStorage.GetBool(scrollKey, true));
    auto& draft = ui.cameraDraft();
    draft.panDeltaX = window->StateStorage.GetFloat(ImHashStr("PX0.PanDeltaX"), 0.0F);
    draft.panDeltaY = window->StateStorage.GetFloat(ImHashStr("PX0.PanDeltaY"), 0.0F);
    draft.zoomAnchorX = window->StateStorage.GetFloat(ImHashStr("PX0.ZoomAnchorX"), 0.0F);
    draft.zoomAnchorY = window->StateStorage.GetFloat(ImHashStr("PX0.ZoomAnchorY"), 0.0F);
    draft.zoomScaleDelta = window->StateStorage.GetFloat(ImHashStr("PX0.ZoomScaleDelta"), 1.0F);
  }
  registerBuiltinPanels(registry);
  DebugControlRouter unavailable(nullptr, nullptr, nullptr, nullptr);
  if (router == nullptr) unavailable.beginFrame(snapshot);
  DebugWorkbench workbench;
  const bool submitted = workbench.render(snapshot, router != nullptr ? *router : unavailable,
                                           ui, registry);
  if (auto* window = ImGui::FindWindowByName("Axiom Debug Workbench")) {
    window->StateStorage.SetInt(workspaceKey, static_cast<int>(ui.workspace()));
    window->StateStorage.SetBool(scrollKey, ui.activityAutoScroll());
    const auto& draft = ui.cameraDraft();
    window->StateStorage.SetFloat(ImHashStr("PX0.PanDeltaX"), draft.panDeltaX);
    window->StateStorage.SetFloat(ImHashStr("PX0.PanDeltaY"), draft.panDeltaY);
    window->StateStorage.SetFloat(ImHashStr("PX0.ZoomAnchorX"), draft.zoomAnchorX);
    window->StateStorage.SetFloat(ImHashStr("PX0.ZoomAnchorY"), draft.zoomAnchorY);
    window->StateStorage.SetFloat(ImHashStr("PX0.ZoomScaleDelta"), draft.zoomScaleDelta);
  }
  return submitted;
}
}  // namespace canvas::debug_ui
