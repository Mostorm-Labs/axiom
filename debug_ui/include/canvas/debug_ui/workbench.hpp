#pragma once

#include "canvas/debug_ui/panel_registry.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"

namespace canvas::debug_ui {
class DebugWorkbench final {
 public:
  [[nodiscard]] bool render(const DebugSnapshot&, DebugControlRouter&,
                            DebugUiSessionState&, const PanelRegistry&);
};
}  // namespace canvas::debug_ui
