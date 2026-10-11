#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "canvas/runtime/debug_control.hpp"
#include "imgui.h"

namespace canvas::debug_ui {
namespace {
void document(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Document");
    ImGui::Text("Product: %s / Axiom: %s",
                panel_detail::availabilityLabel(s.product.availability),
                panel_detail::availabilityLabel(s.axiom.availability));
    ImGui::Text("generation %llu / revision %llu / operations %llu",
                static_cast<unsigned long long>(s.stamp.documentGeneration),
                static_cast<unsigned long long>(s.stamp.documentRevision),
                static_cast<unsigned long long>(s.axiom.value.document.canonicalOperationCount));
}
void scene(DebugPanelContext&) {
    ImGui::SeparatorText("Scene");
    ImGui::TextUnformatted("Unsupported / Not implemented");
}
void arc(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("ARC");
    ImGui::Text("ARC: %s / preview: %s",
                panel_detail::availabilityLabel(s.arc.availability),
                s.arc.value.previewActive ? "active" : "idle");
    ImGui::Text("preview revision %llu / pointers %llu / batches %llu / handoffs %llu",
                static_cast<unsigned long long>(s.arc.value.previewRevision),
                static_cast<unsigned long long>(s.arc.value.activePointerCount),
                static_cast<unsigned long long>(s.arc.value.inputBatchCount),
                static_cast<unsigned long long>(s.arc.value.handoffCount));
}
void render(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Render");
    ImGui::Text("Telemetry: %s / Platform: %s",
                panel_detail::availabilityLabel(s.telemetry.availability),
                panel_detail::availabilityLabel(s.platform.availability));
    ImGui::Text("frame %llu / presents %llu / lost %llu",
                static_cast<unsigned long long>(s.stamp.frameId),
                static_cast<unsigned long long>(s.platform.value.presentCount),
                static_cast<unsigned long long>(s.platform.value.lostCount));
    ImGui::Text("preview frames %llu / canonical frames %llu",
                static_cast<unsigned long long>(s.telemetry.value.previewFrames),
                static_cast<unsigned long long>(s.telemetry.value.canonicalFrames));
}
void surface(DebugPanelContext& c) {
    const auto& s = c.snapshot;
    ImGui::SeparatorText("Surface");
    ImGui::Text("Platform: %s", panel_detail::availabilityLabel(s.platform.availability));
    const auto mode =
        s.platform.value.canonicalSurfaceMode == canvas::runtime::SurfaceMode::kCpuReference
            ? "CPU Reference"
        : s.platform.value.canonicalSurfaceMode == canvas::runtime::SurfaceMode::kGpuDefault
            ? "GPU Default"
            : "Platform Default";
    ImGui::Text("resolved %s / canonical generation %llu / preview generation %llu",
                mode,
                static_cast<unsigned long long>(s.platform.value.canonicalSurfaceGeneration),
                static_cast<unsigned long long>(s.platform.value.previewSurfaceGeneration));
    ImGui::Text("%ux%u / DPR %.2f / available %s / presents %llu / lost %llu",
                s.platform.value.width,
                s.platform.value.height,
                s.platform.value.devicePixelRatio,
                s.platform.value.surfaceAvailable ? "yes" : "no",
                static_cast<unsigned long long>(s.platform.value.presentCount),
                static_cast<unsigned long long>(s.platform.value.lostCount));
    const bool enabled = c.controls.hasPlatformDebugOwner() &&
                         panel_detail::hasReadback(s.platform.availability) &&
                         s.platform.value.canonicalSurfaceGeneration != 0;
    ImGui::BeginDisabled(!enabled);
    if (ImGui::Button("Platform Default")) {
        (void)c.controls.setCanonicalSurfaceMode(canvas::runtime::SurfaceMode::kPlatformDefault);
        c.ui.markControlSubmitted();
    }
    ImGui::SameLine();
    if (ImGui::Button("CPU Reference")) {
        (void)c.controls.setCanonicalSurfaceMode(canvas::runtime::SurfaceMode::kCpuReference);
        c.ui.markControlSubmitted();
    }
    ImGui::SameLine();
    if (ImGui::Button("GPU Default")) {
        (void)c.controls.setCanonicalSurfaceMode(canvas::runtime::SurfaceMode::kGpuDefault);
        c.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    if (!enabled)
        ImGui::TextUnformatted(
            "Surface controls: Unsupported (Platform owner or generation unavailable)");
    if (s.activity.surfaceControl.has_value()) {
        const auto& receipt = *s.activity.surfaceControl;
        const char* state = "Failed";
        switch (receipt.state) {
        case canvas::runtime::SurfaceControlState::kQueued:
            state = "Queued";
            break;
        case canvas::runtime::SurfaceControlState::kApplied:
            state = "Applied";
            break;
        case canvas::runtime::SurfaceControlState::kUnsupported:
            state = "Unsupported";
            break;
        case canvas::runtime::SurfaceControlState::kStaleGeneration:
            state = "Stale generation";
            break;
        case canvas::runtime::SurfaceControlState::kQueueFull:
            state = "Queue full";
            break;
        case canvas::runtime::SurfaceControlState::kExpired:
            state = "Expired";
            break;
        case canvas::runtime::SurfaceControlState::kUnavailable:
            state = "Unavailable";
            break;
        case canvas::runtime::SurfaceControlState::kFailed:
            break;
        }
        ImGui::Text("surface request %llu: %s / generation %llu",
                    static_cast<unsigned long long>(receipt.requestId),
                    state,
                    static_cast<unsigned long long>(receipt.generation));
    } else
        ImGui::TextUnformatted("No Surface receipt");
}
void resources(DebugPanelContext&) {
    ImGui::SeparatorText("Resources");
    ImGui::TextUnformatted("Unsupported / Not implemented");
}
void diagnostics(DebugPanelContext&) {
    ImGui::SeparatorText("Feature Diagnostics");
    ImGui::TextUnformatted("Unsupported / Not implemented");
}
void advanced(DebugPanelContext& c) {
    ImGui::SeparatorText("Advanced Actions");
    const bool enabled = panel_detail::canSubmitAxiom(c);
    ImGui::BeginDisabled(!enabled);
    if (ImGui::Button("Force Full Redraw")) {
        (void)c.controls.submitAxiom(canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw);
        c.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    if (!enabled)
        ImGui::TextUnformatted("Force Full Redraw: Unsupported (Axiom owner unavailable)");
    ImGui::TextUnformatted("Set Overlay Flags: Unsupported");
    ImGui::TextUnformatted("Force Scene Recompile: Unsupported");
    ImGui::TextUnformatted("Evict Tile Cache: Unsupported");
    ImGui::TextUnformatted("Evict Raster Cache: Unsupported");
    ImGui::TextUnformatted("Pause Background Raster: Unsupported");
    ImGui::TextUnformatted("Runtime Memory Budget: Unsupported");
}
} // namespace
void registerRuntimeDebugPanels(PanelRegistry& r) noexcept {
    (void)r.add({WorkspaceId::kRuntime, PanelSlot::kDocument, 0, nullptr, document});
    (void)r.add({WorkspaceId::kRuntime, PanelSlot::kScene, 0, nullptr, scene});
    (void)r.add({WorkspaceId::kRuntime, PanelSlot::kArc, 0, nullptr, arc});
    (void)r.add({WorkspaceId::kRuntime, PanelSlot::kRender, 0, nullptr, render});
    (void)r.add({WorkspaceId::kRuntime, PanelSlot::kSurface, 0, nullptr, surface});
    (void)r.add({WorkspaceId::kRuntime, PanelSlot::kResources, 0, nullptr, resources});
    (void)r.add({WorkspaceId::kRuntime, PanelSlot::kFeatureDiagnostics, 0, nullptr, diagnostics});
    (void)r.add({WorkspaceId::kRuntime, PanelSlot::kAdvancedActions, 0, nullptr, advanced});
}
} // namespace canvas::debug_ui
