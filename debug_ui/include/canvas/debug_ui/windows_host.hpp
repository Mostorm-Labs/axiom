#pragma once
#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/input_capture.hpp"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
  void frame();
  // Refresh the visible overlay from the platform-owned snapshot without
  // requiring the canvas HWND to receive WM_PAINT.
  void refresh() noexcept;
  // Re-anchor the owned debug window after the owner moves or resizes.  This
  // is deliberately separate from frame(): window geometry must not depend
  // on the owner receiving a paint message.
  void reposition() noexcept;
  void raise() noexcept;
  void paint(HDC dc) const;
  // Test seam for the Windows input scheduling contract.
  [[nodiscard]] std::uint64_t renderCountForTesting() const noexcept;
  [[nodiscard]] HWND overlayForTesting() const noexcept { return overlay_; }
  void setController(DebugController* controller) noexcept { controller_ = controller; }
  // The product host owns the gate so Debug and Canvas input sequences are
  // latched in one place. A private fallback keeps standalone qualification
  // hosts source-compatible.
  void setInputCaptureGate(InputCaptureGate* gate) noexcept {
    inputCapture_ = gate != nullptr ? gate : &ownedInputCapture_;
  }
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
  bool initialized_ = false;
  bool visible_ = false;
  bool placementValid_ = false;
  int placementX_ = 0;
  int placementY_ = 0;
  int placementWidth_ = 0;
  int placementHeight_ = 0;
  bool placementShown_ = false;
  DebugController* controller_ = nullptr;
  InputCaptureGate ownedInputCapture_;
  InputCaptureGate* inputCapture_ = &ownedInputCapture_;
  std::uint64_t inputSequence_ = 0;
  std::optional<DebugInputSequence> activeInputSequence_;
  bool refreshPending_ = false;
};
}  // namespace canvas::debug_ui
#endif
