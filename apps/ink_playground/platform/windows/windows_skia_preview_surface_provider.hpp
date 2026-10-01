#pragma once

#include "canvas/render/skia_surface_provider.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cstdint>
#include <vector>

namespace canvas::ink_playground {

// Windows owns only the transparent overlay resource and its presentation.
// Brush geometry is rendered into the provider by the common SkiaRenderer.
class WindowsSkiaPreviewSurfaceProvider final
    : public canvas::render::SkiaSurfaceProvider {
 public:
#if defined(_WIN32)
  explicit WindowsSkiaPreviewSurfaceProvider(HWND owner) noexcept : owner_(owner) {}
#else
  explicit WindowsSkiaPreviewSurfaceProvider(void*) noexcept {}
#endif
  ~WindowsSkiaPreviewSurfaceProvider() override;

  [[nodiscard]] canvas::render::RenderTargetInfo describe() const noexcept override;
  [[nodiscard]] canvas::render::SkiaSurfaceAcquireResult acquire() noexcept override;
  void release() noexcept override;
  [[nodiscard]] canvas::render::BackendSubmissionResult lose() noexcept override;
  [[nodiscard]] canvas::render::BackendSubmissionResult present() noexcept override;
  [[nodiscard]] canvas::render::BackendSubmissionResult resize(
      std::uint32_t width, std::uint32_t height) noexcept override;
  [[nodiscard]] canvas::render::BackendSubmissionResult readbackRgba(
      std::span<std::uint8_t> destination) noexcept override;
  [[nodiscard]] std::uint64_t generation() const noexcept override {
    return raster_.generation();
  }
  [[nodiscard]] canvas::render::BackendSubmissionResult advanceGeneration() noexcept override;
  [[nodiscard]] std::uint64_t readbackCount() const noexcept override {
    return raster_.readbackCount();
  }
  [[nodiscard]] std::uint64_t cpuCopyCount() const noexcept override {
    return raster_.cpuCopyCount() + pixelCopies_;
  }
  [[nodiscard]] std::uint64_t presentCount() const noexcept override {
    return raster_.presentCount();
  }
  void setOverlayVisible(bool visible) noexcept override;
  void setOverlayOffset(int x, int y) noexcept { overlayOffsetX_ = x; overlayOffsetY_ = y; }
  [[nodiscard]] bool overlayVisible() const noexcept override { return visible_; }

 private:
  canvas::render::RasterSkiaSurfaceProvider raster_;
#if defined(_WIN32)
  HWND owner_ = nullptr;
  HWND overlay_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  HDC memoryDc_ = nullptr;
  HGDIOBJ oldBitmap_ = nullptr;
  void* bitmapBits_ = nullptr;
#endif
  std::vector<std::uint8_t> rgba_;
  std::vector<std::uint8_t> bgra_;
  std::uint64_t pixelCopies_ = 0;
  std::uint32_t width_ = 0;
  std::uint32_t height_ = 0;
  bool visible_ = false;
  int overlayOffsetX_ = 0;
  int overlayOffsetY_ = 0;
  bool lost_ = false;

#if defined(_WIN32)
  [[nodiscard]] bool ensureOverlay() noexcept;
  void destroyOverlay() noexcept;
#endif
};

}  // namespace canvas::ink_playground
