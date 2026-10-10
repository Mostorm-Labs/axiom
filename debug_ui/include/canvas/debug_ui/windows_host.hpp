#pragma once
#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/activity_log.hpp"
#include "canvas/debug_ui/input_capture.hpp"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <functional>
#include <memory>
#include <vector>
struct ImGuiContext;
namespace canvas::debug_ui {
class WindowsDebugUiHost final {
 public:
  WindowsDebugUiHost();
  ~WindowsDebugUiHost();
  bool initialize(HWND window);
  void shutdown() noexcept;
  void toggle() noexcept;
  [[nodiscard]] bool visible() const noexcept { return visible_; }
  [[nodiscard]] bool handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
  void frame(const DebugSnapshot& snapshot);
  // Refresh the visible overlay from the platform-owned snapshot without
  // requiring the canvas HWND to receive WM_PAINT.
  void refresh() noexcept;
  // Re-anchor the owned debug window after the owner moves or resizes.  This
  // is deliberately separate from frame(): window geometry must not depend
  // on the owner receiving a paint message.
  void reposition() noexcept;
  void raise() noexcept;
  void paint(HDC dc) const;
  void setRuntimeFacade(canvas::runtime::RuntimeFacade* facade) noexcept { runtime_ = facade; }
  void setPlatformDebugControl(canvas::runtime::PlatformDebugControl* control) noexcept {
    platform_ = control;
  }
  // The product host owns the gate so Debug and Canvas input sequences are
  // latched in one place. A private fallback keeps standalone qualification
  // hosts source-compatible.
  void setInputCaptureGate(InputCaptureGate* gate) noexcept {
    inputCapture_ = gate != nullptr ? gate : &ownedInputCapture_;
  }
  void setDiagnostics(const canvas::runtime::IAxiomDiagnostics* diagnostics) noexcept {
    diagnostics_ = diagnostics;
  }
  void setArcDiagnostics(const canvas::runtime::IArcDiagnostics* diagnostics) noexcept {
    arcDiagnostics_ = diagnostics;
  }
  void setPlatformDiagnostics(const canvas::runtime::IPlatformDiagnostics* diagnostics) noexcept {
    platformDiagnostics_ = diagnostics;
  }
  void setTelemetry(const canvas::runtime::ITelemetry* telemetry) noexcept {
    telemetry_ = telemetry;
  }
  // The platform owner can provide a fresh immutable diagnostics snapshot
  // immediately before an overlay frame. This keeps product state changes
  // made by a panel click visible without waiting for the canvas WM_PAINT.
  void setSnapshotRefresh(std::function<DebugSnapshot()> refresh) {
    snapshotRefresh_ = std::move(refresh);
  }
  void setAxiomDebugControl(canvas::runtime::AxiomDebugControl* control) noexcept {
    axiomDebug_ = control;
  }
  void setControlRouter(DebugControlRouter* router) noexcept { router_ = router; }
 private:
  static LRESULT CALLBACK overlayWindowProc(HWND window, UINT message,
                                            WPARAM wParam, LPARAM lParam);
  void paintOverlay(HDC dc) const;
  void syncOverlay() noexcept;
  void renderFrame() noexcept;
  void releaseInputCapture() noexcept;
  [[nodiscard]] bool ensureSurface() noexcept;
  HWND window_ = nullptr;
  HWND overlay_ = nullptr;
  ImGuiContext* context_ = nullptr;
  struct Impl;
  std::unique_ptr<Impl> impl_;
  DebugSnapshot snapshot_{};
  bool initialized_ = false;
  bool visible_ = false;
  bool placementValid_ = false;
  int placementX_ = 0;
  int placementY_ = 0;
  int placementWidth_ = 0;
  int placementHeight_ = 0;
  bool placementShown_ = false;
  canvas::runtime::RuntimeFacade* runtime_ = nullptr;
  const canvas::runtime::IAxiomDiagnostics* diagnostics_ = nullptr;
  const canvas::runtime::IArcDiagnostics* arcDiagnostics_ = nullptr;
  const canvas::runtime::IPlatformDiagnostics* platformDiagnostics_ = nullptr;
  const canvas::runtime::ITelemetry* telemetry_ = nullptr;
  std::function<DebugSnapshot()> snapshotRefresh_;
  canvas::runtime::AxiomDebugControl* axiomDebug_ = nullptr;
  canvas::runtime::PlatformDebugControl* platform_ = nullptr;
  DebugControlRouter* router_ = nullptr;
  int selectedTool_ = 4101;
  InputCaptureGate ownedInputCapture_;
  InputCaptureGate* inputCapture_ = &ownedInputCapture_;
  std::uint64_t inputSequence_ = 0;
  std::optional<DebugInputSequence> activeInputSequence_;
};
}  // namespace canvas::debug_ui
#endif
