#include "canvas/debug_ui/builtin_panels.hpp"
#include "imgui.h"

namespace canvas::debug_ui {
namespace {
void identity(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Runtime Identity");
    ImGui::Text("frame %llu", static_cast<unsigned long long>(s.stamp.frameId));
    ImGui::Text("runtime %llu / document %llu / revision %llu",
                static_cast<unsigned long long>(s.stamp.runtimeGeneration),
                static_cast<unsigned long long>(s.stamp.documentGeneration),
                static_cast<unsigned long long>(s.stamp.documentRevision));
    ImGui::Text("view %llu / surface %llu / snapshot #%llu",
                static_cast<unsigned long long>(s.stamp.viewGeneration),
                static_cast<unsigned long long>(s.stamp.surfaceGeneration),
                static_cast<unsigned long long>(s.stamp.snapshotSequence));
    ImGui::Text("coherence: %s",
                s.coherence == SnapshotCoherence::kCoherent          ? "Coherent"
                : s.coherence == SnapshotCoherence::kMixedGeneration ? "Mixed generation"
                                                                     : "Stale");
}
void product(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Current Product State");
    if (!panel_detail::hasReadback(s.product.availability)) {
        ImGui::Text("Product: %s", panel_detail::availabilityLabel(s.product.availability));
        return;
    }
    ImGui::Text("tool %u / brush %u rev %u / eraser %u",
                s.product.value.tool.toolId,
                s.product.value.tool.brushId,
                s.product.value.tool.brushRevision,
                s.product.value.tool.eraserId);
    ImGui::Text("selection %s / objects %u / primary %llu",
                s.product.value.selection.enabled ? "enabled" : "disabled",
                s.product.value.selection.selectedObjectCount,
                static_cast<unsigned long long>(s.product.value.selection.primaryObject));
    ImGui::Text("camera %.3f at %.3f, %.3f / undo %s / redo %s",
                s.product.value.camera.scale,
                s.product.value.camera.translationX,
                s.product.value.camera.translationY,
                s.product.value.history.canUndo ? "yes" : "no",
                s.product.value.history.canRedo ? "yes" : "no");
}
void health(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Health");
    ImGui::Text("Product %s / Axiom %s / ARC %s / Platform %s / Telemetry %s",
                panel_detail::availabilityLabel(s.product.availability),
                panel_detail::availabilityLabel(s.axiom.availability),
                panel_detail::availabilityLabel(s.arc.availability),
                panel_detail::availabilityLabel(s.platform.availability),
                panel_detail::availabilityLabel(s.telemetry.availability));
}
void performance(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Live Performance");
    ImGui::Text("Telemetry: %s", panel_detail::availabilityLabel(s.telemetry.availability));
    ImGui::Text("sample %.1f Hz / frame %.2f ms / queue %.2f ms",
                s.telemetry.value.sampleHz,
                s.telemetry.value.frameMs,
                s.telemetry.value.queueAgeMs);
    ImGui::Text("input %llu / preview %llu / canonical %llu",
                static_cast<unsigned long long>(s.telemetry.value.inputEvents),
                static_cast<unsigned long long>(s.telemetry.value.previewFrames),
                static_cast<unsigned long long>(s.telemetry.value.canonicalFrames));
}
void activity(DebugPanelContext& c) {
    ImGui::SeparatorText("Recent Activity");
    if (c.snapshot.activity.entries.empty()) {
        ImGui::TextUnformatted("No control activity");
        return;
    }
    for (const auto& e : c.snapshot.activity.entries) {
        const char* state = "Failed";
        switch (e.state) {
        case DebugActivityState::kPending:
            state = "Pending";
            break;
        case DebugActivityState::kApplied:
            state = "Applied";
            break;
        case DebugActivityState::kRejected:
            state = "Rejected";
            break;
        case DebugActivityState::kUnsupported:
            state = "Unsupported";
            break;
        case DebugActivityState::kUnavailable:
            state = "Unavailable";
            break;
        case DebugActivityState::kStaleGeneration:
            state = "Stale generation";
            break;
        case DebugActivityState::kExpired:
            state = "Expired";
            break;
        case DebugActivityState::kQueueFull:
            state = "Queue full";
            break;
        case DebugActivityState::kFailed:
            break;
        }
        ImGui::Text("#%llu request %llu / %s / %s",
                    static_cast<unsigned long long>(e.sequence),
                    static_cast<unsigned long long>(e.requestId),
                    e.action.c_str(),
                    state);
    }
}
} // namespace
void registerDashboardDebugPanels(PanelRegistry& r) noexcept {
    (void)r.add({WorkspaceId::kDashboard, PanelSlot::kRuntimeIdentity, 0, nullptr, identity});
    (void)r.add({WorkspaceId::kDashboard, PanelSlot::kCurrentProductState, 0, nullptr, product});
    (void)r.add({WorkspaceId::kDashboard, PanelSlot::kHealth, 0, nullptr, health});
    (void)r.add({WorkspaceId::kDashboard, PanelSlot::kLivePerformance, 0, nullptr, performance});
    (void)r.add({WorkspaceId::kDashboard, PanelSlot::kRecentActivity, 0, nullptr, activity});
}
} // namespace canvas::debug_ui
