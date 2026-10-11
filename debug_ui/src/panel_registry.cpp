#include "canvas/debug_ui/panel_registry.hpp"

#include <algorithm>

namespace canvas::debug_ui {

bool PanelRegistry::add(PanelContribution contribution) noexcept {
  if (!isValidWorkspace(contribution.workspace) || contribution.render == nullptr ||
      !panelSlotBelongsTo(contribution.workspace, contribution.slot)) return false;
  try {
    auto& panels = panels_[static_cast<std::size_t>(contribution.workspace)];
    panels.push_back(contribution);
    std::stable_sort(panels.begin(), panels.end(), [](const auto& left, const auto& right) {
      const auto leftSlot = static_cast<std::uint8_t>(left.slot);
      const auto rightSlot = static_cast<std::uint8_t>(right.slot);
      return leftSlot == rightSlot ? left.order < right.order : leftSlot < rightSlot;
    });
    return true;
  } catch (...) {
    return false;
  }
}

std::span<const PanelContribution> PanelRegistry::contributions(WorkspaceId workspace) const noexcept {
  if (!isValidWorkspace(workspace)) return {};
  const auto& panels = panels_[static_cast<std::size_t>(workspace)];
  return {panels.data(), panels.size()};
}

}  // namespace canvas::debug_ui
