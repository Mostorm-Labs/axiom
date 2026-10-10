#include "canvas/debug_ui/panel_model.hpp"

#include <cassert>

int main() {
  canvas::debug_ui::DebugSnapshot snapshot{};
  snapshot.capabilities.fill(canvas::debug_ui::CapabilityState::kUnavailable);
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kInput)] =
      canvas::debug_ui::CapabilityState::kAvailable;
  snapshot.capabilities[static_cast<std::size_t>(canvas::debug_ui::Capability::kCanonicalSurface)] =
      canvas::debug_ui::CapabilityState::kAvailable;
  snapshot.stamp.runtimeGeneration = 1;
  const auto panels = canvas::debug_ui::DebugPanelModel::describe(snapshot);
  assert(panels[static_cast<std::size_t>(canvas::debug_ui::DebugPanel::kInput)].available);
  assert(panels[static_cast<std::size_t>(canvas::debug_ui::DebugPanel::kCanvas)].available);
  assert(!panels[static_cast<std::size_t>(canvas::debug_ui::DebugPanel::kTelemetry)].available);
  assert(!panels[static_cast<std::size_t>(canvas::debug_ui::DebugPanel::kInspection)].available);
  assert(canvas::debug_ui::DebugPanelModel::canSubmitProductControl(snapshot));
  snapshot.stamp.runtimeGeneration = 0;
  assert(!canvas::debug_ui::DebugPanelModel::canSubmitProductControl(snapshot));
  return 0;
}
