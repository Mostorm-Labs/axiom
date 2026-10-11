#pragma once

#include "canvas/debug_ui/panel.hpp"

#include <array>
#include <span>
#include <vector>

namespace canvas::debug_ui {

class PanelRegistry final {
 public:
  [[nodiscard]] bool add(PanelContribution contribution) noexcept;
  [[nodiscard]] bool registerPanel(PanelContribution contribution) noexcept { return add(contribution); }
  [[nodiscard]] std::span<const PanelContribution> contributions(WorkspaceId workspace) const noexcept;

 private:
  std::array<std::vector<PanelContribution>, static_cast<std::size_t>(WorkspaceId::kCount)> panels_{};
};

}  // namespace canvas::debug_ui
