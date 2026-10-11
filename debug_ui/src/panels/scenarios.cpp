#include "canvas/debug_ui/builtin_panels.hpp"
#include "imgui.h"

namespace canvas::debug_ui {
namespace {
void smoke(DebugPanelContext&) {
    ImGui::SeparatorText("Smoke");
    ImGui::TextUnformatted("Reset Canvas - Unsupported / Not implemented");
    ImGui::TextUnformatted("Empty Canvas - Unsupported / Not implemented");
    ImGui::TextUnformatted("Ink Baseline - Unsupported / Not implemented");
}
} // namespace
void registerScenarioDebugPanels(PanelRegistry& registry) noexcept {
    (void)registry.add({WorkspaceId::kScenarios, PanelSlot::kSmoke, 0, nullptr, smoke});
}
} // namespace canvas::debug_ui
