#include "canvas/debug_ui/panel_model.hpp"

#include <cassert>

int main() {
  canvas::debug_ui::DebugSnapshot snapshot{};
  snapshot.arc.availability = canvas::debug_ui::DebugAvailability::kAvailable;
  snapshot.platform.availability = canvas::debug_ui::DebugAvailability::kAvailable;
  snapshot.stamp.runtimeGeneration = 1;
  const auto panels = canvas::debug_ui::DebugPanelModel::describe(snapshot);
  assert(panels[static_cast<std::size_t>(canvas::debug_ui::DebugPanel::kInput)].available);
  assert(panels[static_cast<std::size_t>(canvas::debug_ui::DebugPanel::kCanvas)].available);
  assert(!panels[static_cast<std::size_t>(canvas::debug_ui::DebugPanel::kTelemetry)].available);
  assert(!panels[static_cast<std::size_t>(canvas::debug_ui::DebugPanel::kInspection)].available);
  snapshot.product.availability = canvas::debug_ui::DebugAvailability::kAvailable;
  assert(canvas::debug_ui::DebugPanelModel::canSubmitProductControl(snapshot));
  snapshot.stamp.runtimeGeneration = 0;
  assert(!canvas::debug_ui::DebugPanelModel::canSubmitProductControl(snapshot));
  return 0;
}
