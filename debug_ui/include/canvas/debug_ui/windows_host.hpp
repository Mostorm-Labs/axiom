#pragma once
#include "canvas/debug_ui/controller.hpp"
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
  // Re-anchor the owned debug window after the owner moves or resizes.  This
  // is deliberately separate from frame(): window geometry must not depend
  // on the owner receiving a paint message.
  void reposition() noexcept;
  void raise() noexcept;
  void paint(HDC dc) const;
  void setToolSelector(std::function<bool(int)> selector) { toolSelector_ = std::move(selector); }
 private:
  static LRESULT CALLBACK overlayWindowProc(HWND window, UINT message,
                                            WPARAM wParam, LPARAM lParam);
  void paintOverlay(HDC dc) const;
  void syncOverlay() noexcept;
  void renderFrame() noexcept;
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
  std::function<bool(int)> toolSelector_;
  int selectedTool_ = 4101;
};
}  // namespace canvas::debug_ui
#endif
