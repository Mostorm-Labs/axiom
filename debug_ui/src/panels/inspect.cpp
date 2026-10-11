#include "canvas/debug_ui/builtin_panels.hpp"
#include "imgui.h"

namespace canvas::debug_ui {
namespace {
void selection(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Selection");
    ImGui::Text("Product: %s", panel_detail::availabilityLabel(s.product.availability));
    ImGui::Text("mode %s / count %u / primary %llu / snap candidates %llu",
                s.product.value.selection.enabled ? "enabled" : "disabled",
                s.product.value.selection.selectedObjectCount,
                static_cast<unsigned long long>(s.product.value.selection.primaryObject),
                static_cast<unsigned long long>(s.product.value.selection.snapCandidateCount));
}
void object(DebugPanelContext&) {
    ImGui::SeparatorText("Object");
    ImGui::TextUnformatted("Unsupported / Not implemented");
}
void interaction(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Interaction");
    ImGui::Text("ARC: %s / Axiom: %s",
                panel_detail::availabilityLabel(s.arc.availability),
                panel_detail::availabilityLabel(s.axiom.availability));
    ImGui::Text("ARC pointers %llu / batches %llu / handoffs %llu / preview %s",
                static_cast<unsigned long long>(s.arc.value.activePointerCount),
                static_cast<unsigned long long>(s.arc.value.inputBatchCount),
                static_cast<unsigned long long>(s.arc.value.handoffCount),
                s.arc.value.previewActive ? "active" : "idle");
    ImGui::Text("Axiom overlay updates %llu / transient transforms %llu",
                static_cast<unsigned long long>(s.axiom.value.interaction.overlayUpdateCount),
                static_cast<unsigned long long>(s.axiom.value.interaction.transientTransformCount));
}
void relationship(DebugPanelContext&) {
    ImGui::SeparatorText("Relationship");
    ImGui::TextUnformatted("Unsupported / Not implemented");
}
} // namespace
void registerInspectDebugPanels(PanelRegistry& r) noexcept {
    (void)r.add({WorkspaceId::kInspect, PanelSlot::kSelection, 0, nullptr, selection});
    (void)r.add({WorkspaceId::kInspect, PanelSlot::kObject, 0, nullptr, object});
    (void)r.add({WorkspaceId::kInspect, PanelSlot::kInteraction, 0, nullptr, interaction});
    (void)r.add({WorkspaceId::kInspect, PanelSlot::kRelationship, 0, nullptr, relationship});
}
} // namespace canvas::debug_ui
