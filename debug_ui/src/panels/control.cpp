#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "imgui.h"

#include <cmath>

namespace canvas::debug_ui {
namespace {
void tools(DebugPanelContext& context) {
    const auto& snapshot = context.snapshot;
    ImGui::SeparatorText("Tools");
    ImGui::Text("Product: %s", panel_detail::availabilityLabel(snapshot.product.availability));
    if (!context.controls.hasRuntimeOwner())
        ImGui::TextUnformatted("Tool controls: Unsupported (Runtime owner unbound)");
    bool selectionMode = snapshot.product.value.selection.enabled;
    ImGui::BeginDisabled(!panel_detail::canSubmitProduct(context));
    if (ImGui::Checkbox("Selection mode", &selectionMode)) {
        (void)context.controls.setSelectionMode(selectionMode);
        context.ui.markControlSubmitted();
    }
    const auto eraserId = snapshot.product.value.tool.eraserId;
    if (ImGui::Selectable("Object Eraser", eraserId == 1U)) {
        (void)context.controls.setEraser(1U);
        context.ui.markControlSubmitted();
    }
    if (ImGui::Selectable("Partial Eraser", eraserId == 2U)) {
        (void)context.controls.setEraser(2U);
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    if (panel_detail::hasReadback(snapshot.product.availability)) {
        ImGui::Text("resolved eraserId: %u / selection: %s",
                    eraserId,
                    snapshot.product.value.selection.enabled ? "enabled" : "disabled");
        ImGui::Text("legacy toolId (diagnostic): %u", snapshot.product.value.tool.toolId);
    }
}
void view(DebugPanelContext& context) {
    const auto& snapshot = context.snapshot;
    ImGui::SeparatorText("View");
    if (panel_detail::hasReadback(snapshot.product.availability)) {
        const auto& camera = snapshot.product.value.camera;
        ImGui::Text("camera scale %.3f / translation %.3f, %.3f",
                    camera.scale,
                    camera.translationX,
                    camera.translationY);
        ImGui::Text("view generation %llu",
                    static_cast<unsigned long long>(snapshot.stamp.viewGeneration));
    } else
        ImGui::Text("Camera: %s", panel_detail::availabilityLabel(snapshot.product.availability));
    if (panel_detail::hasReadback(snapshot.axiom.availability))
        ImGui::Text("camera generation %llu",
                    static_cast<unsigned long long>(snapshot.axiom.value.camera.generation));
    else
        ImGui::Text("Camera generation: %s",
                    panel_detail::availabilityLabel(snapshot.axiom.availability));
    auto& draft = context.ui.cameraDraft();
    ImGui::TextUnformatted("Camera input drafts (UI-local)");
    ImGui::InputFloat("Pan delta X", &draft.panDeltaX);
    ImGui::InputFloat("Pan delta Y", &draft.panDeltaY);
    const bool available = panel_detail::canSubmitProduct(context);
    ImGui::BeginDisabled(!available || !std::isfinite(draft.panDeltaX) ||
                         !std::isfinite(draft.panDeltaY));
    if (ImGui::Button("Pan by")) {
        (void)context.controls.panBy(draft.panDeltaX, draft.panDeltaY);
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::InputFloat("Zoom anchor X", &draft.zoomAnchorX);
    ImGui::InputFloat("Zoom anchor Y", &draft.zoomAnchorY);
    ImGui::InputFloat("Zoom scale delta", &draft.zoomScaleDelta);
    ImGui::BeginDisabled(!available || !std::isfinite(draft.zoomAnchorX) ||
                         !std::isfinite(draft.zoomAnchorY) ||
                         !std::isfinite(draft.zoomScaleDelta) || draft.zoomScaleDelta <= 0.0F);
    if (ImGui::Button("Zoom At")) {
        (void)context.controls.zoomAt(draft.zoomAnchorX, draft.zoomAnchorY, draft.zoomScaleDelta);
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available);
    if (ImGui::Button("Fit Content")) {
        (void)context.controls.fitToContent();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available || snapshot.product.value.selection.selectedObjectCount == 0);
    if (ImGui::Button("Fit Selection")) {
        (void)context.controls.fitToSelection();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available || snapshot.product.value.selection.primaryObject == 0);
    if (ImGui::Button("Fit Primary Object")) {
        (void)context.controls.fitPrimaryObject();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    if (!context.controls.hasRuntimeOwner())
        ImGui::TextUnformatted("View controls: Unsupported (Runtime owner unbound)");
}
void history(DebugPanelContext& context) {
    const auto& snapshot = context.snapshot;
    ImGui::SeparatorText("History");
    ImGui::Text("History: %s", panel_detail::availabilityLabel(snapshot.product.availability));
    const bool available = panel_detail::canSubmitProduct(context);
    ImGui::BeginDisabled(!available || !snapshot.product.value.history.canUndo);
    if (ImGui::Button("Undo (Ctrl+Z)")) {
        (void)context.controls.undo();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available || !snapshot.product.value.history.canRedo);
    if (ImGui::Button("Redo (Ctrl+Y)")) {
        (void)context.controls.redo();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
}
void feature(DebugPanelContext&) {
    ImGui::SeparatorText("Feature");
    ImGui::TextUnformatted("Unsupported / Not implemented (successor feature slot)");
}
} // namespace
void registerControlDebugPanels(PanelRegistry& registry) noexcept {
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kTools, 0, nullptr, tools});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kView, 0, nullptr, view});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kHistory, 0, nullptr, history});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kFeature, 0, nullptr, feature});
}
} // namespace canvas::debug_ui
