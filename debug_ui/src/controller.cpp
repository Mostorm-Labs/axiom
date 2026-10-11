#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/builtin_panels.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <array>
#include <utility>

namespace canvas::debug_ui {
namespace {
bool buildLegacyCompatibilityPanel(const DebugSnapshot& snapshot, int selectedTool,
                      DebugControlRouter* router) {
    bool submittedControl = false;
    const std::array<std::pair<const char*, int>, 7> tools{{
        {"Vector", 4101}, {"Marker", 4102}, {"Chalk", 4103},
        {"Membrane", 4104}, {"Object Eraser", 4105}, {"Partial Eraser", 4106},
        {"Pan", 4107},
    }};
    const auto availabilityLabel = [](DebugAvailability state) {
        return state == DebugAvailability::kAvailable ? "Available" :
            state == DebugAvailability::kDegraded ? "Degraded" :
            state == DebugAvailability::kUnsupported ? "Unsupported" : "Unavailable";
    };
    if (ImGui::BeginTabBar("##debug_tabs")) {
      if (ImGui::BeginTabItem("Overview")) {
        ImGui::Text("Input: %s  Canvas: %s  Surface: %s",
                    availabilityLabel(snapshot.arc.availability),
                    availabilityLabel(snapshot.platform.availability),
                    availabilityLabel(snapshot.platform.availability));
        ImGui::Text("Arc Preview: %s  Telemetry: %s  Inspection: %s",
                    availabilityLabel(snapshot.arc.availability),
                    availabilityLabel(snapshot.telemetry.availability),
                    "Unsupported");
        ImGui::Text("runtime gen %llu / document gen %llu / view gen %llu",
                    static_cast<unsigned long long>(snapshot.stamp.runtimeGeneration),
                    static_cast<unsigned long long>(snapshot.stamp.documentGeneration),
                    static_cast<unsigned long long>(snapshot.stamp.viewGeneration));
        ImGui::Text("coherence: %s / snapshot #%llu",
                    snapshot.coherence == SnapshotCoherence::kCoherent ? "Coherent" :
                    snapshot.coherence == SnapshotCoherence::kMixedGeneration ? "MixedGeneration" : "Stale",
                    static_cast<unsigned long long>(snapshot.stamp.snapshotSequence));
        ImGui::Text("canonical %llu / preview %llu / presents %llu",
                    static_cast<unsigned long long>(snapshot.axiom.value.document.canonicalOperationCount),
                    static_cast<unsigned long long>(snapshot.arc.value.previewRevision),
                    static_cast<unsigned long long>(snapshot.platform.value.presentCount));
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Canvas / Selection")) {
        bool selectionMode = snapshot.product.value.selection.enabled;
        if (ImGui::Checkbox("Selection mode", &selectionMode) && router != nullptr) {
            (void)router->setSelectionMode(selectionMode);
            submittedControl = true;
        }
        ImGui::Text("selected objects: %u", snapshot.product.value.selection.selectedObjectCount);
        ImGui::Text("primary object: %llu",
                    static_cast<unsigned long long>(snapshot.product.value.selection.primaryObject));
        ImGui::Text("Click the canvas to select the frontmost eligible object.");
        ImGui::Text("EditingOverlay is per-view and transient.");
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("RuntimeFacade")) {
        ImGui::Text("Product controls are submitted through RuntimeFacade.");
        ImGui::BeginDisabled(router == nullptr || !snapshot.product.value.history.canUndo);
        if (ImGui::Button("Undo (Ctrl+Z)")) {
            (void)router->undo();
            submittedControl = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(router == nullptr || !snapshot.product.value.history.canRedo);
        if (ImGui::Button("Redo (Ctrl+Y)")) {
            (void)router->redo();
            submittedControl = true;
        }
        ImGui::EndDisabled();
        if (snapshot.activity.productControl.has_value()) {
            const auto& receipt = *snapshot.activity.productControl;
            const char* state = "rejected";
            switch (receipt.state) {
            case canvas::runtime::ProductControlState::kApplied: state = "applied"; break;
            case canvas::runtime::ProductControlState::kQueued: state = "queued"; break;
            case canvas::runtime::ProductControlState::kUnsupported: state = "unsupported"; break;
            case canvas::runtime::ProductControlState::kFailed: state = "failed"; break;
            case canvas::runtime::ProductControlState::kRejected: state = "rejected"; break;
            }
            ImGui::Text("product request %llu: %s",
                        static_cast<unsigned long long>(receipt.requestId), state);
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Brush / Eraser")) {
        for (const auto& tool : tools) {
            const bool selected = selectedTool == tool.second;
            if (ImGui::Selectable(tool.first, selected) && router != nullptr) {
                if (tool.second == 4105 || tool.second == 4106) {
                    (void)router->setEraser(tool.second == 4105 ? 1U : 2U);
                } else if (tool.second == 4107) {
                    (void)router->setTool(canvas::runtime::CanvasToolKind::kPan);
                } else {
                    (void)router->setBrush(static_cast<std::uint32_t>(tool.second - 4100),
                                           tool.second == 4103 ? 4U : 1U);
                }
                submittedControl = true;
            }
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Input")) {
        ImGui::Text("active pointers: %llu", static_cast<unsigned long long>(snapshot.arc.value.activePointerCount));
        ImGui::Text("input batches: %llu / sample %.1f Hz",
                    static_cast<unsigned long long>(snapshot.arc.value.inputBatchCount), snapshot.telemetry.value.sampleHz);
        ImGui::Text("queue age %.2f ms / frame %.2f ms",
                    snapshot.telemetry.value.queueAgeMs, snapshot.telemetry.value.frameMs);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Surface")) {
        ImGui::TextColored(ImVec4(0.96f, 0.73f, 0.27f, 1.0f), "Canonical surface mode");
    const auto requestSurface = [&](canvas::runtime::SurfaceMode mode) {
        if (router == nullptr) return;
        (void)router->setCanonicalSurfaceMode(mode);
        submittedControl = true;
    };
    if (ImGui::Button("Platform default")) requestSurface(canvas::runtime::SurfaceMode::kPlatformDefault);
    ImGui::SameLine();
    if (ImGui::Button("CPU reference")) requestSurface(canvas::runtime::SurfaceMode::kCpuReference);
    ImGui::SameLine();
    if (ImGui::Button("GPU default")) requestSurface(canvas::runtime::SurfaceMode::kGpuDefault);
    const char* mode = snapshot.platform.value.canonicalSurfaceMode == canvas::runtime::SurfaceMode::kCpuReference
        ? "CPU reference" : (snapshot.platform.value.canonicalSurfaceMode == canvas::runtime::SurfaceMode::kGpuDefault
        ? "GPU default" : "Platform default");
    ImGui::Text("resolved: %s / canonical generation %llu", mode,
                static_cast<unsigned long long>(snapshot.platform.value.canonicalSurfaceGeneration));
    if (snapshot.activity.surfaceControl.has_value()) {
        const auto& surfaceReceipt = *snapshot.activity.surfaceControl;
        const char* receipt = "failed";
        switch (surfaceReceipt.state) {
        case canvas::runtime::SurfaceControlState::kQueued: receipt = "queued"; break;
        case canvas::runtime::SurfaceControlState::kApplied: receipt = "applied"; break;
        case canvas::runtime::SurfaceControlState::kUnsupported: receipt = "unsupported"; break;
        case canvas::runtime::SurfaceControlState::kStaleGeneration: receipt = "stale-generation"; break;
        case canvas::runtime::SurfaceControlState::kQueueFull: receipt = "queue-full"; break;
        case canvas::runtime::SurfaceControlState::kExpired: receipt = "expired"; break;
        case canvas::runtime::SurfaceControlState::kUnavailable: receipt = "unavailable"; break;
        case canvas::runtime::SurfaceControlState::kFailed: receipt = "failed"; break;
        }
        ImGui::Text("surface request %llu: %s / generation %llu",
                    static_cast<unsigned long long>(surfaceReceipt.requestId), receipt,
                    static_cast<unsigned long long>(surfaceReceipt.generation));
      }
      ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Diagnostics")) {
        ImGui::Text("canonical %llu / preview %llu",
                    static_cast<unsigned long long>(snapshot.axiom.value.document.canonicalOperationCount),
                    static_cast<unsigned long long>(snapshot.arc.value.previewRevision));
        ImGui::Text("handoffs %llu / presents %llu / lost %llu",
                    static_cast<unsigned long long>(snapshot.arc.value.handoffCount),
                    static_cast<unsigned long long>(snapshot.platform.value.presentCount),
                    static_cast<unsigned long long>(snapshot.platform.value.lostCount));
        ImGui::Text("surface: %s / Arc presenter: %s",
                    snapshot.platform.value.surfaceAvailable ? "available" : "unavailable",
                    snapshot.arc.value.previewActive ? "active" : "idle");
        ImGui::Text("overlay selection: %u object(s)", snapshot.product.value.selection.selectedObjectCount);
      ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
    return submittedControl;
}


void renderLegacy(DebugPanelContext& context) {
  ImGui::TextUnformatted("Transitional legacy controls");
  if (buildLegacyCompatibilityPanel(context.snapshot,
      static_cast<int>(context.snapshot.product.value.tool.toolId), &context.controls)) {
    context.ui.markControlSubmitted();
  }
}
bool visibleAlways(const DebugSnapshot&) noexcept { return true; }
}  // namespace

void registerBuiltinPanels(PanelRegistry& registry) noexcept {
  (void)registry.add({WorkspaceId::kControl, PanelSlot::kTools, 0, visibleAlways, renderLegacy});
}

DebugController::DebugController(DebugControllerContext context)
    : context_(context), router_(context.runtime, context.axiomDebug, context.platformDebug, &activity_) {
  registerBuiltinPanels(registry_);
}

DebugSnapshot DebugController::snapshot() const { return latest_; }
DebugUiSessionState& DebugController::uiState() noexcept { return ui_; }

bool DebugController::buildImGuiFrame() {
  DebugSnapshotSources sources{context_.runtime, context_.axiom, context_.arc,
      context_.platformDiagnostics, context_.telemetry, nullptr};
  auto captured = assembler_.capture(sources);
  router_.beginFrame(captured);
  router_.refreshReceipts();
  captured.activity = activity_.snapshot();
  latest_ = std::move(captured);
  return workbench_.render(latest_, router_, ui_, registry_);
}

bool buildImGuiPanels(const DebugSnapshot& snapshot, int selectedTool, DebugControlRouter* router) {
  (void)selectedTool;
  PanelRegistry registry;
  DebugUiSessionState ui;
  // The temporary host path persists UI-local navigation in this ImGui
  // context's Workbench window, never in a process-global session/registry.
  const ImGuiID workspaceKey = ImHashStr("PX0.SelectedWorkspace");
  const ImGuiID scrollKey = ImHashStr("PX0.ActivityAutoScroll");
  if (auto* window = ImGui::FindWindowByName("Axiom Debug Workbench")) {
    ui.setWorkspace(static_cast<WorkspaceId>(window->StateStorage.GetInt(workspaceKey, 0)));
    ui.setActivityAutoScroll(window->StateStorage.GetBool(scrollKey, true));
  }
  registerBuiltinPanels(registry);
  DebugControlRouter unavailable(nullptr, nullptr, nullptr, nullptr);
  if (router == nullptr) unavailable.beginFrame(snapshot);
  DebugWorkbench workbench;
  const bool submitted = workbench.render(snapshot, router != nullptr ? *router : unavailable,
                                           ui, registry);
  if (auto* window = ImGui::FindWindowByName("Axiom Debug Workbench")) {
    window->StateStorage.SetInt(workspaceKey, static_cast<int>(ui.workspace()));
    window->StateStorage.SetBool(scrollKey, ui.activityAutoScroll());
  }
  return submitted;
}
}  // namespace canvas::debug_ui
