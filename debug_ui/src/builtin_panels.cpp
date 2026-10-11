#include "canvas/debug_ui/builtin_panels.hpp"

namespace canvas::debug_ui {
void registerBuiltinPanels(PanelRegistry& registry) noexcept {
  registerDashboardDebugPanels(registry);
  registerControlDebugPanels(registry);
  registerInkDebugPanels(registry);
  registerInspectDebugPanels(registry);
  registerRuntimeDebugPanels(registry);
  registerPerformanceDebugPanels(registry);
  registerScenarioDebugPanels(registry);
}
}  // namespace canvas::debug_ui
