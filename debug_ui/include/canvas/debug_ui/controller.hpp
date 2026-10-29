#pragma once

#include "canvas/debug_ui/activity_log.hpp"
#include "canvas/debug_ui/control_router.hpp"
#include "canvas/debug_ui/input_capture.hpp"
#include "canvas/debug_ui/snapshot_assembler.hpp"
// Transitional source compatibility for current Windows/Web consumers.
#include "canvas/debug_ui/imgui_skia_renderer.hpp"
#include "canvas/debug_ui/workbench.hpp"

namespace canvas::debug_ui {
struct DebugControllerContext final {
  canvas::runtime::RuntimeFacade* runtime = nullptr;
  const canvas::runtime::IAxiomDiagnostics* axiom = nullptr;
  const canvas::runtime::IArcDiagnostics* arc = nullptr;
  const canvas::runtime::IPlatformDiagnostics* platformDiagnostics = nullptr;
  const canvas::runtime::ITelemetry* telemetry = nullptr;
  canvas::runtime::AxiomDebugControl* axiomDebug = nullptr;
  canvas::runtime::PlatformDebugControl* platformDebug = nullptr;
  std::function<ImageFileResult()> chooseImageFile;
  canvas::runtime::MultiSelectionPort* multiSelection = nullptr;
};

class DebugController final {
 public:
  explicit DebugController(DebugControllerContext context);
  [[nodiscard]] bool buildImGuiFrame();
  [[nodiscard]] DebugSnapshot snapshot() const;
  [[nodiscard]] DebugUiSessionState& uiState() noexcept;
  // Called by the host outside ImGui rendering: a native dialog runs a modal loop.
  void processHostRequests();
  [[nodiscard]] std::uint64_t beginImageFileRequest();
  bool completeImageFileRequest(std::uint64_t token, const ImageFileResult& file);
 private:
  DebugControllerContext context_{};
  DebugSnapshotAssembler assembler_{};
  DebugActivityLog activity_{};
  DebugControlRouter router_;
  PanelRegistry registry_{};
  DebugUiSessionState ui_{};
  DebugWorkbench workbench_{};
  DebugSnapshot latest_{};
};

[[nodiscard]] bool buildImGuiPanels(const DebugSnapshot&, int selectedTool, DebugControlRouter* router);
}  // namespace canvas::debug_ui
