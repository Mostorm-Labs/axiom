#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/controller.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace canvas::debug_ui;
namespace {
struct Providers final : canvas::runtime::RuntimeFacade,
                         canvas::runtime::IAxiomDiagnostics,
                         canvas::runtime::IArcDiagnostics,
                         canvas::runtime::IPlatformDiagnostics,
                         canvas::runtime::ITelemetry,
                         canvas::runtime::AxiomDebugControl {
    mutable std::vector<int> reads;
    int mutations = 0;
    canvas::runtime::ProductControlRequest lastProduct{};
    canvas::runtime::AxiomDebugCommand lastAxiom{};
    canvas::runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override {
        reads.push_back(1);
        canvas::runtime::RuntimeStateSnapshot state{};
        state.identity = {11, 12, 13, 14, 15};
        state.tool.toolId = 4101;
        return state;
    }
    canvas::runtime::AxiomDiagnosticsSnapshot readDiagnostics() const noexcept override {
        reads.push_back(2);
        return {};
    }
    canvas::runtime::ArcDiagnosticsSnapshot readArcDiagnostics() const noexcept override {
        reads.push_back(3);
        return {};
    }
    canvas::runtime::PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept override {
        reads.push_back(4);
        canvas::runtime::PlatformDiagnosticsSnapshot state{};
        state.surfaceAvailable = true;
        state.canonicalSurfaceGeneration = 15;
        return state;
    }
    canvas::runtime::TelemetrySnapshot readTelemetry() const noexcept override {
        reads.push_back(5);
        canvas::runtime::TelemetrySnapshot state{};
        state.sequence = 40;
        state.canonicalFrames = 8;
        return state;
    }
    canvas::runtime::ProductControlReceipt
    submitProductControl(const canvas::runtime::ProductControlRequest& request) noexcept override {
        ++mutations;
        lastProduct = request;
        return {
            request.requestId, canvas::runtime::ProductControlState::kApplied, 11, std::nullopt};
    }
    canvas::runtime::AxiomDebugCommandReceipt
    enqueue(const canvas::runtime::AxiomDebugCommand& command) noexcept override {
        ++mutations;
        lastAxiom = command;
        return {command.requestId, canvas::runtime::AxiomDebugCommandState::kQueued};
    }
    canvas::runtime::AxiomDebugCommandReceipt receipt(std::uint64_t id) const noexcept override {
        return {id, canvas::runtime::AxiomDebugCommandState::kApplied};
    }
};
struct SurfaceProbe final : canvas::runtime::PlatformDebugControl {
    int calls = 0;
    canvas::runtime::SurfaceModeRequest last{};
    canvas::runtime::SurfaceModeReceipt
    requestSurfaceMode(const canvas::runtime::SurfaceModeRequest& request) noexcept override {
        ++calls;
        last = request;
        return {request.requestId,
                canvas::runtime::SurfaceControlState::kQueued,
                request.target,
                request.mode,
                request.expectedGeneration};
    }
    canvas::runtime::SurfaceModeReceipt receipt(std::uint64_t id) const noexcept override {
        return {id, canvas::runtime::SurfaceControlState::kApplied, last.target, last.mode, 16};
    }
};
void startFrame() {
    ImGui::NewFrame();
    ImGui::Begin("Log capture");
    ImGui::LogToBuffer();
}
std::string finishFrame() {
    const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    return text;
}
int visibleRenders = 0;
void feature(DebugPanelContext& context) {
    ++visibleRenders;
    ImGui::TextUnformatted("Synthetic feature panel");
    (void)context.controls.undo();
    context.ui.markControlSubmitted();
}
bool hidden(const DebugSnapshot&) noexcept {
    return false;
}
void mustNotRender(DebugPanelContext&) {
    assert(false);
}

// Activates real Dear ImGui widgets; panel callbacks and Router are production code.
std::string renderPanel(const PanelRegistry& registry,
                        WorkspaceId workspace,
                        PanelSlot slot,
                        const DebugSnapshot& snapshot,
                        DebugControlRouter& router,
                        DebugUiSessionState& ui,
                        const char* activate = nullptr) {
    router.beginFrame(snapshot);
    ui.beginFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(900, 1600), ImGuiCond_Always);
    ImGui::Begin("Panel oracle");
    ImGui::LogToBuffer();
    auto& g = *ImGui::GetCurrentContext();
    if (activate != nullptr) {
        g.NavActivateId = g.NavActivateDownId = ImGui::GetID(activate);
        g.NavInputSource = ImGuiInputSource_Keyboard;
    }
    DebugPanelContext context{snapshot, router, ui};
    bool found = false;
    for (const auto& contribution : registry.contributions(workspace)) {
        if (contribution.slot == slot) {
            found = true;
            contribution.render(context);
        }
    }
    assert(found);
    const std::string text = g.LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    g.NavActivateId = g.NavActivateDownId = 0;
    ImGui::ClearActiveID();
    return text;
}
} // namespace

