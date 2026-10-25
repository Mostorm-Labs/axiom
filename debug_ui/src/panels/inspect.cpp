#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "imgui.h"

#include <cstddef>
#include <cstdint>

namespace canvas::debug_ui {
namespace {
std::uint64_t lowObjectId(const canvas::foundation::ObjectId& id) noexcept {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8U; ++i)
        value |= static_cast<std::uint64_t>(id.bytes[i]) << (8U * i);
    return value;
}
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
void object(DebugPanelContext& c) {
    ImGui::SeparatorText("Object");
    const auto& s = c.snapshot;
    if (!panel_detail::hasReadback(s.product.availability) ||
        s.product.value.selection.selectedObjectCount == 0) {
        ImGui::TextUnformatted("No selected object readback");
        return;
    }
    const auto& object = s.product.value.control;
    ImGui::Text("kind %u/%u shape %u / locked %s / visible %s",
                object.selectedKind, object.selectedVersion, object.selectedShapeKind,
                object.selectedLocked ? "yes" : "no", object.selectedVisible ? "yes" : "no");
    if (object.selectedSizeSupported)
        ImGui::Text("size %.2f x %.2f", object.selectedWidth, object.selectedHeight);
    ImGui::Text("transform [%.2f %.2f %.2f %.2f %.2f %.2f]",
                object.selectedTransform[0], object.selectedTransform[1],
                object.selectedTransform[2], object.selectedTransform[3],
                object.selectedTransform[4], object.selectedTransform[5]);
    const bool enabled = panel_detail::canSubmitProduct(c);
    ImGui::BeginDisabled(!enabled);
    if (ImGui::Button(object.selectedLocked ? "Unlock" : "Lock")) {
        (void)c.controls.selectedAction(canvas::runtime::CanvasControlPayloadKind::kSelectedLock,
                                        !object.selectedLocked);
        c.ui.markControlSubmitted();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete selected")) {
        (void)c.controls.selectedAction(canvas::runtime::CanvasControlPayloadKind::kDeleteSelected);
        c.ui.markControlSubmitted();
    }
    ImGui::SameLine();
    if (ImGui::Button("Bring front")) {
        (void)c.controls.selectedAction(canvas::runtime::CanvasControlPayloadKind::kSelectedOrder, true);
        c.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::TextUnformatted("Advanced text/copy/duplicate controls remain unsupported by FC-01");
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
void relationship(DebugPanelContext& c) {
    ImGui::SeparatorText("Relationship");
    const auto sticky = c.controls.selectedSticky();
    const auto connector = c.controls.selectedConnector();
    if (sticky.available) {
        ImGui::Text("Sticky %llu / %.0f x %.0f",
                    static_cast<unsigned long long>(lowObjectId(sticky.objectId)),
                    sticky.content.width, sticky.content.height);
    } else if (connector.available) {
        const auto endpointName = [](const canvas::semantic::ConnectorEndpoint& endpoint) {
            if (std::holds_alternative<canvas::semantic::FreePointEndpoint>(endpoint.value))
                return "free";
            return "attached";
        };
        ImGui::Text("Connector %llu / start %s / end %s",
                    static_cast<unsigned long long>(lowObjectId(connector.objectId)),
                    endpointName(connector.content.start), endpointName(connector.content.end));
        ImGui::TextUnformatted("Auto perimeter snap points: top / right / bottom / left");
    } else {
        ImGui::TextUnformatted("Select a Sticky or Connector to inspect attachment state");
    }
}
} // namespace
void registerInspectDebugPanels(PanelRegistry& r) noexcept {
    (void)r.add({WorkspaceId::kInspect, PanelSlot::kSelection, 0, nullptr, selection});
    (void)r.add({WorkspaceId::kInspect, PanelSlot::kObject, 0, nullptr, object});
    (void)r.add({WorkspaceId::kInspect, PanelSlot::kInteraction, 0, nullptr, interaction});
    (void)r.add({WorkspaceId::kInspect, PanelSlot::kRelationship, 0, nullptr, relationship});
}
} // namespace canvas::debug_ui
