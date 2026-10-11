#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "imgui.h"

#include <array>

namespace canvas::debug_ui {
namespace {
void ink(DebugPanelContext& context) {
    ImGui::SeparatorText("Ink");
    const auto& snapshot = context.snapshot;
    struct Preset {
        const char* label;
        std::uint32_t id;
        std::uint32_t revision;
    };
    constexpr std::array<Preset, 4> presets{{{"Vector Solid", 1U, 1U},
                                             {"Marker Flat", 2U, 1U},
                                             {"Chalk Grain", 3U, 4U},
                                             {"Membrane", 4U, 1U}}};
    if (panel_detail::hasReadback(snapshot.product.availability)) {
        ImGui::Text("resolved brushId: %u / brush revision: %u",
                    snapshot.product.value.tool.brushId,
                    snapshot.product.value.tool.brushRevision);
    } else
        ImGui::Text("Brush readback: %s",
                    panel_detail::availabilityLabel(snapshot.product.availability));
    if (!context.controls.hasRuntimeOwner())
        ImGui::TextUnformatted("Brush controls: Unsupported (Runtime owner unbound)");
    ImGui::BeginDisabled(!panel_detail::canSubmitProduct(context));
    for (const auto& preset : presets) {
        const bool selected = snapshot.product.value.tool.brushId == preset.id &&
                              snapshot.product.value.tool.brushRevision == preset.revision;
        if (ImGui::Selectable(preset.label, selected)) {
            (void)context.controls.setBrush(preset.id, preset.revision);
            context.ui.markControlSubmitted();
        }
    }
    ImGui::EndDisabled();
    ImGui::TextUnformatted("Size: Unsupported");
    ImGui::TextUnformatted("Opacity: Unsupported");
    ImGui::TextUnformatted("Other brush parameters: Unsupported");
}
} // namespace
void registerInkDebugPanels(PanelRegistry& registry) noexcept {
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kInk, 0, nullptr, ink});
}
} // namespace canvas::debug_ui