int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(900, 640);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
    Providers providers;
    DebugController controller(
        {&providers, &providers, &providers, &providers, &providers, &providers, nullptr});
    assert(controller.uiState().workspace() == WorkspaceId::kDashboard);
    for (int i = 0; i < 6; ++i) {
        controller.uiState().setWorkspace(static_cast<WorkspaceId>(i));
        startFrame();
        assert(!controller.buildImGuiFrame());
        const auto text = finishFrame();
        for (const auto label : kWorkspaceLabels)
            assert(text.find(label) != std::string::npos);
        assert(text.find("runtime 11") != std::string::npos);
        assert(text.find("document 12") != std::string::npos);
        assert(text.find("Coherent") != std::string::npos);
        assert(text.find("No control activity") != std::string::npos);
        assert(text.find("No panels registered") == std::string::npos);
        if (i == 0) {
            assert(text.find("Runtime Identity") != std::string::npos);
            assert(text.find("Current Product State") != std::string::npos);
        }
        if (i == 1) {
            assert(text.find("Tools") != std::string::npos);
            assert(text.find("Ink") != std::string::npos);
            assert(text.find("View") != std::string::npos);
            assert(text.find("History") != std::string::npos);
        }
        if (i == 5) {
            assert(text.find("Reset Canvas") != std::string::npos);
            assert(text.find("Empty Canvas") != std::string::npos);
            assert(text.find("Ink Baseline") != std::string::npos);
            assert(text.find("Unsupported") != std::string::npos);
            assert(providers.mutations == 0);
        }
        if (i == 2) {
            for (const auto* label : {"Selection", "Object", "Interaction", "Relationship"})
                assert(text.find(label) != std::string::npos);
        }
        if (i == 3) {
            for (const auto* label : {"Document",
                                      "Scene",
                                      "ARC",
                                      "Render",
                                      "Surface",
                                      "Resources",
                                      "Feature Diagnostics",
                                      "Advanced Actions"})
                assert(text.find(label) != std::string::npos);
        }
        if (i == 4) {
            for (const auto* label : {"Live Metrics",
                                      "Input / ARC",
                                      "Rendering",
                                      "Memory & Cache",
                                      "Trace",
                                      "GPU Timing",
                                      "Feature Metrics"})
                assert(text.find(label) != std::string::npos);
        }
        const auto snapshot = controller.snapshot();
        assert(snapshot.stamp.snapshotSequence == static_cast<std::uint64_t>(i + 1));
        assert(snapshot.stamp.runtimeGeneration == 11 && snapshot.stamp.documentRevision == 13);
        assert(snapshot.stamp.sequence == 40 && snapshot.stamp.frameId == 8);
        assert(snapshot.coherence == SnapshotCoherence::kCoherent);
        assert(snapshot.activity.entries.empty());
        assert(providers.reads == std::vector<int>({1, 2, 3, 4, 5, 1}));
        providers.reads.clear();
        assert(providers.mutations == 0);
    }
    DebugActivityLog activity;
    DebugControlRouter router(&providers, nullptr, nullptr, &activity);
    DebugSnapshot snapshot = controller.snapshot();
    router.beginFrame(snapshot);
    PanelRegistry registry;
    assert(registry.add({WorkspaceId::kControl, PanelSlot::kFeature, 0, nullptr, feature}));
    assert(registry.add({WorkspaceId::kControl, PanelSlot::kFeature, 1, hidden, mustNotRender}));
    DebugUiSessionState ui;
    ui.setWorkspace(WorkspaceId::kControl);
    DebugWorkbench workbench;
    startFrame();
    assert(workbench.render(snapshot, router, ui, registry));
    assert(finishFrame().find("Synthetic feature panel") != std::string::npos);
    assert(visibleRenders == 1 && providers.mutations == 1);
    assert(activity.snapshot().entries.back().state == DebugActivityState::kApplied);
    snapshot.activity = activity.snapshot();
    ui.setWorkspace(WorkspaceId::kScenarios);
    startFrame();
    assert(!workbench.render(snapshot, router, ui, registry));
    const auto empty = finishFrame();
    assert(empty.find("No panels registered") != std::string::npos);
    assert(empty.find("undo") != std::string::npos && empty.find("Applied") != std::string::npos);
    assert(!ui.controlSubmittedThisFrame() && providers.mutations == 1);

    PanelRegistry builtins;
    registerBuiltinPanels(builtins);
    SurfaceProbe surface;
    DebugActivityLog controlsActivity;
    DebugControlRouter bound(&providers, &providers, &surface, &controlsActivity);
    DebugControlRouter unbound(nullptr, nullptr, nullptr, &controlsActivity);
    snapshot.product.value.tool = {4107, 3, 4, 2}; // legacy ID cannot override semantic readback
    snapshot.product.value.selection = {true, 2, 99, 7};
    snapshot.product.value.camera = {2.0F, 10.0F, -20.0F};
    snapshot.axiom.value.camera.generation = 8;
    snapshot.platform.value.width = 900;
    snapshot.platform.value.height = 640;
    snapshot.platform.value.devicePixelRatio = 1.5F;
    snapshot.platform.value.canonicalSurfaceMode = canvas::runtime::SurfaceMode::kCpuReference;
    snapshot.activity = {};
    const auto identity = snapshot.stamp;
    const int beforeDraft = providers.mutations;
    ui.cameraDraft() = {12.0F, -7.0F, 31.0F, 42.0F, 1.25F};
    const auto view =
        renderPanel(builtins, WorkspaceId::kControl, PanelSlot::kView, snapshot, bound, ui);
    assert(view.find("camera scale 2.000 / translation 10.000, -20.000") != std::string::npos);
    assert(view.find("camera generation 8") != std::string::npos);
    assert(providers.mutations == beforeDraft && surface.calls == 0 && snapshot.stamp == identity);
    assert(!ui.controlSubmittedThisFrame());
    const auto ink = renderPanel(
        builtins, WorkspaceId::kControl, PanelSlot::kInk, snapshot, bound, ui, "Vector Solid");
    assert(ui.controlSubmittedThisFrame() && providers.mutations == beforeDraft + 1);
    assert(providers.lastProduct.action == canvas::runtime::ProductControlAction::kSetBrush);
    assert(providers.lastProduct.brushId == 1 && providers.lastProduct.brushRevision == 1);
    assert(ink.find("resolved brushId: 3 / brush revision: 4") != std::string::npos);
    assert(snapshot.product.value.tool.brushId == 3); // receipt cannot replace snapshot truth
    (void)renderPanel(
        builtins, WorkspaceId::kControl, PanelSlot::kTools, snapshot, bound, ui, "Partial Eraser");
    assert(ui.controlSubmittedThisFrame() && providers.lastProduct.eraserId == 2);
    (void)renderPanel(
        builtins, WorkspaceId::kControl, PanelSlot::kTools, snapshot, bound, ui, "Selection mode");
    assert(ui.controlSubmittedThisFrame() && !providers.lastProduct.selectionMode);
    (void)renderPanel(
        builtins, WorkspaceId::kControl, PanelSlot::kView, snapshot, bound, ui, "Pan by");
    assert(ui.controlSubmittedThisFrame() && providers.lastProduct.deltaX == 12.0F &&
           providers.lastProduct.deltaY == -7.0F);
    (void)renderPanel(
        builtins, WorkspaceId::kControl, PanelSlot::kView, snapshot, bound, ui, "Zoom At");
    assert(ui.controlSubmittedThisFrame() && providers.lastProduct.anchorX == 31.0F &&
           providers.lastProduct.anchorY == 42.0F && providers.lastProduct.scaleDelta == 1.25F);
    snapshot.product.value.selection = {};
    const int beforeDisabled = providers.mutations;
    for (const auto* label : {"Fit Selection", "Fit Primary Object"}) {
        (void)renderPanel(
            builtins, WorkspaceId::kControl, PanelSlot::kView, snapshot, bound, ui, label);
        assert(!ui.controlSubmittedThisFrame() && providers.mutations == beforeDisabled);
    }
    for (const auto* label : {"Undo (Ctrl+Z)", "Redo (Ctrl+Y)"}) {
        (void)renderPanel(
            builtins, WorkspaceId::kControl, PanelSlot::kHistory, snapshot, bound, ui, label);
        assert(!ui.controlSubmittedThisFrame() && providers.mutations == beforeDisabled);
    }
    snapshot.product.value.history = {true, true};
    for (const auto* label : {"Undo (Ctrl+Z)", "Redo (Ctrl+Y)"}) {
        (void)renderPanel(
            builtins, WorkspaceId::kControl, PanelSlot::kHistory, snapshot, bound, ui, label);
        assert(ui.controlSubmittedThisFrame());
    }
    const auto surfaceText = renderPanel(
        builtins, WorkspaceId::kRuntime, PanelSlot::kSurface, snapshot, bound, ui, "GPU Default");
    assert(ui.controlSubmittedThisFrame() && surface.calls == 1);
    assert(surface.last.expectedGeneration == 15 &&
           surface.last.mode == canvas::runtime::SurfaceMode::kGpuDefault);
    assert(surfaceText.find("resolved CPU Reference") != std::string::npos);
    bound.refreshReceipts();
    snapshot.activity = controlsActivity.snapshot();
    const auto receiptText =
        renderPanel(builtins, WorkspaceId::kRuntime, PanelSlot::kSurface, snapshot, bound, ui);
    assert(receiptText.find("Applied / generation 16") != std::string::npos);
    assert(receiptText.find("900x640 / DPR 1.50") != std::string::npos);
    (void)renderPanel(builtins,
                      WorkspaceId::kRuntime,
                      PanelSlot::kAdvancedActions,
                      snapshot,
                      bound,
                      ui,
                      "Force Full Redraw");
    assert(ui.controlSubmittedThisFrame() &&
           providers.lastAxiom.kind == canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw);
    (void)renderPanel(builtins,
                      WorkspaceId::kPerformance,
                      PanelSlot::kLiveMetrics,
                      snapshot,
                      bound,
                      ui,
                      "Reset Rolling Metrics");
    assert(ui.controlSubmittedThisFrame() &&
           providers.lastAxiom.kind ==
               canvas::runtime::AxiomDebugCommandKind::kResetRollingMetrics);
    const int beforeUnbound = providers.mutations;
    const auto unavailable = renderPanel(
        builtins, WorkspaceId::kRuntime, PanelSlot::kSurface, snapshot, unbound, ui, "GPU Default");
    assert(unavailable.find("Unsupported") != std::string::npos && !ui.controlSubmittedThisFrame());
    (void)renderPanel(builtins,
                      WorkspaceId::kRuntime,
                      PanelSlot::kAdvancedActions,
                      snapshot,
                      unbound,
                      ui,
                      "Force Full Redraw");
    assert(!ui.controlSubmittedThisFrame());
    (void)renderPanel(builtins,
                      WorkspaceId::kPerformance,
                      PanelSlot::kLiveMetrics,
                      snapshot,
                      unbound,
                      ui,
                      "Reset Rolling Metrics");
    assert(!ui.controlSubmittedThisFrame());
    (void)renderPanel(
        builtins, WorkspaceId::kControl, PanelSlot::kInk, snapshot, unbound, ui, "Vector Solid");
    assert(!ui.controlSubmittedThisFrame());
    for (const auto* label : {"Reset Canvas", "Empty Canvas", "Ink Baseline"}) {
        (void)renderPanel(
            builtins, WorkspaceId::kScenarios, PanelSlot::kSmoke, snapshot, bound, ui, label);
        assert(!ui.controlSubmittedThisFrame());
    }
    assert(providers.mutations == beforeUnbound && surface.calls == 1 &&
           snapshot.stamp == identity);
    ImGui::DestroyContext();
}
