#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "../widgets.hpp"
#include <array>

namespace canvas::debug_ui {
namespace {
void ink(DebugPanelContext& c) {
    const auto& state = c.snapshot.product.value;
    if (!c.ui.advancedControls() && (state.control.creationMode || state.selection.enabled ||
        c.controls.canvasTool() != canvas::runtime::CanvasToolKind::kInk)) return;
    ImGui::SeparatorText("Ink");
    struct Preset { const char* label; std::uint32_t id, revision; };
    constexpr std::array<Preset,4> presets{{{"Vector Solid",1,1},{"Marker Flat",2,1},{"Chalk Grain",3,4},{"Membrane",4,1}}};
    ImGui::BeginDisabled(!panel_detail::canSubmitProduct(c));
    bool first = true;
    for (const auto& preset : presets) {
        if (panel_detail::flowButton(preset.label, first,
            state.tool.brushId == preset.id && state.tool.brushRevision == preset.revision)) {
            (void)c.controls.setBrush(preset.id, preset.revision); c.ui.markControlSubmitted();
        }
    }
    ImGui::EndDisabled();
    if (c.ui.advancedControls()) ImGui::Text("resolved brushId: %u / brush revision: %u",
                                           state.tool.brushId, state.tool.brushRevision);
}
}
void registerInkDebugPanels(PanelRegistry& registry) noexcept {
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kInk, 0, nullptr, ink});
}
}
