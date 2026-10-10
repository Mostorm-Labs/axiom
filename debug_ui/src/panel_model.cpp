#include "canvas/debug_ui/panel_model.hpp"

namespace canvas::debug_ui {

std::array<PanelState, static_cast<std::size_t>(DebugPanel::kCount)>
DebugPanelModel::describe(const DebugSnapshot& snapshot) noexcept {
  return {{
      {DebugPanel::kOverview, panelAvailable(snapshot, PanelCapability::kOverview), "Overview"},
      {DebugPanel::kInput, panelAvailable(snapshot, PanelCapability::kInput), "Input"},
      {DebugPanel::kCanvas, panelAvailable(snapshot, PanelCapability::kCanvas), "Canvas"},
      {DebugPanel::kArcPreview, panelAvailable(snapshot, PanelCapability::kArcPreview), "Arc Preview"},
      {DebugPanel::kSurface, panelAvailable(snapshot, PanelCapability::kSurface), "Surface"},
      {DebugPanel::kBrush, panelAvailable(snapshot, PanelCapability::kBrush), "Brush"},
      {DebugPanel::kTelemetry, panelAvailable(snapshot, PanelCapability::kTelemetry), "Telemetry"},
      {DebugPanel::kInspection, panelAvailable(snapshot, PanelCapability::kInspection), "Inspection"},
  }};
}

bool DebugPanelModel::canSubmitProductControl(const DebugSnapshot& snapshot) noexcept {
  return snapshot.product.availability == DebugAvailability::kAvailable &&
         snapshot.platform.availability == DebugAvailability::kAvailable &&
         snapshot.stamp.runtimeGeneration != 0U;
}

}  // namespace canvas::debug_ui
