#pragma once

#include "canvas/debug_ui/panel_registry.hpp"

namespace canvas::debug_ui {
namespace panel_detail {
[[nodiscard]] inline const char* availabilityLabel(DebugAvailability state) noexcept {
  switch (state) {
    case DebugAvailability::kAvailable: return "Available";
    case DebugAvailability::kDegraded: return "Degraded";
    case DebugAvailability::kUnavailable: return "Unavailable";
    case DebugAvailability::kUnsupported: return "Unsupported";
    case DebugAvailability::kError: return "Error";
  }
  return "Error";
}
[[nodiscard]] inline bool hasReadback(DebugAvailability state) noexcept {
  return state == DebugAvailability::kAvailable || state == DebugAvailability::kDegraded;
}
[[nodiscard]] inline bool canSubmitProduct(const DebugPanelContext& context) noexcept {
  return context.controls.hasRuntimeOwner() &&
      context.snapshot.product.availability == DebugAvailability::kAvailable &&
      context.snapshot.platform.availability == DebugAvailability::kAvailable &&
      context.snapshot.coherence == SnapshotCoherence::kCoherent &&
      context.snapshot.stamp.runtimeGeneration != 0;
}
[[nodiscard]] inline bool canSubmitAxiom(const DebugPanelContext& context) noexcept {
  return context.controls.hasAxiomDebugOwner() &&
      hasReadback(context.snapshot.axiom.availability) &&
      context.snapshot.coherence == SnapshotCoherence::kCoherent &&
      context.snapshot.stamp.runtimeGeneration != 0 &&
      context.snapshot.stamp.documentGeneration != 0;
}
}  // namespace panel_detail
void registerDashboardDebugPanels(PanelRegistry& registry) noexcept;
void registerControlDebugPanels(PanelRegistry& registry) noexcept;
void registerInspectDebugPanels(PanelRegistry& registry) noexcept;
void registerRuntimeDebugPanels(PanelRegistry& registry) noexcept;
void registerPerformanceDebugPanels(PanelRegistry& registry) noexcept;
void registerScenarioDebugPanels(PanelRegistry& registry) noexcept;
void registerInkDebugPanels(PanelRegistry& registry) noexcept;
void registerBuiltinPanels(PanelRegistry& registry) noexcept;
}  // namespace canvas::debug_ui
