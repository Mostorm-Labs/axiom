#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/builtin_panels.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>

using namespace canvas::debug_ui;
namespace {
struct Providers final : canvas::runtime::RuntimeFacade, canvas::runtime::IAxiomDiagnostics,
    canvas::runtime::IArcDiagnostics, canvas::runtime::IPlatformDiagnostics,
    canvas::runtime::ITelemetry, canvas::runtime::AxiomDebugControl {
  mutable std::vector<int> reads;
  int mutations = 0;
  canvas::runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override {
    reads.push_back(1);
    canvas::runtime::RuntimeStateSnapshot state{};
    state.identity = {11, 12, 13, 14, 15}; state.tool.toolId = 4101; return state;
  }
  canvas::runtime::AxiomDiagnosticsSnapshot readDiagnostics() const noexcept override {
    reads.push_back(2); return {};
  }
  canvas::runtime::ArcDiagnosticsSnapshot readArcDiagnostics() const noexcept override {
    reads.push_back(3); return {};
  }
  canvas::runtime::PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept override {
    reads.push_back(4);
    canvas::runtime::PlatformDiagnosticsSnapshot state{};
    state.surfaceAvailable = true; state.canonicalSurfaceGeneration = 15; return state;
  }
  canvas::runtime::TelemetrySnapshot readTelemetry() const noexcept override {
    reads.push_back(5);
    canvas::runtime::TelemetrySnapshot state{};
    state.sequence = 40; state.canonicalFrames = 8; return state;
  }
  canvas::runtime::ProductControlReceipt submitProductControl(
      const canvas::runtime::ProductControlRequest& request) noexcept override {
    ++mutations;
    return {request.requestId, canvas::runtime::ProductControlState::kApplied, 11, std::nullopt};
  }
  canvas::runtime::AxiomDebugCommandReceipt enqueue(
      const canvas::runtime::AxiomDebugCommand& command) noexcept override {
    ++mutations; return {command.requestId, canvas::runtime::AxiomDebugCommandState::kQueued};
  }
  canvas::runtime::AxiomDebugCommandReceipt receipt(std::uint64_t id) const noexcept override {
    return {id, canvas::runtime::AxiomDebugCommandState::kApplied};
  }

};
void startFrame() {
  ImGui::NewFrame(); ImGui::Begin("Log capture"); ImGui::LogToBuffer();
}
std::string finishFrame() {
  const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
  ImGui::LogFinish();
  ImGui::End();
  ImGui::Render(); return text;
}
int visibleRenders = 0;
void feature(DebugPanelContext& context) {
  ++visibleRenders; ImGui::TextUnformatted("Synthetic feature panel");
  (void)context.controls.undo(); context.ui.markControlSubmitted();
}
bool hidden(const DebugSnapshot&) noexcept { return false; }
void mustNotRender(DebugPanelContext&) { assert(false); }
}

int main() {
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(900, 640); io.DeltaTime = 1.0f / 60.0f; io.IniFilename = nullptr;
  unsigned char* pixels = nullptr; int width = 0, height = 0;
  io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
  Providers providers;
  DebugController controller({&providers, &providers, &providers, &providers,
                              &providers, &providers, nullptr});
  assert(controller.uiState().workspace() == WorkspaceId::kDashboard);
  for (int i = 0; i < 6; ++i) {
    controller.uiState().setWorkspace(static_cast<WorkspaceId>(i));
    startFrame(); assert(!controller.buildImGuiFrame());
    const auto text = finishFrame();
    for (const auto label : kWorkspaceLabels) assert(text.find(label) != std::string::npos);
    assert(text.find("runtime 11") != std::string::npos);
    assert(text.find("document 12") != std::string::npos);
    assert(text.find("Coherent") != std::string::npos);
    assert(text.find("No control activity") != std::string::npos);
    if (i != 1) assert(text.find("No panels registered") != std::string::npos);
    else assert(text.find("Transitional legacy controls") != std::string::npos);
    const auto snapshot = controller.snapshot();
    assert(snapshot.stamp.snapshotSequence == static_cast<std::uint64_t>(i + 1));
    assert(snapshot.stamp.runtimeGeneration == 11 && snapshot.stamp.documentRevision == 13);
    assert(snapshot.stamp.sequence == 40 && snapshot.stamp.frameId == 8);
    assert(snapshot.coherence == SnapshotCoherence::kCoherent);
    assert(snapshot.activity.entries.empty());
    assert(providers.reads == std::vector<int>({1, 2, 3, 4, 5, 1}));
    providers.reads.clear(); assert(providers.mutations == 0);
  }
  DebugActivityLog activity;
  DebugControlRouter router(&providers, nullptr, nullptr, &activity);
  DebugSnapshot snapshot = controller.snapshot(); router.beginFrame(snapshot);
  PanelRegistry registry;
  assert(registry.add({WorkspaceId::kControl, PanelSlot::kFeature, 0, nullptr, feature}));
  assert(registry.add({WorkspaceId::kControl, PanelSlot::kFeature, 1, hidden, mustNotRender}));
  DebugUiSessionState ui; ui.setWorkspace(WorkspaceId::kControl);
  DebugWorkbench workbench;
  startFrame(); assert(workbench.render(snapshot, router, ui, registry));
  assert(finishFrame().find("Synthetic feature panel") != std::string::npos);
  assert(visibleRenders == 1 && providers.mutations == 1);
  assert(activity.snapshot().entries.back().state == DebugActivityState::kApplied);
  snapshot.activity = activity.snapshot(); ui.setWorkspace(WorkspaceId::kScenarios);
  startFrame(); assert(!workbench.render(snapshot, router, ui, registry));
  const auto empty = finishFrame();
  assert(empty.find("No panels registered") != std::string::npos);
  assert(empty.find("undo") != std::string::npos && empty.find("Applied") != std::string::npos);
  assert(!ui.controlSubmittedThisFrame() && providers.mutations == 1);
  ImGui::DestroyContext();
}
