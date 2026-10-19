#include "canvas/debug_ui/panel_registry.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include <array>
#include <cassert>
#include <type_traits>

namespace {
void first(canvas::debug_ui::DebugPanelContext&) {}
void second(canvas::debug_ui::DebugPanelContext&) {}
bool visible(const canvas::debug_ui::DebugSnapshot&) noexcept { return true; }
}
int main() {
  using namespace canvas::debug_ui;
  static_assert(static_cast<int>(WorkspaceId::kCount) == 6);
  constexpr std::array expected{"Dashboard", "Control", "Inspect", "Runtime", "Performance", "Scenarios"};
  constexpr std::array slots{PanelSlot::kHealth, PanelSlot::kFeature, PanelSlot::kObject,
                            PanelSlot::kSurface, PanelSlot::kFeatureMetrics, PanelSlot::kEvidence};
  PanelRegistry registry;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const auto workspace = static_cast<WorkspaceId>(i);
    assert(workspaceLabel(workspace) == expected[i]);
    assert(registry.add({workspace, slots[i], 0, nullptr, first}));
    assert(registry.contributions(workspace).size() == 1);
    for (std::size_t j = 0; j < slots.size(); ++j) {
      if (i != j) assert(!registry.add({workspace, slots[j], 0, nullptr, first}));
    }
  }
  assert(!registry.add({WorkspaceId::kCount, PanelSlot::kTools, 0, nullptr, first}));
  assert(!registry.add({WorkspaceId::kControl, static_cast<PanelSlot>(255), 0, nullptr, first}));
  assert(!registry.add({WorkspaceId::kControl, PanelSlot::kTools, 0, nullptr, nullptr}));
  assert(registry.contributions(WorkspaceId::kCount).empty());
  assert(registry.add({WorkspaceId::kControl, PanelSlot::kTools, 20, visible, first}));
  assert(registry.add({WorkspaceId::kControl, PanelSlot::kTools, -1, nullptr, first}));
  assert(registry.add({WorkspaceId::kControl, PanelSlot::kTools, -1, nullptr, second}));
  const auto controls = registry.contributions(WorkspaceId::kControl);
  assert(controls.size() == 4);
  assert(controls[0].order == -1 && controls[0].render == first);
  assert(controls[1].order == -1 && controls[1].render == second);
  assert(controls[2].order == 20 && controls[2].visible == visible);
  assert(controls[3].slot == PanelSlot::kFeature);
  DebugSnapshot snapshot{};
  DebugActivityLog activity;
  DebugControlRouter router(nullptr, nullptr, nullptr, &activity);
  DebugUiSessionState ui;
  DebugPanelContext context{snapshot, router, ui};
  static_assert(std::is_same_v<decltype(context.snapshot), const DebugSnapshot&>);
  static_assert(std::is_same_v<decltype(context.controls), DebugControlRouter&>);
  static_assert(std::is_same_v<decltype(context.ui), DebugUiSessionState&>);
  assert(&context.snapshot == &snapshot);
}
