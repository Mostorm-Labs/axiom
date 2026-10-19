#pragma once
#include "canvas/debug_ui/snapshot.hpp"
namespace canvas::debug_ui {
struct DebugPanelModel final {
  [[nodiscard]] static bool canSubmitProductControl(const DebugSnapshot&) noexcept;
};
}
