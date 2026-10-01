#include "canvas/debug_ui/controller.hpp"
namespace canvas::debug_ui {
std::array<PanelState, static_cast<std::size_t>(DebugPanel::kCount)> DebugController::panels() const {
    return {{{DebugPanel::kOverview, true, "Overview"}, {DebugPanel::kInput, true, "Input"},
             {DebugPanel::kCanvas, true, "Canvas"}, {DebugPanel::kArcPreview, true, "Arc Preview"},
             {DebugPanel::kSurface, true, "Surface"}, {DebugPanel::kBrush, true, "Brush"},
             {DebugPanel::kTelemetry, true, "Telemetry"}, {DebugPanel::kInspection, false, "Inspection (Unavailable)"}}};
}
std::optional<CommandReceipt> DebugController::submit(DebugCommand command) {
    return context_.commands == nullptr ? std::nullopt : context_.commands->admit(std::move(command));
}
}  // namespace canvas::debug_ui
