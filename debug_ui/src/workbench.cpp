#include "canvas/debug_ui/workbench.hpp"
#include "imgui.h"
#include "panels/widgets.hpp"

#include <algorithm>

namespace canvas::debug_ui {
namespace {
const char* activityLabel(DebugActivityState state) noexcept {
  switch (state) {
    case DebugActivityState::kPending: return "Pending";
    case DebugActivityState::kApplied: return "Applied";
    case DebugActivityState::kRejected: return "Rejected";
    case DebugActivityState::kUnsupported: return "Unsupported";
    case DebugActivityState::kUnavailable: return "Unavailable";
    case DebugActivityState::kStaleGeneration: return "Stale generation";
    case DebugActivityState::kExpired: return "Expired";
    case DebugActivityState::kQueueFull: return "Queue full";
    case DebugActivityState::kFailed: return "Failed";
  }
  return "Unavailable";
}
}

bool DebugWorkbench::render(const DebugSnapshot& snapshot, DebugControlRouter& controls,
                            DebugUiSessionState& ui, const PanelRegistry& registry) {
  ui.beginFrame();
  const auto display = ImGui::GetIO().DisplaySize;
  // Hosts supply actual geometry. Preferred dimensions apply only without it.
  constexpr ImVec2 preferred{900.0f, 640.0f};
  const float width = display.x > 0.0f ? display.x : preferred.x;
  const float height = display.y > 0.0f ? display.y : preferred.y;
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
  if (!ImGui::Begin("Axiom Debug Workbench", nullptr, flags)) {
    ImGui::End();
    return false;
  }

  const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
  const float chromeHeight = rowHeight * ((ui.advancedControls() || snapshot.coherence != SnapshotCoherence::kCoherent) ? 2.0f : 1.0f) + ImGui::GetStyle().WindowPadding.y * 2.0f;
  if (ImGui::BeginChild("##header", ImVec2(0, chromeHeight), ImGuiChildFlags_Borders)) {
    const char* coherence = snapshot.coherence == SnapshotCoherence::kCoherent ? "Coherent" :
        snapshot.coherence == SnapshotCoherence::kMixedGeneration ? "Mixed generation" : "Stale";
    if (ui.advancedControls()) {
      ImGui::Text("Workbench | runtime %llu | document %llu",
          static_cast<unsigned long long>(snapshot.stamp.runtimeGeneration),
          static_cast<unsigned long long>(snapshot.stamp.documentGeneration));
      ImGui::Text("%s | frame %llu | snapshot #%llu", coherence,
          static_cast<unsigned long long>(snapshot.stamp.frameId),
          static_cast<unsigned long long>(snapshot.stamp.snapshotSequence));
    } else {
      ImGui::TextUnformatted("Axiom Debug Workbench");
      if (snapshot.coherence != SnapshotCoherence::kCoherent)
        ImGui::TextColored(ImVec4(1,0.6F,0.2F,1), "%s: controls unavailable", coherence);
    }
  }
  ImGui::EndChild();

  const bool compact = width < 650.0F;
  const auto navigation = [&] {
    bool first = true;
    for (const auto workspace : {WorkspaceId::kControl, WorkspaceId::kInspect, WorkspaceId::kPerformance}) {
      if (compact ? panel_detail::flowButton(workspaceLabel(workspace).data(), first, ui.workspace() == workspace)
                  : ImGui::Selectable(workspaceLabel(workspace).data(), ui.workspace() == workspace))
        ui.setWorkspace(workspace);
    }
    bool advanced = ui.advancedControls();
    if (ImGui::Checkbox("Advanced", &advanced)) ui.setAdvancedControls(advanced);
    if (advanced) {
      bool advancedFirst=true;
      for (const auto workspace : {WorkspaceId::kDashboard, WorkspaceId::kRuntime}) {
        if (compact ? panel_detail::flowButton(workspaceLabel(workspace).data(), advancedFirst, ui.workspace() == workspace)
                    : ImGui::Selectable(workspaceLabel(workspace).data(), ui.workspace() == workspace))
          ui.setWorkspace(workspace);
      }
    }
  };
  if (compact) navigation();
  const float bodyHeight = (std::max)(rowHeight * 4.0F,
      ImGui::GetContentRegionAvail().y - chromeHeight - ImGui::GetStyle().ItemSpacing.y);
  if (!compact) {
    const float sidebarWidth = std::clamp(width * 0.20F, 110.0F, 150.0F);
    if (ImGui::BeginChild("##sidebar", ImVec2(sidebarWidth, bodyHeight), ImGuiChildFlags_Borders)) navigation();
    ImGui::EndChild();
    ImGui::SameLine();
  }
  if (ImGui::BeginChild("##content", ImVec2(0, bodyHeight), ImGuiChildFlags_Borders)) {
    ImGui::TextUnformatted(workspaceLabel(ui.workspace()).data());
    ImGui::Separator();
    bool rendered = false;
    DebugPanelContext context{snapshot, controls, ui};
    const auto render = [&](const PanelContribution& contribution) {
      if (contribution.visible != nullptr && !contribution.visible(snapshot)) return;
      contribution.render(context);
      rendered = true;
    };
    if (ui.workspace() == WorkspaceId::kControl) {
      for (const auto slot : {PanelSlot::kHistory, PanelSlot::kView, PanelSlot::kTools, PanelSlot::kInk, PanelSlot::kFeature})
        for (const auto& contribution : registry.contributions(ui.workspace()))
          if (contribution.slot == slot) render(contribution);
    } else {
      for (const auto& contribution : registry.contributions(ui.workspace())) render(contribution);
    }
    if (!rendered) {
      ImGui::TextWrapped("No panels registered for %s (not yet migrated).",
                         workspaceLabel(ui.workspace()).data());
    }
  }
  ImGui::EndChild();

  if (ImGui::BeginChild("##footer", ImVec2(0, chromeHeight), ImGuiChildFlags_Borders)) {
    if (snapshot.activity.entries.empty()) {
      ImGui::TextUnformatted("No control activity");
    } else {
      const auto& entry = snapshot.activity.entries.back();
      if (ui.advancedControls()) ImGui::Text("Activity #%llu | request %llu | %s",
          static_cast<unsigned long long>(entry.sequence),
          static_cast<unsigned long long>(entry.requestId), activityLabel(entry.state));
      const bool failed = entry.state != DebugActivityState::kApplied && entry.state != DebugActivityState::kPending;
      if (failed) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,0.6F,0.2F,1));
      ImGui::TextWrapped("%s: %s", entry.action.c_str(), activityLabel(entry.state));
      if (failed) ImGui::PopStyleColor();
    }
  }
  ImGui::EndChild();
  ImGui::End();
  return ui.controlSubmittedThisFrame();
}
}  // namespace canvas::debug_ui
