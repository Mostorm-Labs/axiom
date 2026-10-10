#pragma once

#include "canvas/debug_ui/controller.hpp"

#include <array>
#include <cstddef>

namespace canvas::debug_ui {

struct DebugPanelModel final {
  [[nodiscard]] static std::array<PanelState,
      static_cast<std::size_t>(DebugPanel::kCount)> describe(
          const DebugSnapshot& snapshot) noexcept;
  [[nodiscard]] static bool canSubmitProductControl(
      const DebugSnapshot& snapshot) noexcept;
};

}  // namespace canvas::debug_ui
