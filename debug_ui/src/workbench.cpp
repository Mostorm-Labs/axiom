#include "canvas/debug_ui/workbench.hpp"
#include "imgui.h"

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
  const float chromeHeight = rowHeight * 2.0f + ImGui::GetStyle().WindowPadding.y * 2.0f;
  if (ImGui::BeginChild("##header", ImVec2(0, chromeHeight), ImGuiChildFlags_Borders)) {
    const char* coherence = snapshot.coherence == SnapshotCoherence::kCoherent ? "Coherent" :
        snapshot.coherence == SnapshotCoherence::kMixedGeneration ? "Mixed generation" : "Stale";
    ImGui::Text("Workbench | runtime %llu | document %llu",
        static_cast<unsigned long long>(snapshot.stamp.runtimeGeneration),
        static_cast<unsigned long long>(snapshot.stamp.documentGeneration));
    ImGui::Text("%s | frame %llu | snapshot #%llu", coherence,
        static_cast<unsigned long long>(snapshot.stamp.frameId),
        static_cast<unsigned long long>(snapshot.stamp.snapshotSequence));
  }
  ImGui::EndChild();

  const float bodyHeight = (std::max)(rowHeight * 6.0f,
      ImGui::GetContentRegionAvail().y - chromeHeight - ImGui::GetStyle().ItemSpacing.y);
  const float sidebarWidth = std::clamp(width * 0.26f, 110.0f, 170.0f);
  if (ImGui::BeginChild("##sidebar", ImVec2(sidebarWidth, bodyHeight), ImGuiChildFlags_Borders)) {
    for (std::size_t i = 0; i < kWorkspaceLabels.size(); ++i) {
      const auto workspace = static_cast<WorkspaceId>(i);
      if (ImGui::Selectable(workspaceLabel(workspace).data(), ui.workspace() == workspace)) {
        ui.setWorkspace(workspace);
      }
    }
  }
  ImGui::EndChild();
  ImGui::SameLine();
  if (ImGui::BeginChild("##content", ImVec2(0, bodyHeight), ImGuiChildFlags_Borders)) {
    ImGui::TextUnformatted(workspaceLabel(ui.workspace()).data());
    ImGui::Separator();
    bool rendered = false;
    DebugPanelContext context{snapshot, controls, ui};
    for (const auto& contribution : registry.contributions(ui.workspace())) {
      if (contribution.visible != nullptr && !contribution.visible(snapshot)) continue;
      contribution.render(context);
      rendered = true;
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
      ImGui::Text("Activity #%llu | request %llu | %s",
          static_cast<unsigned long long>(entry.sequence),
          static_cast<unsigned long long>(entry.requestId), activityLabel(entry.state));
      ImGui::TextUnformatted(entry.action.c_str());
    }
  }
  ImGui::EndChild();
  ImGui::End();
  return ui.controlSubmittedThisFrame();
}
}  // namespace canvas::debug_ui
