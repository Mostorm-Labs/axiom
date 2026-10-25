#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "canvas/runtime/debug_control.hpp"
#include "imgui.h"

namespace canvas::debug_ui {
namespace {
void live(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Live Metrics");
    ImGui::Text("Telemetry: %s", panel_detail::availabilityLabel(s.telemetry.availability));
    ImGui::Text("sequence %llu / sample %.1f Hz / frame %.2f ms / queue %.2f ms",
                static_cast<unsigned long long>(s.telemetry.value.sequence),
                s.telemetry.value.sampleHz,
                s.telemetry.value.frameMs,
                s.telemetry.value.queueAgeMs);
    const bool enabled = panel_detail::canSubmitAxiom(c);
    ImGui::BeginDisabled(!enabled);
    if (ImGui::Button("Reset Rolling Metrics")) {
        (void)c.controls.submitAxiom(canvas::runtime::AxiomDebugCommandKind::kResetRollingMetrics);
        c.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    if (!enabled)
        ImGui::TextUnformatted("Reset Rolling Metrics: Unsupported (Axiom owner unavailable)");
}
void input(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Input / ARC");
    ImGui::Text("Telemetry: %s / ARC: %s",
                panel_detail::availabilityLabel(s.telemetry.availability),
                panel_detail::availabilityLabel(s.arc.availability));
    ImGui::Text("input events %llu / pointers %llu / batches %llu / handoffs %llu",
                static_cast<unsigned long long>(s.telemetry.value.inputEvents),
                static_cast<unsigned long long>(s.arc.value.activePointerCount),
                static_cast<unsigned long long>(s.arc.value.inputBatchCount),
                static_cast<unsigned long long>(s.arc.value.handoffCount));
}
void rendering(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Rendering");
    ImGui::Text("Telemetry: %s / Platform: %s",
                panel_detail::availabilityLabel(s.telemetry.availability),
                panel_detail::availabilityLabel(s.platform.availability));
    ImGui::Text("preview %llu / canonical %llu / presents %llu / lost %llu",
                static_cast<unsigned long long>(s.telemetry.value.previewFrames),
                static_cast<unsigned long long>(s.telemetry.value.canonicalFrames),
                static_cast<unsigned long long>(s.platform.value.presentCount),
                static_cast<unsigned long long>(s.platform.value.lostCount));
}
} // namespace
void registerPerformanceDebugPanels(PanelRegistry& r) noexcept {
    (void)r.add({WorkspaceId::kPerformance, PanelSlot::kLiveMetrics, 0, nullptr, live});
    (void)r.add({WorkspaceId::kPerformance, PanelSlot::kInputArc, 0, nullptr, input});
    (void)r.add({WorkspaceId::kPerformance, PanelSlot::kRendering, 0, nullptr, rendering});
}
} // namespace canvas::debug_ui
